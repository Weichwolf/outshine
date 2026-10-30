#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <tuple>

namespace {
struct vec2 {
  float x, y;

  explicit vec2(float value) : x(value), y(value) {}

  vec2(float u, float v) { std::tie(x, y) = std::tie(u, v); }
};

struct vec4 {
  float x, y, z, w;

  explicit vec4(float value) : x(value), y(value), z(value), w(value) {}

  vec4(float a, float b, float c, float d) { std::tie(x, y, z, w) = std::tie(a, b, c, d); }
};

struct vec3 {
  float x, y, z;

  vec3(float a, float b, float c) { std::tie(x, y, z) = std::tie(a, b, c); }
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
using std::max;
#include "src/render/shaders/shadowReceiverPlane.glsl"

void CheckProjectiveReceiverPlanes() {
  using namespace outshine::Test;
  for (const float p : {-0.4f, 0.0f, 0.7f}) {
    for (const float q : {-0.3f, 0.0f, 0.5f}) {
      for (const float w : {1.0f, 2.0f, 100.0f}) {
        const vec4 receiver{0.3f, -0.2f, 0.3f * p - 0.2f * q + 0.4f * w, w};
        for (const auto directionW : {std::array{0.0f, 0.0f}, std::array{0.25f, -0.125f}}) {
          const auto [dwX, dwY] = directionW;
          for (const float scale : {0.001f, 1.0f, 1000.0f}) {
            const vec4 dx{scale, 0.2f * scale, (p + 0.2f * q + 0.4f * dwX) * scale, dwX * scale};
            const vec4 dy{-0.1f * scale, scale, (-0.1f * p + q + 0.4f * dwY) * scale, dwY * scale};
            const auto actual = shadowReceiverPlaneSlope(receiver, dx, dy);
            CHECK_NEAR(actual.x,
                       2.0f * p,
                       1e-5,
                       "projective U slope",
                       "the homogeneous plane z=p*x+q*y+k*w fixes the receiver gradient");
            CHECK_NEAR(actual.y,
                       -2.0f * q,
                       1e-5,
                       "projective V slope",
                       "gradient is independent of tangent basis scale and projective w");
          }
        }
      }
    }
  }
  const auto collapsed = shadowReceiverPlaneSlope(vec4{0.2f, 0.3f, 0.4f, 1.0f}, vec4{1}, vec4{1});
  CHECK(collapsed.x == 0.0f && collapsed.y == 0.0f,
        "collapsed receiver directions retain the existing zero-slope fallback");
}

double Intersection(double centre, int cell) {
  return std::max(0.0, std::min(centre + 0.5, cell + 0.5) - std::max(centre - 0.5, cell - 0.5));
}

double VisibleArea(std::array<float, 4> depths, vec2 centre, float receiver) {
  double area = 0;
  for (int y = 0; y < 2; ++y) {
    for (int x = 0; x < 2; ++x) {
      if (depths[static_cast<size_t>(y) * 2 + static_cast<size_t>(x)] <= receiver) {
        area += Intersection(centre.x, x) * Intersection(centre.y, y);
      }
    }
  }
  return area;
}
}

int main() {
  using namespace outshine::Test;
  CheckProjectiveReceiverPlanes();
  for (const float spacing : {1.0e-6f, 1.0e-8f, 1.0e-10f}) {
    const vec3 origin = shadowReceiverVector({0, 0, 0});
    const vec3 east = shadowReceiverVector({spacing, 0, 0.15f * spacing});
    const vec3 north = shadowReceiverVector({0, spacing, -0.25f * spacing});
    const vec2 gradient =
        shadowReceiverSlope({east.x - origin.x, east.y - origin.y, east.z - origin.z},
                            {north.x - origin.x, north.y - origin.y, north.z - origin.z});
    CHECK_NEAR(
        gradient.x, 0.3f, 1e-6, "depth per UV", "centred coordinates retain metric east slope");
    CHECK_NEAR(
        gradient.y, 0.5f, 1e-6, "depth per UV", "centred coordinates retain metric north slope");
    if (spacing <= 1.0e-8f) {
      CHECK(east.x + 0.5f == origin.x + 0.5f,
            "texture-space affine offset destroys these real Float32 receiver differences");
    }
  }
  for (unsigned int mask = 0; mask < 16; ++mask) {
    std::array<float, 4> depths{};
    for (unsigned int at = 0; at < 4; ++at) { depths[at] = (mask & (1u << at)) != 0 ? 0.8f : 0.2f; }
    const vec4 stored{depths[0], depths[1], depths[2], depths[3]};
    for (const float x : {0.0f, 0.125f, 0.5f, 0.875f, 1.0f}) {
      for (const float y : {0.0f, 0.125f, 0.5f, 0.875f, 1.0f}) {
        for (const float receiver : {0.0f, 0.2f, 0.5f, 0.8f, 1.0f}) {
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
    for (const float x : {0.0f, 0.3f, 1.0f}) {
      for (const float y : {0.0f, 0.7f, 1.0f}) {
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
