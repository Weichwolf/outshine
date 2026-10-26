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

struct vec2 {
  float x, y;

  vec2(float a, float b) : x(a), y(b) {}
};

struct vec3 {
  float x, y, z;

  vec3(float a, float b, float c) : x(a), y(b), z(c) {}
};

vec3 operator*(vec3 value, float scale) {
  return {value.x * scale, value.y * scale, value.z * scale};
}

namespace {
vec3 operator-(vec3 a, vec3 b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}
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
#include "src/render/shaders/facadeBands.glsl"
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

void CheckCoverage(Shader::vec2 fractions) {
  using namespace outshine::Test;
  const double lowU = (1.0 - fractions.x) * 0.5;
  const double highU = (1.0 + fractions.x) * 0.5;
  const double lowV = (1.0 - fractions.y) * 0.5;
  const double highV = (1.0 + fractions.y) * 0.5;
  for (float x : {-3.75f, 0.0f, 0.27f, 0.5f, 0.73f, 11.0f}) {
    for (float y : {-4.0f, 0.28f, 0.5f, 0.78f, 17.0f}) {
      for (float wx : {0.01f, 0.25f, 1.0f, 8.0f}) {
        for (float wy : {0.01f, 0.5f, 1.0f, 16.0f}) {
          const double expected =
              VisibleFraction(x, wx, lowU, highU) * VisibleFraction(y, wy, lowV, highV);
          CHECK_NEAR(Shader::facadeOpeningCoverage(fractions, x, y, wx, wy),
                     expected,
                     3e-5,
                     "coverage",
                     "filtered glass equals independently clipped opening rectangles");
          const double panes = fractions.x > 0 && fractions.y > 0
                                   ? (VisibleFraction(x, wx, lowU + .02, .494) +
                                      VisibleFraction(x, wx, .506, highU - .02)) *
                                         VisibleFraction(y, wy, lowV + .02, highV - .02)
                                   : 0;
          CHECK_NEAR(Shader::facadePaneCoverage(fractions, x, y, wx, wy),
                     panes,
                     3e-5,
                     "coverage",
                     "panes equal independently clipped rectangles around mullion");
          CHECK(panes <= expected + 3e-5, "frame, panes and wall partition the facade");
        }
      }
    }
  }
}

namespace {
void CheckBands() {
  using namespace outshine::Test;
  constexpr double kBandM = 0.16;
  constexpr double kBandDarkening = 0.35;
  constexpr float kOpeningHeight = 0.53f;
  constexpr float kCommercialHeight = 0.65f;
  constexpr float kTypicalFloorM = 2.9f;
  constexpr double kCoverageError = 5e-5;
  constexpr double kLinearError = 1e-7;
  constexpr double kNarrowGap = 0.02;
  constexpr double kProductError = 0.002;
  for (const float floorM : {1.0f, 2.4f, 2.9f, 4.0f, 12.0f}) {
    const double fraction = std::min(kBandM / floorM, kBandDarkening);
    for (const float at : {-4096.9f, -2.47f, 0.0f, 0.03f, 0.5f, 4096.125f}) {
      for (const float width : {0.0002f, 0.03f, 0.5f, 1.0f, 16.0f}) {
        const double expected = VisibleFraction(at, width, 0, 0.5 * fraction) +
                                VisibleFraction(at, width, 1.0 - 0.5 * fraction, 1);
        CHECK_NEAR(Shader::facadeStoreyBand(kCommercialHeight, at, width, 1.0f / floorM),
                   expected,
                   kCoverageError,
                   "coverage",
                   "metric bands equal explicitly clipped opaque floor-boundary intervals");
      }
    }
  }
  const float band = Shader::facadeStoreyBand(kOpeningHeight, 0.0f, 16.0f, 1.0f / kTypicalFloorM);
  CHECK_NEAR(band,
             kBandM / kTypicalFloorM,
             kLinearError,
             "coverage",
             "whole floors preserve metric band area");
  CHECK_NEAR(Shader::facadeStoreyBand(1.0f - static_cast<float>(kNarrowGap), 0.5f, 1.0f, 1.0f),
             kNarrowGap,
             kLinearError,
             "coverage",
             "band thickness is bounded by the opaque gap");
  CHECK(Shader::facadeStoreyBand(1, 0, 1, 1) == 0, "full glazing leaves no band");
  CHECK(Shader::facadeStoreyBand(kOpeningHeight, 0, 1, 0) == 0,
        "degenerate metric frame adds no band");
  constexpr float opening = 0.48f * 0.53f;
  constexpr float pane = (0.44f - 0.012f) * 0.49f;
  const Shader::vec3 wall(0.5f, 0.4f, 0.3f);
  const auto result = Shader::facadeBandedWindow(opening, pane, band, wall, 0.8f);
  const double expected =
      0.5 * (1 - opening - band) + 0.325 * band + 0.48 * (opening - pane) + 0.025 * pane;
  CHECK_NEAR(result.albedo.x,
             expected,
             kLinearError,
             "linear",
             "one disjoint wall/band/frame/glass mixture");
  const auto old = Shader::facadeWindow(opening, pane, wall, 0.8f);
  CHECK(result.roughness == old.roughness, "band and plaster share roughness");
  const auto unchanged = Shader::facadeBandedWindow(opening, pane, 0, wall, 0.8f);
  CHECK(unchanged.albedo.x == old.albedo.x && unchanged.roughness == old.roughness,
        "zero band exactly preserves the existing facade");
  const auto sequential = Shader::facadeWindow(opening, pane, wall * (1.0f - 0.35f * band), 0.8f);
  CHECK(std::abs(sequential.albedo.x - result.albedo.x) > kProductError,
        "multiplying already filtered masks fails the same area-weighted reference");
}
}

int main() {
  using namespace outshine::Test;
  CheckBands();
  for (const auto fractions : {Shader::vec2(.48f, .53f),
                               Shader::vec2(.68f, .65f),
                               Shader::vec2(.57f, .62f),
                               Shader::vec2(.30f, .22f),
                               Shader::vec2(.25f, .55f),
                               Shader::vec2(0, 0),
                               Shader::vec2(1, 1)}) {
    CheckCoverage(fractions);
  }
  CHECK_NEAR(Shader::facadeOpeningCoverage(Shader::vec2(.48f, .53f), 0.0f, 0.0f, 8.0f, 16.0f),
             0.2544f,
             1e-6f,
             "coverage",
             "whole-period minification preserves 25.44 percent glass area");
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
  constexpr float openingArea = 0.48f * 0.53f;
  constexpr float paneArea = (0.44f - 0.012f) * 0.49f;
  const auto coarse = Shader::facadeWindow(openingArea, paneArea, albedo, roughness);
  const auto near = Shader::facadeWindow(1.0f, paneArea / openingArea, albedo, roughness);
  CHECK_NEAR(coarse.albedo.x,
             albedo.x * (1.0f - openingArea) + near.albedo.x * openingArea,
             1e-7f,
             "linear",
             "minified shell equals area-weighted near wall and framed glass");
  CHECK_NEAR(Shader::facadePaneCoverage(Shader::vec2(.48f, .53f), 0.0f, 0.0f, 8.0f, 16.0f),
             paneArea,
             1e-6f,
             "coverage",
             "minification preserves pane area and mullion coverage");
  return Report();
}
