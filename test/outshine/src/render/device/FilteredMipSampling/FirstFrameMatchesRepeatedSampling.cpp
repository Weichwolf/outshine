#include <SDL3/SDL.h>
#include <import/GltfImporter.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstddef>
#include <memory>
#include <limits>
#include <print>
#include <utility>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <span>
#include <vector>

#include "Check.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOffline.h"
#include "GpuPlacement.h"
#include "Lens.h"
#include "PreparedRoot.h"
#include "Shape.h"
#include "SubjectMaterialPacking.h"
#include "SubjectResidency.h"
#include "Surfacing.h"
#include "TexelChain.h"

namespace {
using namespace outshine::Render;
using namespace outshine::Test;
namespace N = outshine::Render::Native;
using outshine::Test::Gpu::Require;

constexpr uint32_t kSourceWidth = 2048;
constexpr uint32_t kTargetWidth = 1280;
constexpr uint32_t kTargetHeight = 720;
constexpr uint32_t kChannels = 4;
constexpr uint32_t kHalfBytes = 2;
constexpr float kTexelsPerPixel = 2.6f;
constexpr float kFractionalLodUvScaleX =
    kTexelsPerPixel * static_cast<float>(kTargetWidth) / static_cast<float>(kSourceWidth);
constexpr float kFractionalLodUvScaleY =
    kTexelsPerPixel * static_cast<float>(kTargetHeight) / static_cast<float>(kSourceWidth);

#ifdef OUTSHINE_GPU_VALIDATION
constexpr bool kValidation = true;
#else
constexpr bool kValidation = false;
#endif

struct Fixture {
  std::unique_ptr<N::Device> Device;
  std::shared_ptr<N::Texture> Source;
  std::unique_ptr<N::Texture> Target;
  std::unique_ptr<N::Texture> Depth;
  std::shared_ptr<N::Sampler> Sampler;
  std::unique_ptr<N::GraphicsPipeline> Pipeline;
  std::unique_ptr<N::Buffer> Vertices;
  std::unique_ptr<N::Buffer> Uvs;
  std::unique_ptr<N::Buffer> Emitted;
  std::unique_ptr<N::Buffer> Placements;
  std::unique_ptr<N::Buffer> Indices;
  std::unique_ptr<N::Buffer> Indirect;
  std::vector<float> VertexUniform;
  std::vector<float> FragmentUniform;
  uint32_t IndexCount = 3;
  bool IndirectDraw = true;
  bool Derivatives = false;
  bool DescriptorTable = false;

  Fixture() : Device(Require(N::CreateDevice({.Validation = kValidation}))) {}
};

std::unique_ptr<N::Commands> Begin(Fixture &fixture) {
  return Require(
      Gpu::RetryOffline([&] { return fixture.Device->Begin(); }, std::chrono::seconds(10)));
}

struct TextureUploadCounts {
  size_t Submissions = 0;
  size_t StagingAllocations = 0;
  size_t Bytes = 0;
};

enum class MipMode { None, Nearest, Linear };
enum class RasterShape { Fullscreen, SmallTriangle };

uint32_t Levels(MipMode mode);

bool UploadBuffer(Fixture &fixture,
                  std::span<const std::byte> bytes,
                  N::BufferUse usage,
                  std::unique_ptr<N::Buffer> &destination) {
  destination = Require(fixture.Device->CreateBuffer(
      {.Bytes = bytes.size(), .Use = usage | N::BufferUse::CopyDestination}));
  auto staging = Require(fixture.Device->CreateBuffer(
      {.Bytes = bytes.size(), .Use = N::BufferUse::CopySource, .Placement = N::Memory::Upload}));
  const auto mapped = Require(fixture.Device->MapWrite(*staging, {.Bytes = bytes.size()}));
  std::memcpy(mapped.data(), bytes.data(), bytes.size());
  Require(fixture.Device->Unmap(*staging));
  auto commands = Begin(fixture);
  Require(commands->Copy(*staging, *destination, {.Bytes = bytes.size()}));
  Require(fixture.Device->Submit(std::move(commands)));
  return true;
}

bool CreateTargets(Fixture &fixture) {
  if (fixture.Target) { return static_cast<bool>(fixture.Depth); }
  fixture.Target = Require(fixture.Device->CreateTexture(
      {.Width = kTargetWidth,
       .Height = kTargetHeight,
       .Texels = N::Format::Rgba16Float,
       .Use = N::TextureUse::ColourAttachment | N::TextureUse::CopySource}));
  fixture.Depth = Require(fixture.Device->CreateTexture({.Width = kTargetWidth,
                                                         .Height = kTargetHeight,
                                                         .Texels = N::Format::Depth32Float,
                                                         .Use = N::TextureUse::DepthAttachment}));
  return true;
}

std::vector<float> Pixels(uint32_t width) {
  std::vector<float> pixels(static_cast<size_t>(width) * width * kChannels);
  for (uint32_t y = 0; y < width; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      const size_t at = (static_cast<size_t>(y) * width + x) * kChannels;
      const float checker = ((x ^ y) & 1u) == 0 ? 0.0f : 1.0f;
      pixels[at] = checker;
      pixels[at + 1] = 1.0f - checker;
      pixels[at + 2] = static_cast<float>(x) / static_cast<float>(width - 1u);
      pixels[at + 3] = 1.0f;
    }
  }
  return pixels;
}

std::vector<uint8_t> Encode(std::span<const float> linear) {
  std::vector<uint8_t> encoded(linear.size());
  for (size_t at = 0; at < linear.size(); ++at) {
    encoded[at] =
        static_cast<uint8_t>(std::lround(std::clamp(linear[at], 0.0f, 1.0f) *
                                         static_cast<float>(std::numeric_limits<uint8_t>::max())));
  }
  return encoded;
}

std::vector<std::vector<uint8_t>> Chain(MipMode mode) {
  std::vector<float> level = Pixels(kSourceWidth);
  std::vector<std::vector<uint8_t>> chain;
  uint32_t width = kSourceWidth;
  for (uint32_t mip = 0; mip < Levels(mode); ++mip) {
    chain.push_back(Encode(level));
    if (width > 1) {
      std::vector<float> next;
      HalveInPlace(level, {.WidthPx = width, .HeightPx = width}, next, TexelKind::Value);
      level.swap(next);
      width /= 2;
    }
  }
  return chain;
}

uint32_t Levels(MipMode mode) {
  if (mode == MipMode::None) { return 1; }
  uint32_t levels = 1;
  for (uint32_t width = kSourceWidth; width > 1; width /= 2) { ++levels; }
  return levels;
}

void UploadLevels(Fixture &fixture,
                  std::span<const std::vector<uint8_t>> chain,
                  uint32_t firstMip) {
  size_t bytes = 0;
  for (const auto &level : chain) { bytes += level.size(); }
  auto staging = Require(fixture.Device->CreateBuffer(
      {.Bytes = bytes, .Use = N::BufferUse::CopySource, .Placement = N::Memory::Upload}));
  const auto mapped = Require(fixture.Device->MapWrite(*staging, {.Bytes = bytes}));
  size_t offset = 0;
  for (const auto &level : chain) {
    std::memcpy(mapped.data() + offset, level.data(), level.size());
    offset += level.size();
  }
  Require(fixture.Device->Unmap(*staging));
  auto commands = Begin(fixture);
  offset = 0;
  for (uint32_t at = 0; at < chain.size(); ++at) {
    const uint32_t mip = firstMip + at;
    const uint32_t width = kSourceWidth >> mip;
    Require(commands->Copy(*staging,
                           *fixture.Source,
                           {.BufferOffset = offset,
                            .RowBytes = static_cast<uint64_t>(width) * kChannels,
                            .Region = {.Mip = mip, .Width = width, .Height = width}}));
    offset += chain[at].size();
  }
  Require(fixture.Device->Submit(std::move(commands)));
}

bool UploadChain(Fixture &fixture, MipMode mode, bool separateSubmits, bool srgb) {
  const auto chain = Chain(mode);
  fixture.Source = Require(fixture.Device->CreateTexture(
      {.Width = kSourceWidth,
       .Height = kSourceWidth,
       .Mips = Levels(mode),
       .Texels = srgb ? N::Format::Rgba8Srgb : N::Format::Rgba8Unorm,
       .Use = N::TextureUse::Sampled | N::TextureUse::CopyDestination}));
  if (separateSubmits) {
    for (uint32_t mip = 0; mip < chain.size(); ++mip) {
      UploadLevels(fixture, std::span(chain).subspan(mip, 1), mip);
    }
  } else {
    UploadLevels(fixture, chain, 0);
  }
  return true;
}

bool Configure(Fixture &fixture,
               MipMode mode,
               bool separateSubmits,
               bool descriptorTable,
               bool derivatives,
               bool srgb,
               RasterShape rasterShape) {
  if (!UploadChain(fixture, mode, separateSubmits, srgb)) { return false; }
  fixture.DescriptorTable = descriptorTable;
  fixture.Derivatives = derivatives;
  fixture.Sampler = Require(fixture.Device->CreateSampler(
      {.Mip = mode == MipMode::Nearest ? N::Filter::Nearest : N::Filter::Linear,
       .U = N::Address::Repeat,
       .V = N::Address::Repeat,
       .MaxLod = static_cast<float>(Levels(mode) - 1u)}));
  const char *vertexPath = derivatives ? "build/shaders/filteredMipDerivativeSample.vert.spv"
                                       : "build/shaders/fullscreen.vert.spv";
  const char *fragmentPath = "build/shaders/filteredMipSample.frag.spv";
  if (descriptorTable) {
    fragmentPath = "build/shaders/filteredMipDescriptorSample.frag.spv";
  } else if (derivatives) {
    fragmentPath = "build/shaders/filteredMipDerivativeSample.frag.spv";
  }
  const auto vertex = Gpu::Spirv(vertexPath);
  const auto fragment = Gpu::Spirv(fragmentPath);
  const std::array target{
      N::ColourTarget{.Texels = N::Format::Rgba16Float, .Rgb = {}, .Alpha = {}}};
  const std::array streams{N::VertexStream{.Slot = 0, .Stride = 4u * sizeof(float)},
                           N::VertexStream{.Slot = 1, .Stride = 2u * sizeof(float)}};
  const std::array attributes{
      N::VertexAttribute{.Location = 0, .Stream = 0, .Format = N::VertexFormat::Float4},
      N::VertexAttribute{.Location = 1, .Stream = 1, .Format = N::VertexFormat::Float2}};
  fixture.Pipeline = Require(fixture.Device->CreateGraphicsPipeline(
      {.Vertex = {.Spirv = vertex},
       .Fragment = N::ShaderCode{.Spirv = fragment},
       .Streams = derivatives ? std::span(streams) : std::span<const N::VertexStream>{},
       .Attributes = derivatives ? std::span(attributes) : std::span<const N::VertexAttribute>{},
       .Colours = target,
       .Depth = N::DepthTarget{.Test = derivatives, .Write = derivatives},
       .Culling = N::Cull::None,
       .Label = {}}));
  if (!derivatives) { return true; }
  constexpr std::array<float, 12> fullscreenPositions = {
      -1, -1, 0.5f, 1, 3, -1, 0.5f, 1, -1, 3, 0.5f, 1};
  constexpr std::array<float, 6> fullscreenUvs = {
      0, 0, 2 * kFractionalLodUvScaleX, 0, 0, 2 * kFractionalLodUvScaleY};
  constexpr float kSmallUvSpanX = 0.25f * kFractionalLodUvScaleX;
  constexpr float kSmallUvSpanY = 0.25f * kFractionalLodUvScaleY;
  constexpr std::array<float, 12> smallPositions = {
      -0.25f, -0.25f, 0.5f, 1, 0.5f, -0.5f, 1, 2, -0.375f, 0.375f, 0.75f, 1.5f};
  constexpr std::array<float, 6> smallUvs = {0, 0, kSmallUvSpanX, 0, 0, kSmallUvSpanY};
  constexpr std::array<uint32_t, 3> indices = {0, 1, 2};
  constexpr N::IndexedIndirectArguments indirect{.IndexCount = 3, .InstanceCount = 1};
  const std::array<float, 12> &positions =
      rasterShape == RasterShape::Fullscreen ? fullscreenPositions : smallPositions;
  const std::array<float, 6> &uvs =
      rasterShape == RasterShape::Fullscreen ? fullscreenUvs : smallUvs;
  const auto bytesOf = [](const auto &values) { return std::as_bytes(std::span(values)); };
  return UploadBuffer(fixture, bytesOf(positions), N::BufferUse::Vertex, fixture.Vertices) &&
         UploadBuffer(fixture, bytesOf(uvs), N::BufferUse::Vertex, fixture.Uvs) &&
         UploadBuffer(fixture, bytesOf(indices), N::BufferUse::Index, fixture.Indices) &&
         UploadBuffer(fixture,
                      std::as_bytes(std::span(&indirect, 1)),
                      N::BufferUse::Indirect,
                      fixture.Indirect);
}

std::vector<uint8_t> Draw(Fixture &fixture) {
  if (!CreateTargets(fixture)) { return {}; }
  constexpr size_t bytes =
      static_cast<size_t>(kTargetWidth) * kTargetHeight * kChannels * kHalfBytes;
  auto download = Require(fixture.Device->CreateBuffer(
      {.Bytes = bytes, .Use = N::BufferUse::CopyDestination, .Placement = N::Memory::Readback}));
  auto commands = Begin(fixture);
  const std::array colour{
      N::ColourAttachment{.Image = {.Image = fixture.Target.get()}, .Begin = N::Load::Clear}};
  Require(commands->BeginRenderPass(
      {.Colours = colour,
       .Depth = N::DepthAttachment{.Image = {.Image = fixture.Depth.get()},
                                   .Begin = N::Load::Clear,
                                   .End = N::Store::Discard}}));
  std::vector<N::Binding> bindings;
  if (!fixture.VertexUniform.empty()) {
    bindings.push_back(
        {.Set = 1,
         .Slot = 0,
         .Value = N::Uniform{.Bytes = std::as_bytes(std::span(fixture.VertexUniform))}});
  }
  if (!fixture.FragmentUniform.empty()) {
    bindings.push_back(
        {.Set = 3,
         .Slot = 0,
         .Value = N::Uniform{.Bytes = std::as_bytes(std::span(fixture.FragmentUniform))}});
  }
  if (fixture.Placements) { bindings.push_back(Gpu::Storage(0, 0, *fixture.Placements)); }
  for (uint32_t slot = 0; slot < (fixture.DescriptorTable ? 8u : 1u); ++slot) {
    bindings.push_back(
        {.Set = 2,
         .Slot = slot,
         .Value = N::SampledTexture{
             .Image = {.Image = fixture.Source.get(), .Mips = fixture.Source->Description().Mips},
             .Sampling = fixture.Sampler.get()}});
  }
  const std::array streams{
      N::VertexBuffer{
          .Stream = 0,
          .Data = fixture.Vertices.get(),
          .Range = {.Bytes = fixture.Vertices ? fixture.Vertices->Description().Bytes : 0}},
      N::VertexBuffer{.Stream = 1,
                      .Data = fixture.Uvs.get(),
                      .Range = {.Bytes = fixture.Uvs ? fixture.Uvs->Description().Bytes : 0}},
      N::VertexBuffer{
          .Stream = 2,
          .Data = fixture.Emitted.get(),
          .Range = {.Bytes = fixture.Emitted ? fixture.Emitted->Description().Bytes : 0}}};
  const N::DrawState state{.Pipeline = fixture.Pipeline.get(),
                           .Bindings = bindings,
                           .Vertices = fixture.Derivatives
                                           ? std::span(streams).first(fixture.Emitted ? 3u : 2u)
                                           : std::span<const N::VertexBuffer>{},
                           .Area = {.Width = kTargetWidth, .Height = kTargetHeight},
                           .Clip = {.Width = kTargetWidth, .Height = kTargetHeight}};
  if (fixture.Derivatives) {
    const N::IndexBuffer indices{.Data = fixture.Indices.get(),
                                 .Range = {.Bytes = fixture.Indices->Description().Bytes}};
    if (fixture.IndirectDraw) {
      Require(
          commands->DrawIndexedIndirect({.State = state,
                                         .Indices = indices,
                                         .Range = {.Data = fixture.Indirect.get(), .Count = 1}}));
    } else {
      Require(commands->DrawIndexed(
          {.State = state, .Indices = indices, .Range = {.Count = fixture.IndexCount}}));
    }
  } else {
    Require(commands->Draw({.State = state, .Range = {.Count = 3}}));
  }
  Require(commands->EndRenderPass());
  Require(commands->Copy(*fixture.Target,
                         *download,
                         {.RowBytes = static_cast<uint64_t>(kTargetWidth) * kChannels * kHalfBytes,
                          .Region = {.Width = kTargetWidth, .Height = kTargetHeight}}));
  const auto submitted = Require(fixture.Device->Submit(std::move(commands)));
  CHECK(Require(fixture.Device->Wait(submitted, std::chrono::seconds(10))) ==
            N::CompletionStatus::Ready,
        "the filtered frame readback completes within its bounded wait");
  const auto mapped = Require(fixture.Device->MapRead(*download, {.Bytes = bytes}));
  std::vector<uint8_t> pixels(bytes);
  std::memcpy(pixels.data(), mapped.data(), bytes);
  Require(fixture.Device->Unmap(*download));
  return pixels;
}

bool ConfigureImported(Fixture &fixture, TextureUploadCounts &counts) {
  if (!CreateTargets(fixture)) { return false; }
  const std::string path = PreparedRoot() + "/test-khronos-glTF-ABeautifulGame/scene.gltf";
  outshine::GltfImporter imported;
  const auto loaded = imported.load(path);
  if (!loaded) {
    SDL_SetError("%s", loaded.error().c_str());
    return false;
  }
  const outshine::Geometry &native = imported.geometry();
  ShapeStore store;
  const auto shaped = PrepareShape(native, store);
  if (!shaped || shaped->Parts.empty()) {
    SDL_SetError("could not pack the imported chess part");
    return false;
  }
  const ShapePart &part = shaped->Parts.front();
  SubjectMaterial material;
  material.Row = native.surfaceAt(native.materialOf(0));
  std::string error;
  if (!ResolveNativeTextures(native, std::span(&material, 1), error)) {
    SDL_SetError("%s", error.c_str());
    return false;
  }
  SubjectResidency residency;
  if (!residency.StandsOn(fixture.Device.get())) { return false; }
  auto bound =
      residency.Upload(material.Colour, SubjectResidency::Transfer::Srgb, TexelKind::Value);
  if (!bound) {
    SDL_SetError("%s", bound.error().Message.c_str());
    return false;
  }
  counts.Submissions = residency.TakeUploadAttempts();
  counts.StagingAllocations = residency.TakeStagingAllocationAttempts();
  counts.Bytes = residency.TakeUploadBytes();
  fixture.Source = std::move(bound->Image);
  fixture.Sampler = std::move(bound->Sample);

  const auto vertex = Gpu::Spirv("build/shaders/flat-10-0.vert.spv");
  const auto fragment = Gpu::Spirv("build/shaders/flat-01-000.frag.spv");
  const std::array target{
      N::ColourTarget{.Texels = N::Format::Rgba16Float, .Rgb = {}, .Alpha = {}}};
  const std::array streams{N::VertexStream{.Slot = 0, .Stride = 3u * sizeof(float)},
                           N::VertexStream{.Slot = 1, .Stride = 2u * sizeof(float)},
                           N::VertexStream{.Slot = 2, .Stride = 3u * sizeof(float)}};
  const std::array attributes{
      N::VertexAttribute{.Location = 0, .Stream = 0, .Format = N::VertexFormat::Float3},
      N::VertexAttribute{.Location = 1, .Stream = 1, .Format = N::VertexFormat::Float2},
      N::VertexAttribute{.Location = 2, .Stream = 2, .Format = N::VertexFormat::Float3}};
  fixture.Pipeline =
      Require(fixture.Device->CreateGraphicsPipeline({.Vertex = {.Spirv = vertex},
                                                      .Fragment = N::ShaderCode{.Spirv = fragment},
                                                      .Streams = streams,
                                                      .Attributes = attributes,
                                                      .Colours = target,
                                                      .Depth = N::DepthTarget{},
                                                      .Label = {}}));

  const auto indices = shaped->Indices.subspan(part.FirstIndex, part.IndexCount);
  const N::IndexedIndirectArguments indirect{.IndexCount = static_cast<uint32_t>(indices.size()),
                                             .InstanceCount = 1};
  std::vector<float> emitted(part.VertexCount * 3u);
  for (size_t vertexIndex = 0; vertexIndex < part.VertexCount; ++vertexIndex) {
    for (size_t channel = 0; channel < 3; ++channel) {
      emitted[vertexIndex * 3u + channel] = material.Row.BaseColour[channel];
    }
  }
  constexpr std::array placements = {
      GpuPlacement{.Current = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
                   .Previous = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}}};
  if (!UploadBuffer(
          fixture, std::as_bytes(part.PositionsM), N::BufferUse::Vertex, fixture.Vertices) ||
      !UploadBuffer(fixture, std::as_bytes(part.Uv), N::BufferUse::Vertex, fixture.Uvs) ||
      !UploadBuffer(
          fixture, std::as_bytes(std::span(emitted)), N::BufferUse::Vertex, fixture.Emitted) ||
      !UploadBuffer(fixture,
                    std::as_bytes(std::span(placements)),
                    N::BufferUse::Storage,
                    fixture.Placements) ||
      !UploadBuffer(fixture, std::as_bytes(indices), N::BufferUse::Index, fixture.Indices) ||
      !UploadBuffer(fixture,
                    std::as_bytes(std::span(&indirect, 1)),
                    N::BufferUse::Indirect,
                    fixture.Indirect)) {
    return false;
  }
  fixture.Derivatives = true;
  fixture.IndexCount = static_cast<uint32_t>(indices.size());
  fixture.IndirectDraw = part.ClusterCount > 0;

  constexpr outshine::Vec3 eye = {{2.781138576118416, 1.3202289998916963, 1.9473741958039332}};
  constexpr outshine::Vec3 aim = {{0, 0.08449789705936794, 0}};
  auto viewpoint = Viewpoint::LookAt({.EyeM = eye, .AimM = aim}, 0.0);
  if (!viewpoint) { return false; }
  constexpr double kImportedYfovRad = 0.47108996144172666;
  constexpr double kImportedNearM = 3.107125103623776;
  constexpr double kImportedFarM = 4.118946968135317;
  viewpoint->YfovRad = kImportedYfovRad;
  viewpoint->ZNearM = kImportedNearM;
  viewpoint->ZFarM = kImportedFarM;
  const auto lens = Lens::From(*viewpoint, kTargetWidth, kTargetHeight);
  if (!lens) { return false; }
  const outshine::Mat4f viewProjection = lens->ViewProjection(*viewpoint);
  fixture.VertexUniform.assign(viewProjection.begin(), viewProjection.end());
  fixture.VertexUniform.insert(
      fixture.VertexUniform.end(), viewProjection.begin(), viewProjection.end());
  fixture.VertexUniform.insert(fixture.VertexUniform.end(), 16u, 0.0f);
  fixture.VertexUniform.push_back(-static_cast<float>(eye[0]));
  fixture.VertexUniform.push_back(-static_cast<float>(eye[1]));
  fixture.VertexUniform.push_back(-static_cast<float>(eye[2]));
  fixture.VertexUniform.push_back(0.0f);
  fixture.VertexUniform.push_back(-static_cast<float>(eye[0]));
  fixture.VertexUniform.push_back(-static_cast<float>(eye[1]));
  fixture.VertexUniform.push_back(-static_cast<float>(eye[2]));
  fixture.VertexUniform.push_back(0.0f);
  const PackedSubjectMaterial packed = PackSubjectMaterial(material, 1.0f);
  fixture.FragmentUniform.assign(packed.begin(), packed.end());
  return true;
}

std::vector<uint8_t> Render(MipMode mode,
                            bool separateSubmits,
                            bool descriptorTable,
                            bool derivatives,
                            bool srgb,
                            RasterShape rasterShape) {
  Fixture fixture;
  if (!fixture.Device ||
      !Configure(fixture, mode, separateSubmits, descriptorTable, derivatives, srgb, rasterShape)) {
    return {};
  }
  return Draw(fixture);
}

void CheckMode(MipMode mode,
               bool separateSubmits,
               bool descriptorTable,
               bool derivatives,
               bool srgb,
               const char *name,
               RasterShape rasterShape = RasterShape::Fullscreen) {
  Fixture fixture;
  CHECK(fixture.Device &&
            Configure(
                fixture, mode, separateSubmits, descriptorTable, derivatives, srgb, rasterShape),
        "the native GPU fixture configures");
  if (!fixture.Device || !fixture.Pipeline) { return; }
  const auto first = Draw(fixture);
  const auto repeated = Draw(fixture);
  const auto fresh = Render(mode, separateSubmits, descriptorTable, derivatives, srgb, rasterShape);
  CHECK(!first.empty(), "the native GPU fixture renders its first filtered frame");
  CHECK(std::ranges::any_of(first, [](uint8_t byte) { return byte != 0; }),
        "the filtered primitive shades the target instead of repeating only the clear value");
  CHECK(first == repeated, name);
  CHECK(first == fresh, "a fresh native device has the same first filtered frame");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video initializes for raw filtered sampling");
  CheckMode(MipMode::None, false, false, false, false, "base-only linear sampling repeats exactly");
  CheckMode(
      MipMode::Nearest, false, false, false, false, "batched nearest-mip sampling repeats exactly");
  CheckMode(
      MipMode::Linear, false, false, false, false, "batched linear-mip sampling repeats exactly");
  CheckMode(MipMode::Nearest,
            true,
            false,
            false,
            false,
            "per-level nearest-mip sampling repeats exactly");
  CheckMode(
      MipMode::Linear, true, false, false, false, "per-level linear-mip sampling repeats exactly");
  CheckMode(
      MipMode::Linear, true, true, false, false, "eight material sampler bindings repeat exactly");
  CheckMode(
      MipMode::Linear, true, false, true, false, "interpolated UV derivatives repeat exactly");
  CheckMode(
      MipMode::Linear, true, false, true, true, "sRGB interpolated UV derivatives repeat exactly");
  CheckMode(MipMode::Linear,
            true,
            false,
            true,
            true,
            "small sRGB primitive edges repeat exactly",
            RasterShape::SmallTriangle);
  Fixture imported;
  TextureUploadCounts importedUpload;
  const bool configured = imported.Device && ConfigureImported(imported, importedUpload);
  if (!configured) { std::println(stderr, "imported fixture: {}", SDL_GetError()); }
  CHECK(configured, "the exact imported chess draw configures through native GPU");
  if (imported.Device && imported.Pipeline) {
    CHECK(importedUpload.Submissions == 1,
          "a complete imported mip chain submits through one copy command");
    CHECK(importedUpload.StagingAllocations == 1,
          "a complete imported mip chain owns one staging allocation");
    CHECK(importedUpload.Bytes > 0, "the imported mip chain records its transferred bytes");
    CHECK(imported.IndirectDraw, "the imported part uses the engine's clustered indirect draw");
    const auto first = Draw(imported);
    const auto repeated = Draw(imported);
    CHECK(!first.empty(), "the exact imported chess draw produces pixels");
    CHECK(std::ranges::any_of(first, [](uint8_t byte) { return byte != 0; }),
          "the exact imported chess draw reaches the target");
    CHECK(first == repeated, "the exact imported chess draw repeats every half channel exactly");
  }
  SDL_Quit();
  return Report();
}
