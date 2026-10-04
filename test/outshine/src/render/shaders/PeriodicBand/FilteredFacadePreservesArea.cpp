#include "Check.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {

using std::clamp;
using std::max;
using std::min;

float fract(float value) {
  return value - std::floor(value);
}

#include "src/render/shaders/periodicBand.glsl"

double Covered(double centre, double width, double low, double high) {
  const double left = centre - width / 2;
  const double right = centre + width / 2;
  double area = 0;
  for (int period = static_cast<int>(std::floor(left)) - 1;
       period <= static_cast<int>(std::ceil(right));
       ++period) {
    area += std::max(0.0, std::min(right, period + high) - std::max(left, period + low));
  }
  return area / width;
}

}

int main() {
  using namespace outshine::Test;
  constexpr std::array bands{std::array{0.0f, 0.055f},
                             std::array{0.015f, 0.040f},
                             std::array{0.12f, 0.88f},
                             std::array{0.18f, 0.82f},
                             std::array{0.38f, 0.84f}};
  for (const auto &band : bands) {
    for (const float width : {0.01f, 0.2f, 0.75f, 1.0f, 2.5f, 17.0f, 128.0f}) {
      for (const float centre : {-3.1f, -0.01f, 0.0f, 0.2f, 0.5f, 0.82f, 0.99f, 20.7f}) {
        CHECK_NEAR(periodicBand(centre, band[0], band[1], width),
                   Covered(centre, width, band[0], band[1]),
                   0.00015,
                   "periodic coverage",
                   "the shader integral equals independent interval intersections across seams");
        const float point = centre + 0.0031f;
        constexpr double epsilon = 0.000001;
        const double slope = (Covered(point + epsilon, width, band[0], band[1]) -
                              Covered(point - epsilon, width, band[0], band[1])) /
                             (2.0 * epsilon);
        CHECK_NEAR(periodicBandSlope(point, band[0], band[1], width), slope, 0.002,
                   "filtered relief slope",
                   "the analytic shader slope equals independently intersected coverage differences");
      }
    }
    for (const float centre : {-0.25f, 0.0f, 0.4f, 0.99f, 31.5f}) {
      CHECK_NEAR(periodicBand(centre, band[0], band[1], 1.0f),
                 band[1] - band[0],
                 0.00001,
                 "subpixel area",
                 "a full period preserves material area at every phase instead of fading away");
    }
  }
  CHECK_NEAR(periodicBand(0.5f, 0.18f, 0.82f, 0.0f),
             1.0f,
             0.001,
             "magnified window",
             "a resolved pane remains fully covered");
  CHECK_NEAR(periodicBand(0.0f, 0.18f, 0.82f, 0.0f),
             0.0f,
             0.001,
             "magnified wall",
             "a resolved wall remains outside the pane");
  CHECK_NEAR(intervalBand(0.5f, 0.04f, 0.91f, 4.0f),
             (0.91f - 0.04f) / 4.0f,
             0.00001,
             "single entrance",
             "a distant entrance contributes its area without repeating on upper floors");
  CHECK(intervalBand(2.5f, 0.04f, 0.91f, 1.0f) == 0.0f,
        "upper floors do not acquire a repeated entrance");
  return Report();
}
