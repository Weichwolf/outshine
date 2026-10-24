#ifndef OUTSHINE_RENDER_SCENE_PRISMINSTANCE_H
#define OUTSHINE_RENDER_SCENE_PRISMINSTANCE_H

#include "Mat4.h"
#include <cstdint>

namespace outshine::Render {
struct PrismInstance {
  uint32_t Patch = 0;
  Mat4f Current;
  Mat4f Previous;
};
}
#endif
