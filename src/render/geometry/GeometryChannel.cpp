#include "GeometryChannel.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace outshine::Render {
void GeometryChannel::Write(const void *carrying, float *into, uint32_t floats) {
  const GeometryChannel &what = *static_cast<const GeometryChannel *>(carrying);
  uint32_t at = 0;
  for (const RenderGeometryPart &one : what.From->Parts) {
    const std::span<const float> held = one.*what.Channel;
    const auto want = static_cast<uint32_t>(one.VertexCount * what.Wide);
    if (at + want > floats) { break; }
    if (held.size() >= one.VertexCount * what.Wide) {
      std::memcpy(into + at, held.data(), static_cast<size_t>(want) * sizeof(float));
    } else {
      std::memset(into + at, 0, static_cast<size_t>(want) * sizeof(float));
    }
    at += want;
  }
  if (at < floats) { std::memset(into + at, 0, static_cast<size_t>(floats - at) * sizeof(float)); }
}

}
