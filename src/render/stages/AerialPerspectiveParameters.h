#ifndef OUTSHINE_RENDER_STAGES_AERIALPERSPECTIVEPARAMETERS_H
#define OUTSHINE_RENDER_STAGES_AERIALPERSPECTIVEPARAMETERS_H

#include "Atmosphere.h"
#include "math/Vec2.h"
#include "math/Vec4.h"
#include <cstddef>

namespace outshine::Render {

struct AerialPerspectiveParameters {
  Vec4f Right;
  Vec4f Up;
  Vec4f Fwd;
  Vec4f WorldUp;
  Vec4f SunDir;
  Vec2f TanHalf;
  float Illuminance;
  float EyeRadiusKm;
  Vec4f DepthReconstruction;
  Medium Air;
};

static_assert(sizeof(AerialPerspectiveParameters) == 7 * sizeof(Vec4f) + sizeof(Medium) &&
              alignof(AerialPerspectiveParameters) == 16);
static_assert(offsetof(AerialPerspectiveParameters, Right) == 0 &&
              offsetof(AerialPerspectiveParameters, Up) == sizeof(Vec4f) &&
              offsetof(AerialPerspectiveParameters, Fwd) == 2 * sizeof(Vec4f) &&
              offsetof(AerialPerspectiveParameters, WorldUp) == 3 * sizeof(Vec4f) &&
              offsetof(AerialPerspectiveParameters, SunDir) == 4 * sizeof(Vec4f) &&
              offsetof(AerialPerspectiveParameters, TanHalf) == 5 * sizeof(Vec4f) &&
              offsetof(AerialPerspectiveParameters, Illuminance) ==
                  5 * sizeof(Vec4f) + sizeof(Vec2f) &&
              offsetof(AerialPerspectiveParameters, EyeRadiusKm) ==
                  5 * sizeof(Vec4f) + sizeof(Vec2f) + sizeof(float) &&
              offsetof(AerialPerspectiveParameters, DepthReconstruction) == 6 * sizeof(Vec4f) &&
              offsetof(AerialPerspectiveParameters, Air) == 7 * sizeof(Vec4f));

}
#endif
