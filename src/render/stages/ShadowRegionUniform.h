#ifndef OUTSHINE_RENDER_STAGES_SHADOWREGIONUNIFORM_H
#define OUTSHINE_RENDER_STAGES_SHADOWREGIONUNIFORM_H

#include "math/Vec4.h"
#include <cstddef>
#include <cstdint>

namespace outshine::Render {
inline constexpr size_t kSunShadowRegions = 4;
inline constexpr int kShadowTilePx = 2048;
inline constexpr int kShadowAtlasPx = 2 * kShadowTilePx;
inline constexpr size_t kShadowRegionUniformBytes = 48;

struct alignas(16) ShadowRegionUniform {
  Vec4f Transform;
  Vec4f Depth;
  Vec4f Atlas;
};

static_assert(sizeof(ShadowRegionUniform) == kShadowRegionUniformBytes &&
              alignof(ShadowRegionUniform) == 16);
static_assert(offsetof(ShadowRegionUniform, Transform) == 0);
static_assert(offsetof(ShadowRegionUniform, Depth) == 16);
static_assert(offsetof(ShadowRegionUniform, Atlas) == 32);
}
#endif
