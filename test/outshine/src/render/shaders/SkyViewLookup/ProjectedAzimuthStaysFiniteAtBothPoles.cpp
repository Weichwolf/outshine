#include "Check.h"
#include <math/Vec3.h>
#include <algorithm>
#include <cmath>

namespace {
using vec3 = outshine::Vec3f;
using std::clamp;

float dot(vec3 a, vec3 b) {
  return outshine::Dot(a, b);
}

float inversesqrt(float value) {
  return 1.0f / std::sqrt(value);
}

#include "src/render/shaders/skyViewLookup.glsl"
}

int main() {
  using namespace outshine::Test;
  const vec3 up{{0.0f, 1.0f, 0.0f}};
  const vec3 east{{1.0f, 0.0f, 0.0f}};
  const vec3 south{{0.0f, 0.0f, 1.0f}};
  for (const vec3 sun : {up, up * -1.0f, east}) {
    for (const vec3 look : {up, up * -1.0f}) {
      CHECK(std::isfinite(skyLightViewCos(look, up, sun)), "a pole has no undefined azimuth");
    }
  }
  CHECK(skyLightViewCos(east, up, up) == 1.0f,
        "zenith sun uses the rotationally symmetric table column");
  CHECK_NEAR(skyLightViewCos(east, up, east),
             1.0,
             0.000001,
             "sunward",
             "equal horizontal projections have zero relative azimuth");
  CHECK_NEAR(skyLightViewCos(east * -1.0f, up, east),
             -1.0,
             0.000001,
             "opposite",
             "opposite horizontal projections have a half-turn relative azimuth");
  CHECK_NEAR(skyLightViewCos(south, up, east),
             0.0,
             0.000001,
             "quarter-turn",
             "orthogonal projections have a right-angle relative azimuth");
  CHECK_NEAR(skyLightViewCos({{1.0f, 3.0f, 0.0f}}, up, {{2.0f, 7.0f, 0.0f}}),
             1.0,
             0.000001,
             "projection",
             "vertical components and direction lengths do not change azimuth");
  CHECK_NEAR(skyLightViewCos({{1.0f, 0.0f, 3.0f}}, south, {{2.0f, 0.0f, 7.0f}}),
             1.0,
             0.000001,
             "rotated frame",
             "the world-up axis determines the projection plane");
  return Report();
}
