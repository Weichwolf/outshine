#ifndef OUTSHINE_RENDER_STAGES_SKYPASS_H
#define OUTSHINE_RENDER_STAGES_SKYPASS_H

#include "math/Vec3.h"

namespace outshine::Render {

struct SkyStanding {
  Vec3f SunDir;
  Vec3f WorldUp;
  float IlluminanceLux = 0.0f;
  float EyeHeightM = 0.0f;
};

struct EyeBasis {
  Vec3f Right;
  Vec3f Up;
  Vec3f Forward;
  float TanHalfWidth = 0.0f;
  float TanHalfHeight = 0.0f;
};

} // namespace outshine::Render
#endif
