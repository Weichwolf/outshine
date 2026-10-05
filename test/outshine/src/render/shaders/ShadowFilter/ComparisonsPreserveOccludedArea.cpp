#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace {
struct vec2 {
  float x, y;
};

struct vec4 {
  float x, y, z, w;
};

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
  return Report();
}
