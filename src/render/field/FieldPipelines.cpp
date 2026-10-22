#include "FieldPipelines.h"
#include "FieldSurface.h"
#include "SpirvFile.h"
#include <array>
#include <expected>
#include <string_view>
#include <vector>

namespace outshine::Render {
namespace {
std::vector<Native::VertexAttribute> Attributes(bool depth) {
  using F = Native::VertexFormat;
  constexpr std::array attributes{
      Native::VertexAttribute{
          .Location = 0, .Offset = offsetof(FieldVertex, Position), .Format = F::Float3},
      Native::VertexAttribute{
          .Location = 1, .Offset = offsetof(FieldVertex, Normal), .Format = F::Float3},
      Native::VertexAttribute{
          .Location = 2, .Offset = offsetof(FieldVertex, Kind), .Format = F::Uint32},
      Native::VertexAttribute{
          .Location = 3, .Offset = offsetof(FieldVertex, FixedCell), .Format = F::Uint32},
      Native::VertexAttribute{
          .Location = 4, .Offset = offsetof(FieldVertex, PatchCell), .Format = F::Uint32},
      Native::VertexAttribute{.Location = 5,
                              .Offset = offsetof(FieldVertex, PatchCell) + sizeof(uint32_t),
                              .Format = F::Uint32},
      Native::VertexAttribute{
          .Location = 6, .Offset = offsetof(FieldVertex, LevelReserved), .Format = F::Float2}};
  std::vector<Native::VertexAttribute> result;
  for (const auto &attribute : attributes) {
    if (!depth || attribute.Location != 1) { result.push_back(attribute); }
  }
  return result;
}

Native::Result<std::unique_ptr<Native::GraphicsPipeline>>
Configure(Native::Device &device,
          std::string_view vertexPath,
          std::string_view fragmentPath,
          std::span<const Native::ColourTarget> targets,
          bool depth,
          Native::Cull culling) {
  auto vertex = ReadSpirv(vertexPath);
  if (!vertex) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = vertex.error()});
  }
  auto fragment = ReadSpirv(fragmentPath);
  if (!fragment) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = fragment.error()});
  }
  const std::array streams{Native::VertexStream{.Stride = sizeof(FieldVertex)}};
  const auto attributes = Attributes(depth);
  return device.CreateGraphicsPipeline({.Vertex = {.Spirv = *vertex},
                                        .Fragment = Native::ShaderCode{.Spirv = *fragment},
                                        .Streams = streams,
                                        .Attributes = attributes,
                                        .Colours = targets,
                                        .Depth = Native::DepthTarget{},
                                        .Culling = culling,
                                        .ClampDepth = true,
                                        .Label = depth ? "FieldDepth" : "FieldLit"});
}
}

Native::Result<std::unique_ptr<Native::GraphicsPipeline>>
ConfigureFieldColour(Native::Device &device,
                     const SurfaceOutputs &outputs,
                     std::span<const Native::ColourTarget> targets) {
  return Configure(device,
                   outputs.VertexPath("field"),
                   outputs.FragmentPath("field"),
                   targets,
                   false,
                   Native::Cull::Back);
}

Native::Result<std::unique_ptr<Native::GraphicsPipeline>>
ConfigureFieldDepth(Native::Device &device, Native::Cull culling) {
  return Configure(device,
                   "build/shaders/fieldDepth.vert.spv",
                   "build/shaders/fieldDepth.frag.spv",
                   {},
                   true,
                   culling);
}
}
