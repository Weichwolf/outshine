#include "PrismRenderer.h"
#include <array>
#include <cstdint>
#include <expected>
#include <span>

namespace outshine::Render {
namespace {
Native::Binding Storage(uint32_t set, uint32_t slot, const Native::Buffer &buffer) {
  return {.Set = set,
          .Slot = slot,
          .Value = Native::StorageBuffer{.Data = &buffer,
                                         .Range = {.Bytes = buffer.Description().Bytes}}};
}
}

Boxf PrismRenderer::Bounds() const noexcept {
  Boxf bounds;
  for (const auto &product : Products_) {
    for (const auto &record : product.Records) {
      const Boxf local{.Min = {{0, record.Shape[3], 0}},
                       .Max = {{record.Bounds[1], record.Bounds[0], record.Bounds[2]}}};
      bounds.Cover(local.Through(record.Current));
    }
  }
  return bounds;
}

Native::Result<void> PrismRenderer::Encode(const PrismProduct &product,
                                           Native::Commands &commands,
                                           const Native::GraphicsPipeline &pipeline,
                                           std::span<const Native::Binding> bindings,
                                           Native::Viewport area,
                                           Native::Scissor clip) {
  if (product.Records.empty()) { return {}; }
  if (!product.Buffers[0] || !product.Buffers[1]) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "prism drawing requires resident storage"});
  }
  constexpr uint32_t proxyVertices = 36;
  return commands.Draw({.State = {.Pipeline = &pipeline,
                                  .Bindings = bindings,
                                  .Vertices = {},
                                  .Area = area,
                                  .Clip = clip},
                        .Range = {.Count = proxyVertices,
                                  .Instances = static_cast<uint32_t>(product.Records.size())}});
}

Native::Result<void> PrismRenderer::EncodeDepth(Native::Commands &commands,
                                                const Native::GraphicsPipeline &pipeline,
                                                const PrismDepthView &view,
                                                Native::Viewport area,
                                                Native::Scissor clip) const {
  const std::array viewport{area.X, area.Y, area.Width, area.Height};
  const auto viewportBytes = std::as_bytes(std::span(viewport));
  for (const auto &product : Products_) {
    if (product.Records.empty()) { continue; }
    const auto viewBytes = std::as_bytes(std::span(&view, 1));
    const std::array bindings{
        Storage(0, 0, *product.Buffers[1]),
        Native::Binding{.Set = 1, .Slot = 0, .Value = Native::Uniform{.Bytes = viewBytes}},
        Native::Binding{.Set = 3, .Slot = 0, .Value = Native::Uniform{.Bytes = viewBytes}},
        Native::Binding{.Set = 3, .Slot = 1, .Value = Native::Uniform{.Bytes = viewportBytes}},
        Storage(4, 0, *product.Buffers[0]),
        Storage(4, 1, *product.Buffers[1])};
    if (auto drawn = Encode(product, commands, pipeline, bindings, area, clip); !drawn) {
      return drawn;
    }
  }
  return {};
}
}
