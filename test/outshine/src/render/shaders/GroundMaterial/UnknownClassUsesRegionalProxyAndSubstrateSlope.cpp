#include "Check.h"
#include <array>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {
using uint = uint32_t;

struct vec2 {
  float x, y;
};

struct vec4 {
  float x, y, z, w;
};

std::array<uint, 4> groundClasses{0, 0, 2, 0};
std::array<float, 27> groundPalette{};

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
  for (uint invalid : {uint{99}, std::numeric_limits<uint>::max()}) {
    groundClasses[2] = invalid;
    CHECK(groundMaterialRow(-1, rows) == rows,
          "missing or invalid substrate resolves safe fallback");
    Same(groundWearsSlope(-1, rows, 20), fallback, "generic worlds retain their explicit fallback");
  }
  CHECK(groundMaterialRow(0, rows) == 0, "unmapped metadata cannot override a known class");
  return Report();
}
