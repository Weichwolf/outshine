#ifndef OUTSHINE_RENDER_STAGES_MEDIUMVIEWPARAMETERS_H
#define OUTSHINE_RENDER_STAGES_MEDIUMVIEWPARAMETERS_H

#include "Atmosphere.h"
#include "math/Vec2.h"
#include "math/Vec4.h"
#include <cstddef>

namespace outshine::Render {

struct MediumViewParameters {
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
  Vec4f PixelToNdc;
  Vec4f OrthoProjection;
};

static_assert(sizeof(MediumViewParameters) == 9 * sizeof(Vec4f) + sizeof(Medium) &&
              alignof(MediumViewParameters) == 16);
static_assert(offsetof(MediumViewParameters, Right) == 0 &&
              offsetof(MediumViewParameters, Up) == sizeof(Vec4f) &&
              offsetof(MediumViewParameters, Fwd) == 2 * sizeof(Vec4f) &&
              offsetof(MediumViewParameters, WorldUp) == 3 * sizeof(Vec4f) &&
              offsetof(MediumViewParameters, SunDir) == 4 * sizeof(Vec4f) &&
              offsetof(MediumViewParameters, TanHalf) == 5 * sizeof(Vec4f) &&
              offsetof(MediumViewParameters, Illuminance) == 5 * sizeof(Vec4f) + sizeof(Vec2f) &&
              offsetof(MediumViewParameters, EyeRadiusKm) ==
                  5 * sizeof(Vec4f) + sizeof(Vec2f) + sizeof(float) &&
              offsetof(MediumViewParameters, DepthReconstruction) == 6 * sizeof(Vec4f) &&
              offsetof(MediumViewParameters, Air) == 7 * sizeof(Vec4f) &&
              offsetof(MediumViewParameters, PixelToNdc) == 7 * sizeof(Vec4f) + sizeof(Medium) &&
              offsetof(MediumViewParameters, OrthoProjection) ==
                  8 * sizeof(Vec4f) + sizeof(Medium));

}
#endif
