#ifndef OUTSHINE_RENDER_SCENE_SCENELIGHTING_H
#define OUTSHINE_RENDER_SCENE_SCENELIGHTING_H

#include "PunctualLight.h"
#include "Vec3.h"

namespace outshine::Render {

struct SceneLight {
  outshine::PunctualLight Light;
  Vec3 PositionEcefM = {{0, 0, 0}};
};

struct EnvironmentLighting {
  Vec3 RadianceLinear = {{0, 0, 0}};
  Vec3 GroundLinear = {{0, 0, 0}};
  Vec3 UpUnit = {{0, 1, 0}};

  Vec3 GroundAlbedo;
  double SkyLux = 0.0;
  double CosSunZenith = 0.0;
};

}

#endif
