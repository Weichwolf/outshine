#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace Shader {
using std::clamp;
using std::max;
using std::min;

float fract(float value) {
  return value - std::floor(value);
}

struct vec3 {
  float x, y, z;

  vec3(float a, float b, float c) : x(a), y(b), z(c) {}
};

vec3 operator*(vec3 value, float scale) {
  return {value.x * scale, value.y * scale, value.z * scale};
}

vec3 operator+(vec3 a, vec3 b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}

vec3 mix(vec3 a, vec3 b, float t) {
  return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}

float mix(float a, float b, float t) {
  return a + (b - a) * t;
}

#include "src/render/shaders/periodicBand.glsl"
#include "src/render/shaders/facadeOpenings.glsl"
}

namespace {
double VisibleFraction(double coordinate, double width, double low, double high) {
  const double left = coordinate - 0.5 * width;
  const double right = coordinate + 0.5 * width;
  double visible = 0;
  for (int period = static_cast<int>(std::floor(left)); period <= std::ceil(right); ++period) {
    visible += std::max(0.0, std::min(right, period + high) - std::max(left, period + low));
  }
  return visible / width;
}
}

int main() {
  using namespace outshine::Test;
  for (float x : {-3.75f, 0.0f, 0.27f, 0.5f, 0.73f, 11.0f}) {
    for (float y : {-4.0f, 0.28f, 0.5f, 0.78f, 17.0f}) {
      for (float wx : {0.01f, 0.25f, 1.0f, 8.0f}) {
        for (float wy : {0.01f, 0.5f, 1.0f, 16.0f}) {
          const double expected =
              VisibleFraction(x, wx, 0.27, 0.73) * VisibleFraction(y, wy, 0.28, 0.78);
          CHECK_NEAR(Shader::facadeOpeningCoverage(x, y, wx, wy),
                     expected,
                     3e-5,
                     "coverage",
                     "filtered glass equals independently clipped opening rectangles");
          const double panes =
              (VisibleFraction(x, wx, 0.29, 0.494) + VisibleFraction(x, wx, 0.506, 0.71)) *
              VisibleFraction(y, wy, 0.30, 0.76);
          CHECK_NEAR(Shader::facadePaneCoverage(x, y, wx, wy),
                     panes,
                     3e-5,
                     "coverage",
                     "panes equal independently clipped rectangles around mullion");
          CHECK(panes <= expected + 3e-5, "frame, panes and wall partition the facade");
        }
      }
    }
  }
  CHECK_NEAR(Shader::facadeOpeningCoverage(0.0f, 0.0f, 8.0f, 16.0f),
             0.23f,
             1e-6f,
             "coverage",
             "whole-period minification preserves 23 percent glass area");
  CHECK_NEAR(Shader::kOpeningDepthM, 0.16f, 1e-7f, "m", "material and mesh share recess depth");
  Shader::vec3 albedo(0.5f, 0.5f, 0.5f);
  float roughness = 0.8f;
  const auto wall = Shader::facadeGlazing(0.0f, albedo, roughness);
  CHECK(wall.albedo.x == 0.5f && wall.roughness == 0.8f, "closed wall material stays intact");
  const auto glass = Shader::facadeGlazing(1.0f, albedo, roughness);
  CHECK_NEAR(glass.albedo.x, 0.025f, 1e-7f, "linear", "planned glass has its own material");
  CHECK_NEAR(glass.roughness, 0.12f, 1e-7f, "roughness", "planned glass keeps the native BRDF");
  const auto frame = Shader::facadeWindow(1.0f, 0.0f, albedo, roughness);
  CHECK_NEAR(frame.albedo.x, 0.48f, 1e-7f, "linear", "opaque frame has its own material");
  CHECK_NEAR(frame.roughness, 0.55f, 1e-7f, "roughness", "frame differs from glass and plaster");
  const auto closed = Shader::facadeWindow(0.0f, 0.0f, albedo, roughness);
  CHECK(closed.albedo.x == albedo.x && closed.roughness == roughness,
        "window composition preserves untouched wall");
  constexpr float openingArea = 0.46f * 0.50f;
  constexpr float paneArea = (0.42f - 0.012f) * 0.46f;
  const auto coarse = Shader::facadeWindow(openingArea, paneArea, albedo, roughness);
  const auto near = Shader::facadeWindow(1.0f, paneArea / openingArea, albedo, roughness);
  CHECK_NEAR(coarse.albedo.x,
             albedo.x * (1.0f - openingArea) + near.albedo.x * openingArea,
             1e-7f,
             "linear",
             "minified shell equals area-weighted near wall and framed glass");
  CHECK_NEAR(Shader::facadePaneCoverage(0.0f, 0.0f, 8.0f, 16.0f),
             paneArea,
             1e-6f,
             "coverage",
             "minification preserves pane area and mullion coverage");
  return Report();
}
