#ifndef OUTSHINE_RENDER_SCENE_PRISMINSTANCE_H
#define OUTSHINE_RENDER_SCENE_PRISMINSTANCE_H

#include "Mat4.h"
#include "Vec4.h"
#include <cstdint>
#include <optional>

namespace outshine::Render {
struct PrismInstance {
  uint32_t Patch = 0;
  Mat4f Current;
  Mat4f Previous;
  std::optional<Vec4f> TopCoating = std::nullopt;
};
}
#endif
