#include "SubjectDraw.h"
#include "SurfaceBindings.h"
#include "VertexArms.h"
#include "SceneTargets.h"
#include "SpirvFile.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>
#include <expected>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Render {
namespace {
struct VertexShape {
  std::array<Native::VertexStream, kMostVertexRuns> Buffers{};
  std::array<Native::VertexAttribute, kMostVertexRuns> Attributes{};
  uint32_t Count = 0;
};

Native::VertexFormat FormatOf(uint32_t floats) {
  switch (floats) {
    case 2: return Native::VertexFormat::Float2;
    case 4: return Native::VertexFormat::Float4;
    default: return Native::VertexFormat::Float3;
  }
}

VertexShape ShapeOf(VertexLayout layout, bool writesVelocity) {
  std::array<VertexRun, kMostVertexRuns> runs{};
  const auto count = RunsOf(layout, writesVelocity, runs);
  VertexShape shape;
  for (uint32_t at = 0; at < count; ++at) {
    const bool packedNormal = CarriesNormal(layout) && runs[at].Location == 3;
    shape.Buffers[at] = {.Slot = at,
                         .Stride = (packedNormal ? 1u : runs[at].Floats) *
                                   static_cast<uint32_t>(sizeof(float))};
    shape.Attributes[at] = {.Location = runs[at].Location,
                            .Stream = at,
                            .Format = packedNormal ? Native::VertexFormat::Uint32
                                                   : FormatOf(runs[at].Floats)};
    ++shape.Count;
  }
  return shape;
}

std::array<Native::ColourTarget, kMaxColourAttachments> Targets(const Gpu &gpu,
                                                                const SurfaceOutputs &options,
                                                                std::span<const Resource> colours,
                                                                bool blends) {
  std::array<Native::ColourTarget, kMaxColourAttachments> targets{};
  targets[0].Texels = gpu.HdrFormat;
  targets[0].Blend = blends;
  if (blends) {
    targets[0].Rgb = {.Source = Native::BlendFactor::SourceAlpha,
                      .Destination = Native::BlendFactor::OneMinusSourceAlpha};
    targets[0].Alpha = {.Source = Native::BlendFactor::One,
                        .Destination = Native::BlendFactor::OneMinusSourceAlpha};
  }
  if (options.WritesVelocity) {
    const auto at = std::ranges::find(colours, Resource::SceneVelocity) - colours.begin();
    targets[at] = VelocityTarget(!blends);
  }
  if (options.NormalIndex >= 0) {
    targets[options.NormalIndex].Texels = Native::Format::Rgba16Float;
  }
  if (options.IdentityIndex >= 0) {
    targets[options.IdentityIndex].Texels = Native::Format::Rgba32Float;
  }
  if (options.BaseIndex >= 0) {
    targets[options.BaseIndex].Texels = Native::Format::Rgba16Float;
    targets[options.MetalRoughIndex].Texels = Native::Format::Rg16Float;
  }
  return targets;
}
}

const char *
SubjectDraw::FragmentEntry(SurfaceDomain domain, SurfaceKind kind, VertexLayout layout) {
  return FragmentArmNamed(domain, ShadingArmOf(layout), CarriesUv(layout), kind);
}

const char *SubjectDraw::VertexEntry(VertexLayout layout) {
  return VertexArmName(layout);
}

namespace {

std::string VertexShaderVariant(VertexLayout layout) {
  int uvSets = 0;
  if (CarriesUv1(layout)) {
    uvSets = 2;
  } else if (CarriesUv(layout)) {
    uvSets = 1;
  }
  const int tinted = CarriesColour(layout) ? 1 : 0;
  if (!CarriesNormal(layout)) { return std::format("flat-{}{}", uvSets, tinted); }
  return std::format("lit-{}{}{}", uvSets, tinted, CarriesTangent(layout) ? 1 : 0);
}

std::string FragmentShaderVariant(SurfaceDomain domain, SurfaceKind kind, VertexLayout layout) {
  if (domain == SurfaceDomain::Ground) { return "groundLit"; }
  const bool transmits = kind == SurfaceKind::ThinTransmissive || kind == SurfaceKind::Refractive;
  const int fragmentKind = transmits ? 3 : static_cast<int>(kind);
  if (!CarriesNormal(layout)) {
    return std::format("flat-{}{}", fragmentKind, CarriesUv(layout) && !transmits ? 1 : 0);
  }
  return std::format(
      "lit-{}{}{}", fragmentKind, CarriesUv(layout) ? 1 : 0, CarriesTangent(layout) ? 1 : 0);
}

}

class SubjectDraw::PipelineShaders {
public:
  [[nodiscard]] Native::Result<std::span<const uint32_t>> Get(const std::string &path) {
    for (const auto &entry : Entries_) {
      if (entry.Path == path) { return entry.Words; }
    }
    auto words = ReadSpirv(path);
    if (!words) {
      return std::unexpected(
          Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = words.error()});
    }
    Entries_.push_back({.Path = path, .Words = std::move(*words)});
    return Entries_.back().Words;
  }

private:
  struct Entry {
    std::string Path;
    std::vector<uint32_t> Words;
  };

  std::vector<Entry> Entries_;
};

Native::Result<void> SubjectDraw::Configure(const Gpu &gpu) {
  SubjectPipelineBinding candidate;
  const auto configured = Configure(candidate, gpu, nullptr, nullptr, false);
  if (!configured) { return configured; }
  OwnedBinding_ = std::move(candidate);
  Binding_ = &OwnedBinding_;
  Ground_.UsePipelines(OwnedBinding_.Ground);
  return {};
}

Native::Result<void> SubjectDraw::AttachPipelines(SubjectPipelineBinding &binding, const Gpu &gpu) {
  Bound().UploadsFrom(gpu.Uploads);
  Ground_.UploadsFrom(gpu.Uploads);
  const auto attached = Bound().StandsOn(gpu.Device);
  if (!attached) { return attached; }
  Binding_ = &binding;
  Ground_.UsePipelines(binding.Ground);
  return {};
}

Native::Result<void> SubjectDraw::PrepareGroundStorage(const Gpu &gpu) {
  Ground_.UploadsFrom(gpu.Uploads);
  if (gpu.Device == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "ground preparation requires a device"});
  }
  return Ground_.AttachPipelines(Binding().Ground, *gpu.Device);
}

Native::Result<void> SubjectDraw::Configure(SubjectPipelineBinding &binding,
                                            const Gpu &gpu,
                                            Native::Texture *behind,
                                            Native::Sampler *exact,
                                            bool separateTransmission) {
  if (gpu.Device == nullptr || gpu.SceneColours.Size() > kMaxColourAttachments ||
      (behind != nullptr && exact == nullptr)) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = std::format("subject pipeline requires a device and valid targets "
                                             "(device={}, colours={}, behind={}, sampler={})",
                                             gpu.Device != nullptr,
                                             gpu.SceneColours.Size(),
                                             behind != nullptr,
                                             exact != nullptr)});
  }
  std::vector<Resource> colours(gpu.SceneColours.begin(), gpu.SceneColours.end());
  const SourceOptions options{
      .WritesVelocity = ColourAttachment(colours, Resource::SceneVelocity) >= 0,
      .NormalIndex = ColourAttachment(colours, Resource::SceneShadingNormal),
      .IdentityIndex = ColourAttachment(colours, Resource::SceneSurfaceIdentity),
      .BaseIndex = ColourAttachment(colours, Resource::SceneSurfaceBase),
      .MetalRoughIndex = ColourAttachment(colours, Resource::SceneSurfaceMetalRough)};
  if ((options.BaseIndex >= 0 || options.MetalRoughIndex >= 0) &&
      (options.WritesVelocity || options.NormalIndex != 1 || options.IdentityIndex != 2 ||
       options.BaseIndex != 3 || options.MetalRoughIndex != 4)) {
    return std::unexpected(Native::Error{
        .Kind = Native::ErrorKind::Invalid,
        .Message =
            "surface capture requires HDR, normal, identity, base and metallic-roughness targets"});
  }
  std::array<std::unique_ptr<Native::GraphicsPipeline>, kPipelines> pipelines;
  PipelineShaders shaders;
  uint32_t built = 0;
  const bool glass = behind != nullptr;
  for (const auto kind : {SurfaceKind::Opaque,
                          SurfaceKind::Masked,
                          SurfaceKind::Blended,
                          SurfaceKind::ThinTransmissive,
                          SurfaceKind::Refractive}) {
    if (!glass && (kind == SurfaceKind::ThinTransmissive || kind == SurfaceKind::Refractive)) {
      continue;
    }
    const auto configured = ConfigureKind(gpu, options, colours, kind, shaders, pipelines, built);
    if (!configured) { return configured; }
  }
  Bound().UploadsFrom(gpu.Uploads);
  Ground_.UploadsFrom(gpu.Uploads);
  const auto attached = Bound().StandsOn(gpu.Device);
  if (!attached) { return attached; }
  if (!glass) {
    const auto targets = Targets(gpu, options, colours, false);
    const auto configured = Ground_.Configure(
        binding.Ground, *gpu.Device, options, std::span(targets).first(colours.size()));
    Ground_.UsePipelines(Binding().Ground);
    if (!configured) { return configured; }
  }
  binding.Device = gpu.Device;
  binding.Colours = std::move(colours);
  binding.WritesVelocity = options.WritesVelocity;
  binding.CapturesMaterial = options.BaseIndex >= 0;
  binding.Pipelines = std::move(pipelines);
  binding.Built = built;
  binding.Behind = behind;
  binding.BehindSampler = exact;
  binding.GlassDrawnElsewhere = separateTransmission;
  return {};
}

long SubjectDraw::ColourAttachment(std::span<const Resource> colours, Resource which) {
  const auto at = std::ranges::find(colours, which);
  return at == colours.end() ? -1 : static_cast<long>(at - colours.begin());
}

Native::Result<void> SubjectDraw::ConfigureKind(
    const Gpu &gpu,
    const SourceOptions &options,
    std::span<const Resource> colours,
    SurfaceKind kind,
    PipelineShaders &shaders,
    std::array<std::unique_ptr<Native::GraphicsPipeline>, kPipelines> &pipelines,
    uint32_t &built) {
  const auto targets = Targets(gpu, options, colours, kind == SurfaceKind::Blended);
  for (const auto domain : {SurfaceDomain::Subject, SurfaceDomain::Ground}) {
    for (const auto &row : kVertexLayouts) {
      if (!DomainPresents(domain, ShadingArmOf(row.Layout), CarriesUv(row.Layout), kind)) {
        continue;
      }
      const auto configured = ConfigureVariant(kind,
                                               domain,
                                               row.Layout,
                                               options,
                                               std::span(targets).first(colours.size()),
                                               *gpu.Device,
                                               shaders,
                                               pipelines,
                                               built);
      if (!configured) { return configured; }
    }
  }
  return {};
}

Native::Result<void> SubjectDraw::ConfigureVariant(
    SurfaceKind kind,
    SurfaceDomain domain,
    VertexLayout layout,
    const SourceOptions &options,
    std::span<const Native::ColourTarget> targets,
    Native::Device &device,
    PipelineShaders &shaders,
    std::array<std::unique_ptr<Native::GraphicsPipeline>, kPipelines> &pipelines,
    uint32_t &built) {
  const auto shape = ShapeOf(layout, options.WritesVelocity);
  auto vertex = shaders.Get(options.VertexPath(VertexShaderVariant(layout)));
  if (!vertex) { return std::unexpected(vertex.error()); }
  auto fragment = shaders.Get(options.FragmentPath(FragmentShaderVariant(domain, kind, layout)));
  if (!fragment) { return std::unexpected(fragment.error()); }
  for (const bool cullsBack : {false, true}) {
    auto made = device.CreateGraphicsPipeline(
        {.Vertex = {.Spirv = *vertex},
         .Fragment = Native::ShaderCode{.Spirv = *fragment},
         .Streams = std::span(shape.Buffers).first(shape.Count),
         .Attributes = std::span(shape.Attributes).first(shape.Count),
         .Colours = targets,
         .Depth = Native::DepthTarget{.Write = kind != SurfaceKind::Blended},
         .Culling = cullsBack ? Native::Cull::Back : Native::Cull::None,
         .ClampDepth = true,
         .Label = FragmentEntry(domain, kind, layout)});
    if (!made) { return std::unexpected(made.error()); }
    pipelines[PipelineAt(domain, layout, kind, cullsBack)] = std::move(*made);
    ++built;
  }
  return {};
}

size_t SubjectDraw::PipelineAt(SurfaceDomain domain,
                               VertexLayout layout,
                               SurfaceKind kind,
                               bool cullsBack) {
  const size_t placed =
      static_cast<size_t>(domain) * kVertexLayoutCount + static_cast<size_t>(layout);
  return (placed * 2u + (cullsBack ? 1u : 0u)) * kSurfaceKinds + static_cast<size_t>(kind);
}
}
