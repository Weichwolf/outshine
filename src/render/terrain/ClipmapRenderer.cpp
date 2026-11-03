#include "ClipmapRenderer.h"
#include "ClipmapTopology.h"
#include "SpirvFile.h"
#include "SurfaceOutputs.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string_view>
#include <utility>

namespace outshine::Render {
namespace {
struct ShaderPaths {
  std::string_view Vertex;
  std::string_view Fragment;
};

Native::Result<std::unique_ptr<Native::GraphicsPipeline>>
Pipeline(Native::Device &device,
         ShaderPaths paths,
         std::span<const Native::ColourTarget> targets,
         Native::Cull culling) {
  auto vert = ReadShaderSpirv(paths.Vertex);
  if (!vert) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = vert.error()});
  }
  auto frag = ReadShaderSpirv(paths.Fragment);
  if (!frag) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = frag.error()});
  }
  constexpr std::array streams{Native::VertexStream{.Slot = 0, .Stride = sizeof(ClipmapVertex)}};
  constexpr std::array attributes{
      Native::VertexAttribute{.Location = 0, .Stream = 0, .Format = Native::VertexFormat::Float2}};
  return device.CreateGraphicsPipeline({.Vertex = {.Spirv = *vert},
                                        .Fragment = Native::ShaderCode{.Spirv = *frag},
                                        .Streams = streams,
                                        .Attributes = attributes,
                                        .Colours = targets,
                                        .Depth = Native::DepthTarget{},
                                        .Culling = culling,
                                        .ClampDepth = true,
                                        .Label = paths.Vertex});
}
}

Native::Result<void> ClipmapRenderer::Configure(Native::Device &device,
                                                const SurfaceOutputs &outputs,
                                                std::span<const Native::ColourTarget> targets,
                                                uint32_t halfCells,
                                                size_t geometryBytesMost) {
  ClipmapRenderer candidate;
  const auto built = candidate.BuildMeshes(device, halfCells, geometryBytesMost);
  if (!built) { return built; }
  auto surface = Pipeline(
      device,
      {.Vertex = outputs.VertexPath("clipmap"), .Fragment = outputs.FragmentPath("clipmapLit")},
      targets,
      Native::Cull::Back);
  if (!surface) { return std::unexpected(surface.error()); }
  auto depth = Pipeline(device,
                        {.Vertex = "clipmapDepth.vert.spv", .Fragment = "depth.frag.spv"},
                        {},
                        Native::Cull::None);
  if (!depth) { return std::unexpected(depth.error()); }
  auto nearest =
      device.CreateSampler({.Min = Native::Filter::Nearest, .Mag = Native::Filter::Nearest});
  if (!nearest) { return std::unexpected(nearest.error()); }
  auto linear = device.CreateSampler({.Min = Native::Filter::Linear,
                                      .Mag = Native::Filter::Linear,
                                      .U = Native::Address::Repeat,
                                      .V = Native::Address::Repeat});
  if (!linear) { return std::unexpected(linear.error()); }
  candidate.Surface_ = std::move(*surface);
  candidate.Depth_ = std::move(*depth);
  candidate.Nearest_ = std::move(*nearest);
  candidate.Linear_ = std::move(*linear);
  *this = std::move(candidate);
  return {};
}
}
