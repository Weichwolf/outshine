#ifndef OUTSHINE_RENDER_DRAW_GEOMETRYPASSES_H
#define OUTSHINE_RENDER_DRAW_GEOMETRYPASSES_H

#include <cstdint>

namespace outshine::Render {
enum class GeometryPasses : uint8_t { MainAndShadow, Main, Shadow };

[[nodiscard]] constexpr bool ValidGeometryPasses(GeometryPasses passes) noexcept {
  return passes == GeometryPasses::MainAndShadow || passes == GeometryPasses::Main ||
         passes == GeometryPasses::Shadow;
}
}
#endif
