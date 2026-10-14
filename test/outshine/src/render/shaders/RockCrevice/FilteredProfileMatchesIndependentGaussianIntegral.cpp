#include "Check.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace Shader {
struct vec2 {
  float x, y;

  explicit vec2(float value) : x(value), y(value) {}

  vec2(float a, float b) : x(a), y(b) {}
};

vec2 operator*(vec2 value, float factor) {
  return {value.x * factor, value.y * factor};
}

float inversesqrt(float value) {
  return 1 / std::sqrt(value);
}

float exp(float value) {
  return std::exp(value);
}

float smoothstep(float least, float most, float value) {
  const float t = std::clamp((value - least) / (most - least), 0.0f, 1.0f);
  return t * t * (3 - 2 * t);
}

#include "src/render/shaders/rockCrevice.glsl"
}

namespace {
std::array<double, 2> Integrated(double value, double footprint, double wavelength) {
  constexpr int intervals = 2048;
  constexpr double width = .07;
  const double sigma = footprint / wavelength * 1.5 * std::sqrt(3.0 / 6.0);
  std::array<double, 2> result{};
  for (int at = 0; at <= intervals; ++at) {
    const double position = -8 + 16.0 * at / intervals;
    const double displaced = value + sigma * position;
    const double density = std::exp(-position * position / 2) / std::sqrt(2 * std::numbers::pi);
    const double pulse = std::exp(-displaced * displaced / (2 * width * width));
    const double weight = (at == 0 || at == intervals ? .5 : 1) * 16 / intervals;
    result[0] -= weight * density * pulse;
    result[1] += weight * density * pulse * displaced / (width * width);
  }
  const double t = std::clamp((footprint / wavelength - .25) / .55, 0.0, 1.0);
  for (double &channel : result) { channel *= 1 - t * t * (3 - 2 * t); }
  return result;
}
}

int main() {
  using namespace outshine::Test;
  for (float wavelength : {1.0f, 6.0f, 72.0f}) {
    for (float fraction : {0.0f, .01f, .05f, .2f, .4f, .7f, .8f, 2.0f}) {
      const float footprint = wavelength * fraction;
      for (float value : {-.49f, -.14f, -.02f, 0.0f, .02f, .14f, .49f}) {
        const auto actual = Shader::rockCrevice(value, footprint, wavelength);
        const auto expected = Integrated(value, footprint, wavelength);
        CHECK_NEAR(actual.x,
                   expected[0],
                   1e-5,
                   "noise height",
                   "filtered crevice matches numerical convolution");
        CHECK_NEAR(actual.y,
                   expected[1],
                   1e-4,
                   "noise derivative",
                   "filtered derivative matches independent integration");
        CHECK(std::isfinite(actual.x) && std::isfinite(actual.y) && actual.x >= -1 && actual.x <= 0,
              "bounded profile remains finite for every declared footprint");
        if (fraction >= .8f) {
          CHECK(actual.x == 0 && actual.y == 0,
                "unresolved crevices spend no response on subpixel relief");
        }
      }
    }
  }
  CHECK(Shader::rockCrevice(0, 1, 0).x == 0, "absent wavelength has no relief");
  return Report();
}
