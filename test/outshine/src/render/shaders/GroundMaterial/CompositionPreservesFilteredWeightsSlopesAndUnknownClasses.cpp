#include "Check.h"
#include <array>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace {
using uint = uint32_t;
using std::abs;
using std::clamp;
using std::max;
using std::sqrt;

struct vec2 {
  float x, y;
};

struct vec4 {
  float x, y, z, w;
};

std::array<uint, 4> groundClasses{0, 0, 2, 0};
std::array<float, 57> groundPalette{};

uint floatBitsToUint(float value) {
  return std::bit_cast<uint>(value);
}

float smoothstep(float low, float high, float value) {
  const float t = std::clamp((value - low) / (high - low), 0.0f, 1.0f);
  return t * t * (3 - 2 * t);
}

vec4 mix(vec4 a, vec4 b, float t) {
  return {
      a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
}

#include "src/render/shaders/groundMaterial.glsl"
#include "src/render/shaders/groundEdge.glsl"

double IntegratedMask(float distance, float reach, float half) {
  const auto value = [reach](double at) {
    if (reach == 0) { return at > 0 ? 1.0 : (at < 0 ? 0.0 : 0.5); }
    const double u = std::clamp(.5 + at / (2.0 * reach), 0.0, 1.0);
    return u * u * (3 - 2 * u);
  };
  if (half == 0) { return value(distance); }
  const double start = static_cast<double>(distance) - half;
  const double end = static_cast<double>(distance) + half;
  std::vector<double> cuts{start, end};
  for (const double at : {-static_cast<double>(reach), static_cast<double>(reach)}) {
    if (at > start && at < end) { cuts.push_back(at); }
  }
  std::ranges::sort(cuts);
  double integral = 0;
  for (size_t at = 1; at < cuts.size(); ++at) {
    const double middle = .5 * (cuts[at - 1] + cuts[at]);
    const double radius = .5 * (cuts[at] - cuts[at - 1]);
    const double sample = radius / std::sqrt(3.0);
    integral += radius * (value(middle - sample) + value(middle + sample));
  }
  return integral / (2.0 * half);
}

void CheckMetricMasks() {
  using namespace outshine::Test;
  for (const float reach : {0.0f, .1f, .3f, .6f, .9f, 1.5f}) {
    for (const float half : {0.0f, 1e-6f, .02f, .5f, 8.0f, 32.0f, 1e3f, 1e6f}) {
      for (const float fraction : {-2.0f, -1.01f, -.99f, -.5f, 0.0f, .5f, .99f, 1.01f, 2.0f}) {
        const float distance = fraction * std::max(reach + half, .001f);
        const float actual = groundEdgeWeight(distance, reach, half);
        CHECK_NEAR(actual,
                   IntegratedMask(distance, reach, half),
                   8e-7,
                   "filtered material weight",
                   "the actual shader agrees with independent split Gaussian integration");
        CHECK(actual >= 0 && actual <= 1, "material weights stay normalized");
        CHECK_NEAR(actual + groundEdgeWeight(-distance, reach, half),
                   1.0,
                   1e-7,
                   "complementary material weights",
                   "both sides meet continuously without a half-coverage seam");
      }
    }
  }
  const std::array<vec2, 4> tangents{{{1, 0}, {0, 1}, {2, -3}, {-.25f, 4}}};
  for (const auto x : tangents) {
    for (const auto y : tangents) {
      const float half = groundHalfFootprint(x, y);
      for (double angle : {0.0, .3, .8, 1.5, 2.2, 3.0}) {
        const double exact = .5 * (std::abs(std::cos(angle) * x.x + std::sin(angle) * x.y) +
                                   std::abs(std::cos(angle) * y.x + std::sin(angle) * y.y));
        CHECK(half + 1e-6 >= exact,
              "the isotropic support bounds every tested rotated pixel projection");
      }
    }
  }
}

void Same(vec4 actual, vec4 expected, const char *reason) {
  using namespace outshine::Test;
  CHECK_NEAR(actual.x, expected.x, 1e-6, "linear red", reason);
  CHECK_NEAR(actual.y, expected.y, 1e-6, "linear green", reason);
  CHECK_NEAR(actual.z, expected.z, 1e-6, "linear blue", reason);
  CHECK_NEAR(actual.w, expected.w, 1e-6, "roughness", reason);
}
}

int main() {
  using namespace outshine::Test;
  CheckMetricMasks();
  constexpr uint rows = 3;
  groundPalette[0] = std::bit_cast<float>(rows);
  groundPalette[1] = std::bit_cast<float>(uint{1});
  groundPalette[2] = 4;
  constexpr vec4 sealed{.1f, .1f, .1f, .8f};
  constexpr vec4 rock{.5f, .6f, .7f, .65f};
  constexpr vec4 substrate{.3f, .2f, .1f, .9f};
  constexpr vec4 fallback{.1f, .8f, .1f, .5f};
  const std::array colours{sealed, rock, substrate, fallback};
  for (size_t at = 0; at < colours.size(); ++at) {
    const vec4 row = colours[at];
    groundPalette[4 + at * 4] = row.x;
    groundPalette[5 + at * 4] = row.y;
    groundPalette[6 + at * 4] = row.z;
    groundPalette[7 + at * 4] = row.w;
    groundPalette[20 + at] = at == 2 ? 30.0f : 90.0f;
  }
  for (int absent : {-1, 3, 999}) {
    CHECK(groundMaterialRow(absent, rows) == rows && groundSubstrateRow(absent, rows) == 2,
          "unknown appearance uses regional proxy; substrate controls rock stability");
    Same(groundWearsSlope(absent, rows, 10),
         fallback,
         "unknown landcover does not become a claim of bare soil");
    Same(groundWearsSlope(absent, rows, 32),
         {.3f, .7f, .4f, .575f},
         "authored slope transition mixes regional proxy and rock equally");
    Same(groundWearsSlope(absent, rows, 80),
         rock,
         "steep unknown terrain exposes rock instead of the green medium fallback");
  }
  for (float slope : {0.0f, 40.0f, 90.0f}) {
    Same(groundWearsSlope(0, rows, slope),
         sealed,
         "known sealed material retains its authored rule");
    Same(groundWearsSlope(1, rows, slope), rock, "explicit rock remains rock at every inclination");
  }
  groundClasses[2] = 1;
  Same(groundWearsSlope(-1, rows, 0), rock, "declared rock substrate receives full rock coverage");
  groundPalette[37] = .9f;
  groundPalette[45] = .3f;
  CHECK_NEAR(groundBoundaryReach(1, 2, rows),
             .9,
             1e-6,
             "metric edge reach",
             "natural neighbours share the broader authored transition");
  groundPalette[46] = 1;
  CHECK(groundBoundaryReach(1, 2, rows) == 0,
        "constructed boundaries retain a hard edge before footprint filtering");
  for (uint invalid : {uint{99}, std::numeric_limits<uint>::max()}) {
    groundClasses[2] = invalid;
    CHECK(groundMaterialRow(-1, rows) == rows,
          "missing or invalid substrate resolves safe fallback");
    Same(groundWearsSlope(-1, rows, 20), fallback, "generic worlds retain their explicit fallback");
  }
  CHECK(groundMaterialRow(0, rows) == 0, "unmapped metadata cannot override a known class");
  return Report();
}
