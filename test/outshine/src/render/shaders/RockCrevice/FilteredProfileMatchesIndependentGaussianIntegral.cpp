#include "Check.h"

#include <array>
#include <cmath>
#include <numbers>

namespace Shader {
struct vec2 {
  float x, y;

  vec2(float a, float b) : x(a), y(b) {}
};

float inversesqrt(float value) {
  return 1 / std::sqrt(value);
}

float exp(float value) {
  return std::exp(value);
}

#include "src/render/shaders/rockCrevice.glsl"
}

namespace {
std::array<double, 2> Integrated(double value, double variance) {
  constexpr int intervals = 2048;
  constexpr double width = .07;
  const double sigma = std::sqrt(variance);
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
  return result;
}
}

int main() {
  using namespace outshine::Test;
  constexpr std::array kPhaseSpans{0.0f, .01f, .05f, .2f, .4f, .7f, .8f, 2.0f};
  constexpr std::array kValues{-.49f, -.14f, -.02f, 0.0f, .02f, .14f, .49f};
  constexpr float kBoxVariance = 1.0f / 12.0f;
  for (float dx : kPhaseSpans) {
    for (float dy : kPhaseSpans) {
      const float variance = (dx * dx + dy * dy) * kBoxVariance;
      for (float value : kValues) {
        const auto actual = Shader::rockCrevice(value, variance);
        const auto expected = Integrated(value, variance);
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
              "bounded profile remains finite for every declared phase variance");
      }
    }
  }
  const auto point = Shader::rockCrevice(0, 0);
  CHECK(point.x == -1 && point.y == 0, "zero phase variance retains the point-profile limit");
  return Report();
}
