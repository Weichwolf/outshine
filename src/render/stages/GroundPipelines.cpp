#include "GroundLattice.h"
#include "SurfaceOutputs.h"
#include "SpirvFile.h"
#include <array>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string_view>
#include <utility>

namespace outshine::Render {
namespace {
struct LatticeInput {
  std::array<Native::VertexStream, 2> Streams{
      {{.Slot = 0, .Stride = GroundLattice::kGridFloats * sizeof(float)},
       {.Slot = 1,
        .Stride = kGroundInstanceFloats * sizeof(float),
        .Rate = Native::VertexRate::Instance}}};
  std::array<Native::VertexAttribute, 8> Attributes{};

  constexpr LatticeInput() {
    Attributes[0] = {.Location = 0, .Stream = 0, .Format = Native::VertexFormat::Float3};
    for (uint32_t at = 1; at < Attributes.size(); ++at) {
      Attributes[at] = {.Location = at,
                        .Stream = 1,
                        .Offset = (at - 1u) * 4u * static_cast<uint32_t>(sizeof(float)),
                        .Format = Native::VertexFormat::Float4};
    }
  }
};

struct ShaderPaths {
  std::string_view Vertex, Fragment;
};

Native::Result<std::unique_ptr<Native::GraphicsPipeline>>
Pipeline(Native::Device &device,
         ShaderPaths paths,
         std::span<const Native::ColourTarget> targets,
         Native::Cull culling) {
  auto vertex = ReadSpirv(paths.Vertex);
  if (!vertex) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = vertex.error()});
  }
  auto fragment = ReadSpirv(paths.Fragment);
  if (!fragment) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = fragment.error()});
  }
  constexpr LatticeInput input;
  return device.CreateGraphicsPipeline({.Vertex = {.Spirv = *vertex},
                                        .Fragment = Native::ShaderCode{.Spirv = *fragment},
                                        .Streams = input.Streams,
                                        .Attributes = input.Attributes,
                                        .Colours = targets,
                                        .Depth = Native::DepthTarget{},
                                        .Culling = culling,
                                        .Label = paths.Vertex});
}
}

Native::Result<void>
GroundPipelineBinding::ConfigureLit(Native::Device &device,
                                    const SurfaceOutputs &outputs,
                                    std::span<const Native::ColourTarget> targets) {
  auto made = Pipeline(device,
                       {.Vertex = outputs.VertexPath("groundLattice"),
                        .Fragment = outputs.FragmentPath("groundLit")},
                       targets,
                       Native::Cull::Back);
  if (!made) { return std::unexpected(made.error()); }
  Lit_ = std::move(*made);
  return {};
}

Native::Result<void> GroundPipelineBinding::ConfigureDepth(Native::Device &device) {
  auto made = Pipeline(device,
                       {.Vertex = "build/shaders/groundLatticeDepth.vert.spv",
                        .Fragment = "build/shaders/depth.frag.spv"},
                       {},
                       Native::Cull::Front);
  if (!made) { return std::unexpected(made.error()); }
  Depth_ = std::move(*made);
  return {};
}

Native::Result<void> GroundLattice::Configure(GroundPipelineBinding &pipelines,
                                              Native::Device &device,
                                              const SurfaceOutputs &outputs,
                                              std::span<const Native::ColourTarget> targets) {
  GroundPipelineBinding candidate;
  const auto depth = ConfigureDepth(candidate, device);
  if (!depth) { return depth; }
  const auto lit = candidate.ConfigureLit(device, outputs, targets);
  if (!lit) { return lit; }
  const auto attached = AttachPipelines(pipelines, device);
  if (!attached) { return attached; }
  pipelines = std::move(candidate);
  return {};
}

Native::Result<void> GroundLattice::AttachPipelines(GroundPipelineBinding &pipelines,
                                                    Native::Device &device) {
  if (Device_ != nullptr && Device_ != &device) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "ground resources belong to another device"});
  }
  Device_ = &device;
  if (!Grid_) {
    const auto grid = BuildGrid({}, Grid_);
    if (!grid) { return grid; }
  }
  if (!UniformGrid_) {
    const auto grid = BuildGrid({}, UniformGrid_);
    if (!grid) { return grid; }
  }
  Pipelines_ = &pipelines;
  return {};
}

Native::Result<void> GroundLattice::ConfigureDepth(GroundPipelineBinding &pipelines,
                                                   Native::Device &device) {
  return pipelines.ConfigureDepth(device);
}

Native::Result<void> GroundLattice::Configure(Native::Device &device,
                                              const SurfaceOutputs &outputs,
                                              std::span<const Native::ColourTarget> targets) {
  GroundPipelineBinding candidate;
  auto *const previous = Pipelines_;
  const auto configured = Configure(candidate, device, outputs, targets);
  if (!configured) {
    Pipelines_ = previous;
    return configured;
  }
  OwnedPipelines_ = std::move(candidate);
  Pipelines_ = &OwnedPipelines_;
  return {};
}
}
