#ifndef OUTSHINE_RENDER_STAGES_LIGHTINGUNIFORM_H
#define OUTSHINE_RENDER_STAGES_LIGHTINGUNIFORM_H

#include "ShadowRegionUniform.h"
#include "Vec4.h"
#include <array>
#include <cstddef>

namespace outshine::Render {
inline constexpr size_t kMaxUniformLights = 16;
inline constexpr size_t kPunctualLightItemsOffset = 320;

struct alignas(16) PunctualLightUniform {
  Vec4f Tint, Place, Beam, Cone;
};

struct alignas(16) LightingUniform {
  Vec4f Count, Environment, Bounced, Up, SkyGround, ViewPosition, SkyUp, SkyToSun;
  std::array<ShadowRegionUniform, kSunShadowRegions> ShadowRegions;
  std::array<PunctualLightUniform, kMaxUniformLights> Items;
};

static_assert(sizeof(Vec4f) == 16);
static_assert(sizeof(PunctualLightUniform) == 64 && alignof(PunctualLightUniform) == 16);
static_assert(offsetof(PunctualLightUniform, Tint) == 0);
static_assert(offsetof(PunctualLightUniform, Place) == 16);
static_assert(offsetof(PunctualLightUniform, Beam) == 32);
static_assert(offsetof(PunctualLightUniform, Cone) == 3 * sizeof(Vec4f));
static_assert(sizeof(LightingUniform) == kPunctualLightItemsOffset + 64 * kMaxUniformLights);
static_assert(alignof(LightingUniform) == 16);
static_assert(offsetof(LightingUniform, Count) == 0);
static_assert(offsetof(LightingUniform, Environment) == 16);
static_assert(offsetof(LightingUniform, Bounced) == 32);
static_assert(offsetof(LightingUniform, Up) == 3 * sizeof(Vec4f));
static_assert(offsetof(LightingUniform, SkyGround) == 64);
static_assert(offsetof(LightingUniform, ViewPosition) == 5 * sizeof(Vec4f));
static_assert(offsetof(LightingUniform, SkyUp) == 6 * sizeof(Vec4f));
static_assert(offsetof(LightingUniform, SkyToSun) == 7 * sizeof(Vec4f));
static_assert(offsetof(LightingUniform, ShadowRegions) == 128);
static_assert(offsetof(LightingUniform, Items) == kPunctualLightItemsOffset);
}
#endif
