#include "LightVisibilityStage.h"
#include "SpirvFile.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Render {
namespace {
struct ShadowInput {
  static constexpr size_t kStreams = 3;
  std::array<Native::VertexStream, kStreams> Streams{};
  std::array<Native::VertexAttribute, kStreams> Attributes{};
  uint32_t Count = 0;

  explicit ShadowInput(uint32_t uv) : Count(uv + 1) {
    for (uint32_t slot = 0; slot < Count; ++slot) {
      Streams[slot] = {.Slot = slot,
                       .Stride = (slot == 0 ? 3u : 2u) * static_cast<uint32_t>(sizeof(float))};
      Attributes[slot] = {.Location = slot,
                          .Stream = slot,
                          .Format = slot == 0 ? Native::VertexFormat::Float3
                                              : Native::VertexFormat::Float2};
    }
  }
};

struct ShaderFiles {
  std::string_view Vertex, Fragment;
};

Native::Result<std::unique_ptr<Native::GraphicsPipeline>>
Pipeline(Native::Device &device, ShaderFiles files, uint32_t uv, Native::Cull culling) {
  const auto vertex = ReadSpirv(files.Vertex);
  if (!vertex) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = vertex.error()});
  }
  const auto fragment = ReadSpirv(files.Fragment);
  if (!fragment) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = fragment.error()});
  }
  const ShadowInput input(uv);
  return device.CreateGraphicsPipeline(
      {.Vertex = {.Spirv = *vertex},
       .Fragment = Native::ShaderCode{.Spirv = *fragment},
       .Streams = std::span(input.Streams).first(input.Count),
       .Attributes = std::span(input.Attributes).first(input.Count),
       .Colours = {},
       .Depth = Native::DepthTarget{},
       .Culling = culling,
       .Label = files.Vertex});
}
}

Native::GraphicsPipeline *LightVisibilityStage::MaskedPipeline(VertexLayout layout) const {
  const auto uv = static_cast<size_t>(CarriesUv(layout)) + static_cast<size_t>(CarriesUv1(layout));
  return Masked_[uv + (CarriesColour(layout) ? kMaskedUvLayouts : 0u)].get();
}

Native::Result<std::unique_ptr<Native::GraphicsPipeline>>
LightVisibilityStage::ConfigureDepthOnly(Native::Device &device) {
  return Pipeline(
      device,
      {.Vertex = "build/shaders/subjectDepth.vert.spv", .Fragment = "build/shaders/depth.frag.spv"},
      0,
      Native::Cull::Front);
}

Native::Result<LightVisibilityStage::MaskedPipelines>
LightVisibilityStage::ConfigureMasked(Native::Device &device) {
  MaskedPipelines pipelines;
  constexpr auto uvLayouts = static_cast<uint32_t>(kMaskedUvLayouts);
  for (uint32_t at = 0; at < pipelines.size(); ++at) {
    const auto uv = at % uvLayouts;
    const auto tinted = at / uvLayouts;
    auto made =
        Pipeline(device,
                 {.Vertex = std::format("build/shaders/subjectShadow-{}{}.vert.spv", uv, tinted),
                  .Fragment = std::format("build/shaders/subjectShadow-{}.frag.spv", uv)},
                 uv,
                 Native::Cull::None);
    if (!made) { return std::unexpected(made.error()); }
    pipelines[at] = std::move(*made);
  }
  return pipelines;
}
}
