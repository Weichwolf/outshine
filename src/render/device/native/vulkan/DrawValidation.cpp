#ifdef __linux__
#include "RenderState.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <expected>
#include <limits>
#include <cstdint>
#include <span>

namespace outshine::Render::Native::Vulkan {

Result<void> CheckDrawArea(const Context &context, const RenderState &pass, const DrawState &draw) {
  const auto &area = draw.Area;
  const auto &clip = draw.Clip;
  const auto &limits = context.Properties.limits;
  const std::array floats{area.X, area.Y, area.Width, area.Height, area.MinDepth, area.MaxDepth};
  if (!std::ranges::all_of(floats, [](float value) { return std::isfinite(value); }) ||
      area.Width <= 0 || area.Height <= 0 ||
      area.Width > static_cast<float>(limits.maxViewportDimensions[0]) ||
      area.Height > static_cast<float>(limits.maxViewportDimensions[1]) ||
      area.X < limits.viewportBoundsRange[0] || area.Y < limits.viewportBoundsRange[0] ||
      area.X + area.Width > limits.viewportBoundsRange[1] ||
      area.Y + area.Height > limits.viewportBoundsRange[1] || area.MinDepth < 0 ||
      area.MinDepth > 1 || area.MaxDepth < 0 || area.MaxDepth > 1 || clip.X > pass.Width ||
      clip.Y > pass.Height || clip.Width > pass.Width - clip.X ||
      clip.Height > pass.Height - clip.Y ||
      clip.X > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) ||
      clip.Y > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "invalid graphics viewport or scissor"});
  }
  return {};
}

Result<void> CheckVertexRange(const GraphicsState &graphics,
                              std::span<const BoundVertex> vertices,
                              VertexRange range,
                              bool indexed) {
  if (uint64_t(range.First) + range.Count > uint64_t(std::numeric_limits<uint32_t>::max()) + 1 ||
      uint64_t(range.FirstInstance) + range.Instances >
          uint64_t(std::numeric_limits<uint32_t>::max()) + 1) {
    return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                 .Message = "draw range overflows vertex or instance index"});
  }
  for (const auto &vertex : vertices) {
    const bool instance = vertex.Stream.Rate == VertexRate::Instance;
    if (!instance && indexed) { continue; }
    const uint64_t count = instance ? range.Instances : range.Count;
    const uint64_t first = instance ? range.FirstInstance : range.First;
    uint32_t tail = 0;
    for (const auto &attribute : graphics.Attributes) {
      if (attribute.Stream == vertex.Stream.Slot) {
        tail = std::max(tail, attribute.Offset + AttributeBytes(attribute.Format));
      }
    }
    if (count > 0 && ((first + count - 1) * vertex.Stream.Stride + tail > vertex.Range.Bytes)) {
      return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                   .Message = "draw exceeds bound vertex or instance range"});
    }
  }
  return {};
}

}
#endif
