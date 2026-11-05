#include "GeometryOcclusionPass.h"
#include "GeometryRenderer.h"
#include "SpirvFile.h"
#include <array>
#include <cstdint>
#include <expected>

namespace outshine::Render {
Native::Result<void> GeometryOcclusionPass::Configure(Native::Device &device,
                                                      Native::Texture *depth,
                                                      Native::Sampler *sampler) {
  const auto configured = Pyramid_.Configure(depth, sampler);
  if (!configured) { return configured; }
  if (Pipelines_[0] && Pipelines_[1]) { return {}; }
  const auto vertex = ReadShaderSpirv("geometryOcclusion.vert.spv");
  const auto fragment = ReadShaderSpirv("depth.frag.spv");
  if (!vertex || !fragment) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = !vertex ? vertex.error() : fragment.error()});
  }
  const std::array streams{Native::VertexStream{.Slot = 0, .Stride = 3 * sizeof(float)}};
  const std::array attributes{
      Native::VertexAttribute{.Location = 0, .Stream = 0, .Format = Native::VertexFormat::Float3}};
  for (size_t at = 0; at < Pipelines_.size(); ++at) {
    auto made =
        device.CreateGraphicsPipeline({.Vertex = {.Spirv = *vertex},
                                       .Fragment = Native::ShaderCode{.Spirv = *fragment},
                                       .Streams = streams,
                                       .Attributes = attributes,
                                       .Colours = {},
                                       .Depth = Native::DepthTarget{},
                                       .Culling = at == 0 ? Native::Cull::None : Native::Cull::Back,
                                       .Label = "geometryOcclusion"});
    if (!made) { return std::unexpected(made.error()); }
    Pipelines_[at] = std::move(*made);
  }
  return {};
}

Native::Result<void> GeometryOcclusionPass::EncodeDepth(GeometryRenderer &geometry,
                                                        const FrameContext &ctx,
                                                        const PassRecording &into,
                                                        bool hasSelection) {
  return geometry.EncodeOccluders(ctx, into, Pipelines_, hasSelection);
}
}
