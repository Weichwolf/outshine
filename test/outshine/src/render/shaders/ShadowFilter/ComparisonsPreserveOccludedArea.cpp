#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace {
struct vec2 {
  float x, y;

  explicit vec2(float value) : x(value), y(value) {}

  vec2(float u, float v) : x(u), y(v) {}
};

struct vec4 {
  float x, y, z, w;

  explicit vec4(float value) : x(value), y(value), z(value), w(value) {}

  vec4(float a, float b, float c, float d) : x(a), y(b), z(c), w(d) {}
};

struct vec3 {
  float x, y, z;
};

vec2 operator/(vec2 value, float divisor) {
  return {value.x / divisor, value.y / divisor};
}

vec2 operator*(vec2 a, vec2 b) {
  return {a.x * b.x, a.y * b.y};
}

float dot(vec2 a, vec2 b) {
  return a.x * b.x + a.y * b.y;
}

float step(float edge, float value) {
  return value < edge ? 0.0f : 1.0f;
}

float mix(float a, float b, float weight) {
  return a + (b - a) * weight;
}

#include "src/render/shaders/shadowFilter.glsl"

double Intersection(double centre, int cell) {
  return std::max(0.0, std::min(centre + 0.5, cell + 0.5) - std::max(centre - 0.5, cell - 0.5));
}

double VisibleArea(std::array<float, 4> depths, vec2 centre, float receiver) {
  double area = 0;
  for (int y = 0; y < 2; ++y) {
    for (int x = 0; x < 2; ++x) {
      if (depths[static_cast<size_t>(y * 2 + x)] <= receiver) {
        area += Intersection(centre.x, x) * Intersection(centre.y, y);
      }
    }
  }
  return area;
}
}

int main() {
  using namespace outshine::Test;
  for (unsigned int mask = 0; mask < 16; ++mask) {
    std::array<float, 4> depths{};
    for (unsigned int at = 0; at < 4; ++at) { depths[at] = (mask & (1u << at)) != 0 ? 0.8f : 0.2f; }
    const vec4 stored{depths[0], depths[1], depths[2], depths[3]};
    for (float x : {0.0f, 0.125f, 0.5f, 0.875f, 1.0f}) {
      for (float y : {0.0f, 0.125f, 0.5f, 0.875f, 1.0f}) {
        for (float receiver : {0.0f, 0.2f, 0.5f, 0.8f, 1.0f}) {
          CHECK_NEAR(bilinearShadowVisibility(stored, {x, y}, receiver),
                     VisibleArea(depths, {x, y}, receiver),
                     1e-6,
                     "visibility",
                     "reverse-Z comparisons preserve independently intersected lit area");
        }
      }
    }
  }
  CHECK_NEAR(bilinearShadowVisibility({0.2f, 0.8f, 0.2f, 0.8f}, {0.5f, 0.5f}, 0.5f),
             0.5f,
             1e-6,
             "visibility",
             "a half-covered pixel stays half lit instead of comparing averaged depth");
  for (const vec2 slope : {vec2{0.2f, 0.0f}, vec2{0.0f, -0.3f}, vec2{0.2f, -0.3f}}) {
    const vec2 actual = shadowReceiverSlope({0.2f, 0.1f, 0.2f * slope.x + 0.1f * slope.y},
                                            {-0.1f, 0.3f, -0.1f * slope.x + 0.3f * slope.y});
    CHECK_NEAR(
        actual.x, slope.x, 1e-6, "depth slope", "the light-space affine plane preserves U slope");
    CHECK_NEAR(
        actual.y, slope.y, 1e-6, "depth slope", "the light-space affine plane preserves V slope");
    for (float x : {0.0f, 0.3f, 1.0f}) {
      for (float y : {0.0f, 0.7f, 1.0f}) {
        const vec4 depths{0.5f, 0.5f + slope.x, 0.5f + slope.y, 0.5f + slope.x + slope.y};
        const float centre = 0.5f + slope.x * x + slope.y * y;
        const vec4 receivers = shadowReceiverDepths(centre + 1e-5f, actual, {-x, -y}, {1.0f, 1.0f});
        CHECK_NEAR(bilinearShadowVisibility(depths, {x, y}, receivers),
                   1.0f,
                   1e-6,
                   "visibility",
                   "an unoccluded affine receiver plane never shadows itself");
        CHECK_NEAR(bilinearShadowVisibility(
                       depths,
                       {x, y},
                       shadowReceiverDepths(centre - 0.01f, actual, {-x, -y}, {1.0f, 1.0f})),
                   0.0f,
                   1e-6,
                   "visibility",
                   "a parallel occluder remains fully opaque");
      }
    }
  }
  const vec2 collapsed = shadowReceiverSlope({1, 2, 3}, {2, 4, 6});
  CHECK(collapsed.x == 0 && collapsed.y == 0,
        "a degenerate receiver projection has a finite fallback");
  return Report();
}
