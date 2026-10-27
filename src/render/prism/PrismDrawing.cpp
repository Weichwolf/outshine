#include "PrismDrawing.h"
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

Native::Result<void> EncodePrism(const PrismProduct &product,
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
  constexpr uint32_t proxyVertices = 6;
  return commands.Draw({.State = {.Pipeline = &pipeline,
                                  .Bindings = bindings,
                                  .Vertices = {},
                                  .Area = area,
                                  .Clip = clip},
                        .Range = {.Count = proxyVertices,
                                  .Instances = static_cast<uint32_t>(product.Records.size())}});
}

Native::Result<void> EncodePrismDepth(const PrismResidency &residency,
                                      Native::Commands &commands,
                                      const Native::GraphicsPipeline &pipeline,
                                      const PrismDepthView &view,
                                      Native::Viewport area,
                                      Native::Scissor clip) {
  const std::array viewport{area.X, area.Y, area.Width, area.Height};
  const auto viewportBytes = std::as_bytes(std::span(viewport));
  for (const auto &product : residency.Products()) {
    if (product.Records.empty()) { continue; }
    const auto viewBytes = std::as_bytes(std::span(&view, 1));
    const std::array bindings{
        Storage(0, 0, *product.Buffers[1]),
        Native::Binding{.Set = 1, .Slot = 0, .Value = Native::Uniform{.Bytes = viewBytes}},
        Native::Binding{.Set = 3, .Slot = 0, .Value = Native::Uniform{.Bytes = viewBytes}},
        Native::Binding{.Set = 3, .Slot = 1, .Value = Native::Uniform{.Bytes = viewportBytes}},
        Storage(4, 0, *product.Buffers[0]),
        Storage(4, 1, *product.Buffers[1])};
    if (auto drawn = EncodePrism(product, commands, pipeline, bindings, area, clip); !drawn) {
      return drawn;
    }
  }
  return {};
}
}
