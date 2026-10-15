#ifndef OUTSHINE_RENDER_STAGES_CLOUDVIEWUNIFORM_H
#define OUTSHINE_RENDER_STAGES_CLOUDVIEWUNIFORM_H

#include "CloudLayerUniform.h"
#include "math/Vec4.h"

#include <cstddef>

namespace outshine::Render {

struct alignas(16) CloudViewUniform {
  CloudLayerUniform Layer;
  Vec4f OriginM;
  Vec4f SunIrradiance;
  Vec4f SkyRadiance;

  [[nodiscard]] constexpr bool operator==(const CloudViewUniform &) const = default;
};

static_assert(sizeof(CloudViewUniform) == 6 * sizeof(Vec4f) && alignof(CloudViewUniform) == 16);
static_assert(offsetof(CloudViewUniform, Layer) == 0);
static_assert(offsetof(CloudViewUniform, OriginM) == 3 * sizeof(Vec4f));
static_assert(offsetof(CloudViewUniform, SunIrradiance) == 64);
static_assert(offsetof(CloudViewUniform, SkyRadiance) == 5 * sizeof(Vec4f));

}
#endif
