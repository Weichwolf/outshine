#ifndef OUTSHINE_RENDER_STAGES_SKYPARAMETERS_H
#define OUTSHINE_RENDER_STAGES_SKYPARAMETERS_H

#include "Vec3.h"

namespace outshine::Render {

struct SkyParameters {
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

}
#endif
