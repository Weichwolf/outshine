#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace {
using std::clamp;
using std::floor;
using std::max;

struct vec2 {
  float x, y;
};

struct ivec2 {
  int x, y;

  ivec2(int a, int b) : x(a), y(b) {}

  explicit ivec2(vec2 at) : x(static_cast<int>(at.x)), y(static_cast<int>(at.y)) {}
};

struct ivec3 {
  int x, y, z;

  ivec3(ivec2 at, int c) : x(at.x), y(at.y), z(c) {}
};

vec2 operator/(vec2 at, float divisor) {
  return {at.x / divisor, at.y / divisor};
}

ivec2 operator+(ivec2 first, ivec2 second) {
  return {first.x + second.x, first.y + second.y};
}

vec2 floor(vec2 at) {
  return {std::floor(at.x), std::floor(at.y)};
}

vec2 fract(vec2 at) {
  return {at.x - std::floor(at.x), at.y - std::floor(at.y)};
}

float smoothstep(float low, float high, float at) {
  const float unit = std::clamp((at - low) / (high - low), 0.0f, 1.0f);
  return unit * unit * (3 - 2 * unit);
}

double Corner2(int x, int y) {
  return .5 + .4 * std::sin(static_cast<double>(x) * 1.73 + static_cast<double>(y) * 2.31);
}

int HashCalls = 0;

float surfaceValue(ivec3 at) {
  ++HashCalls;
  return static_cast<float>(Corner2(at.x, at.y));
}

struct vec4 {
  std::array<float, 4> Values;

  explicit vec4(float value) : Values{value, value, value, value} {}

  vec4(float a, float b, float c, float d) : Values{a, b, c, d} {}

  float &operator[](size_t at) { return Values[at]; }
};

float fract(float value) {
  return value - std::floor(value);
}

#include "src/render/shaders/groundLitterFilter.glsl"

double Corner(int at) {
  return .5 + .4 * std::sin(static_cast<double>(at) * 1.73);
}

double Point(double at) {
  const int first = static_cast<int>(std::floor(at));
  const double t = at - first;
  const double smooth = t * t * (3 - 2 * t);
  return std::lerp(Corner(first), Corner(first + 1), smooth);
}

double Integrated(double centre, double width) {
  if (width == 0) { return Point(centre); }
  const double low = centre - width * .5;
  const double high = centre + width * .5;
  double integral = 0;
  for (int cell = static_cast<int>(std::floor(low)); cell <= static_cast<int>(std::floor(high));
       ++cell) {
    const double begin = std::max(low, static_cast<double>(cell));
    const double end = std::min(high, static_cast<double>(cell + 1));
    const double middle = (begin + end) * .5;
    const double radius = (end - begin) * .5;
    const double delta = radius / std::sqrt(3.0);
    integral += radius * (Point(middle - delta) + Point(middle + delta));
  }
  return integral / width;
}

std::vector<std::pair<double, double>> BoxPoints(double centre, double width) {
  if (width == 0) { return {{centre, 1}}; }
  const double low = centre - .5 * width;
  const double high = centre + .5 * width;
  std::vector<std::pair<double, double>> points;
  for (int cell = static_cast<int>(std::floor(low)); cell <= static_cast<int>(std::floor(high));
       ++cell) {
    const double start = std::max(low, static_cast<double>(cell));
    const double end = std::min(high, static_cast<double>(cell + 1));
    const double radius = .5 * (end - start);
    const double middle = .5 * (end + start);
    const double delta = radius / std::sqrt(3.0);
    points.emplace_back(middle - delta, radius / width);
    points.emplace_back(middle + delta, radius / width);
  }
  return points;
}

double Point2(double x, double y) {
  const int ix = static_cast<int>(std::floor(x));
  const int iy = static_cast<int>(std::floor(y));
  const double fx = x - ix;
  const double fy = y - iy;
  const double tx = fx * fx * (3 - 2 * fx);
  const double ty = fy * fy * (3 - 2 * fy);
  return std::lerp(std::lerp(Corner2(ix, iy), Corner2(ix + 1, iy), tx),
                   std::lerp(Corner2(ix, iy + 1), Corner2(ix + 1, iy + 1), tx),
                   ty);
}

double Box2(vec2 at, vec2 width) {
  const float footprint = std::max(width.x, width.y);
  if (footprint >= 2) { return 0; }
  double average = 0;
  for (const auto &[x, wx] : BoxPoints(at.x, width.x)) {
    for (const auto &[y, wy] : BoxPoints(at.y, width.y)) { average += wx * wy * Point2(x, y); }
  }
  const double t = std::clamp(static_cast<double>(footprint) - 1, 0.0, 1.0);
  return (average - .5) * (1 - t * t * (3 - 2 * t));
}

void CheckRectangles() {
  using namespace outshine::Test;
  for (int sample = 0; sample < 64; ++sample) {
    const vec2 at{-800.3f + static_cast<float>(sample) * .37f,
                  1200.7f - static_cast<float>(sample) * .11f};
    for (const float scale : {.1f, 1.0f, 7.0f}) {
      for (const vec2 width : {vec2{0, 0},
                               {.001f, .8f},
                               {.4f, .7f},
                               {1.2f, .2f},
                               {.9f, 1},
                               {1.99f, 1.1f},
                               {2, 0},
                               {0, 2},
                               {8, 8}}) {
        HashCalls = 0;
        const float actual = groundLitterField(at, width, scale);
        CHECK_NEAR(actual,
                   Box2(at / scale, width / scale),
                   4e-5,
                   "actual GLSL rectangular filter",
                   "independent split tensor Gaussian integration of the bicubic field");
        CHECK(HashCalls <= 16, "one filtered sample uses at most sixteen corners");
        if (std::max(width.x, width.y) / scale >= 2) {
          CHECK(actual == 0 && HashCalls == 0,
                "unresolved layers return the exact mean before hashes");
        }
        for (const float coverage : {0.0f, .58f, .7f, 1.0f}) {
          const float weight = coverage + 2 * std::min(coverage, 1 - coverage) * actual;
          CHECK(weight >= -1e-5f && weight <= 1.0f + 1e-5f,
                "filtered coverage stays convex for empty, partial and full litter");
        }
      }
    }
  }
}
}

int main() {
  using namespace outshine::Test;
  CheckRectangles();
  for (int sample = -128; sample <= 128; ++sample) {
    const float centre = static_cast<float>(sample) / 57.0f;
    for (const float width :
         {0.0f, .0001f, .005f, .01f, .011f, .05f, .25f, .5f, .8f, 1.0f, 1.3f, 1.99f, 2.0f}) {
      auto axis = groundLitterAxis(centre, width);
      double actual = 0;
      double total = 0;
      for (size_t at = 0; at < 4; ++at) {
        const float weight = axis.Weights[at];
        CHECK(weight >= -1e-5f && weight <= 1.0f + 1e-5f,
              "box weights remain convex within floating-point rounding");
        actual += weight * Corner(axis.First + static_cast<int>(at));
        total += weight;
      }
      CHECK_NEAR(total, 1.0, 3e-5, "constant field", "box filtering preserves mean material");
      CHECK_NEAR(actual,
                 Integrated(centre, width),
                 3e-5,
                 "actual GLSL box weights",
                 "independent split two-point Gaussian quadrature integrates each cubic exactly");
      CHECK(actual >= .1 - 3e-5 && actual <= .9 + 3e-5,
            "filtering never creates a new material extreme");
    }
  }
  return Report();
}
