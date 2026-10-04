#include "Check.h"
#include <math/Vec3.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace Shader {
using vec3 = outshine::Vec3f;
using std::abs;
using std::clamp;
using std::max;
using std::min;

float fract(float value) {
  return value - std::floor(value);
}

float sign(float value) {
  return value < 0.0f ? -1.0f : value > 0.0f ? 1.0f : 0.0f;
}

float inversesqrt(float value) {
  return 1.0f / std::sqrt(value);
}

vec3 cross(vec3 a, vec3 b) {
  return outshine::Cross(a, b);
}

float dot(vec3 a, vec3 b) {
  return outshine::Dot(a, b);
}

vec3 operator/(vec3 value, float divisor) {
  return value * (1.0f / divisor);
}

vec3 normalize(vec3 value) {
  static_cast<void>(outshine::Normalise(value));
  return value;
}

#include "src/render/shaders/periodicBand.glsl"
#include "src/render/shaders/surfaceGradient.glsl"
#include "src/render/shaders/roofTiles.glsl"

double Intersection(double left, double right, double low, double high) {
  return std::max(0.0, std::min(right, high) - std::max(left, low));
}

double JointArea(double course, double tile, double courseWidth, double tileWidth) {
  const double bottom = course - courseWidth / 2;
  const double top = course + courseWidth / 2;
  const double left = tile - tileWidth / 2;
  const double right = tile + tileWidth / 2;
  double solid = 0;
  for (int row = static_cast<int>(std::floor(bottom)); row < std::ceil(top); ++row) {
    const double height = Intersection(bottom, top, row + 0.055, row + 1.0);
    for (int column = static_cast<int>(std::floor(left)); column < std::ceil(right); ++column) {
      const double width = Intersection(left, right, column, column + 0.485) +
                           Intersection(left, right, column + 0.515, column + 1.0);
      solid += height * width;
    }
  }
  return 1.0 - solid / (courseWidth * tileWidth);
}
}

int main() {
  using namespace outshine::Test;
  using namespace Shader;
  constexpr std::array widths{0.01f, 0.2f, 0.75f, 2.5f, 17.0f};
  for (float courseWidth : widths) {
    for (float tileWidth : widths) {
      for (float centre : {-3.1f, 0.2f, 0.5031f, 20.7f}) {
        const float course = centre + 0.0031f;
        const float tile = centre - 0.127f;
        const auto joint = roofTileJoint(course, tile, courseWidth, tileWidth);
        CHECK_NEAR(joint.coverage,
                   JointArea(course, tile, courseWidth, tileWidth),
                   0.0003,
                   "joint area",
                   "filtered union equals independent solid-rectangle intersections");
        constexpr double epsilon = 1.0e-6;
        const double courseSlope = (JointArea(course + epsilon, tile, courseWidth, tileWidth) -
                                    JointArea(course - epsilon, tile, courseWidth, tileWidth)) /
                                   (2 * epsilon);
        const double tileSlope = (JointArea(course, tile + epsilon, courseWidth, tileWidth) -
                                  JointArea(course, tile - epsilon, courseWidth, tileWidth)) /
                                 (2 * epsilon);
        CHECK_NEAR(joint.courseSlope,
                   courseSlope,
                   0.003,
                   "course relief",
                   "analytic union derivative agrees with independent coverage differences");
        CHECK_NEAR(joint.tileSlope,
                   tileSlope,
                   0.003,
                   "cross-joint relief",
                   "analytic union derivative agrees with independent coverage differences");
      }
    }
  }
  for (float phase : {-31.5f, -0.1f, 0.2f, 0.99f, 20.7f}) {
    const auto joint = roofTileJoint(phase, phase, 1, 1);
    CHECK_NEAR(joint.coverage,
               0.08335,
               0.00001,
               "minified material area",
               "one complete tile preserves joint coverage at every phase");
    CHECK_NEAR(joint.courseSlope,
               0,
               0.00001,
               "minified course relief",
               "unresolved complete periods contribute no false normal slope");
    CHECK_NEAR(joint.tileSlope,
               0,
               0.00001,
               "minified cross-joint relief",
               "unresolved complete periods contribute no false normal slope");
  }
  for (float scale : {0.01f, 1.0f, 1000.0f}) {
    const auto axis =
        roofTileAxis(vec3{{2, 0, 0}} * scale, vec3{{0, 0.6f, 0.8f}} * scale, 0, 0.6f * scale);
    CHECK_NEAR(dot(axis, vec3{{1, 0, 0}}),
               1,
               0.00001,
               "metric eave axis",
               "screen derivative scale preserves the known roof direction");
    const auto rotated =
        roofTileAxis(vec3{{0, 2, 0}} * scale, vec3{{0.8f, 0, 0.6f}} * scale, 0, 0.6f * scale);
    CHECK_NEAR(dot(rotated, vec3{{0, 1, 0}}),
               1,
               0.00001,
               "rotated roof axis",
               "object rotation rotates the eave direction without changing tile dimensions");
  }
  const auto flat = roofTileAxis({{1, 0, 0}}, {{0, 0, 1}}, 0, 0);
  const auto singular = roofTileAxis({{1, 0, 0}}, {{1, 0, 0}}, 1, 1);
  CHECK(dot(flat, flat) == 0 && dot(singular, singular) == 0,
        "flat and singular charts introduce no arbitrary cross-joint direction");
  return Report();
}
