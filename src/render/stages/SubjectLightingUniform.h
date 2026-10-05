#ifndef OUTSHINE_RENDER_STAGES_SUBJECTLIGHTINGUNIFORM_H
#define OUTSHINE_RENDER_STAGES_SUBJECTLIGHTINGUNIFORM_H

#include "SubjectTypes.h"
#include "ShadowRegionUniform.h"
#include "math/Vec4.h"
#include <array>
#include <cstddef>

namespace outshine::Render {
struct alignas(16) SubjectLightUniform {
  Vec4f Tint, Place, Beam, Cone;
};

struct alignas(16) SubjectLightingUniform {
  Vec4f Count, Environment, Bounced, Up, SkyGround, ViewPosition, SkyUp, SkyToSun;
  std::array<ShadowRegionUniform, kSunShadowRegions> ShadowRegions;
  std::array<SubjectLightUniform, kMaxSubjectLights> Items;
};

static_assert(sizeof(Vec4f) == 16);
static_assert(sizeof(SubjectLightUniform) == 64 && alignof(SubjectLightUniform) == 16);
static_assert(offsetof(SubjectLightUniform, Tint) == 0);
static_assert(offsetof(SubjectLightUniform, Place) == 16);
static_assert(offsetof(SubjectLightUniform, Beam) == 32);
static_assert(offsetof(SubjectLightUniform, Cone) == 3 * sizeof(Vec4f));
static_assert(sizeof(SubjectLightingUniform) == 256 + 64 * kMaxSubjectLights);
static_assert(alignof(SubjectLightingUniform) == 16);
static_assert(offsetof(SubjectLightingUniform, Count) == 0);
static_assert(offsetof(SubjectLightingUniform, Environment) == 16);
static_assert(offsetof(SubjectLightingUniform, Bounced) == 32);
static_assert(offsetof(SubjectLightingUniform, Up) == 3 * sizeof(Vec4f));
static_assert(offsetof(SubjectLightingUniform, SkyGround) == 64);
static_assert(offsetof(SubjectLightingUniform, ViewPosition) == 5 * sizeof(Vec4f));
static_assert(offsetof(SubjectLightingUniform, SkyUp) == 6 * sizeof(Vec4f));
static_assert(offsetof(SubjectLightingUniform, SkyToSun) == 7 * sizeof(Vec4f));
static_assert(offsetof(SubjectLightingUniform, ShadowRegions) == 128);
static_assert(offsetof(SubjectLightingUniform, Items) == 256);
}
#endif
