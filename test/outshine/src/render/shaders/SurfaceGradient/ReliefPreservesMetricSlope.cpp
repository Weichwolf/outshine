#include "Check.h"
#include <math/Vec3.h>
#include <cmath>

namespace {

using vec3 = outshine::Vec3f;
using std::abs;

float sign(float value) { return value < 0.0f ? -1.0f : value > 0.0f ? 1.0f : 0.0f; }
vec3 cross(vec3 a, vec3 b) { return outshine::Cross(a, b); }
float dot(vec3 a, vec3 b) { return outshine::Dot(a, b); }
vec3 operator/(vec3 value, float divisor) { return value * (1.0f / divisor); }
vec3 normalize(vec3 value) {
  static_cast<void>(outshine::Normalise(value));
  return value;
}

#include "src/render/shaders/surfaceGradient.glsl"

void SameNormal(vec3 actual, vec3 expected) {
  for (size_t axis = 0; axis < 3; ++axis) {
    CHECK_NEAR(actual[axis], expected[axis], 0.00001, "metric relief",
               "the shader matches the analytic displaced plane in the same coordinate frame");
  }
}

}

int main() {
  using namespace outshine::Test;
  const vec3 n{{0.0f, 1.0f, 0.0f}};
  const vec3 dx{{2.0f, 0.0f, 0.5f}};
  const vec3 dy{{-1.0f, 0.0f, 3.0f}};
  const vec3 expected = normalize({{-0.12f, 1.0f, 0.07f}});
  for (const float scale : {0.01f, 1.0f, 1000.0f}) {
    SameNormal(bumpNormal(n, n, dx * scale, dy * scale, 0.205f * scale, -0.33f * scale), expected);
    SameNormal(bumpNormal(n, n, dy * scale, dx * scale, -0.33f * scale, 0.205f * scale), expected);
  }
  SameNormal(bumpNormal({{0.0f, 0.0f, 1.0f}}, {{0.0f, 0.0f, 1.0f}},
                        {{2.0f, 0.5f, 0.0f}}, {{-1.0f, 3.0f, 0.0f}}, 0.205f, -0.33f),
             normalize({{-0.12f, 0.07f, 1.0f}}));
  SameNormal(bumpNormal(n, n, dx, dx, 0.205f, 0.205f), n);
  SameNormal(bumpNormal(n, n, dx, dy, 0.0f, 0.0f), n);
  const vec3 mapped = normalize({{-0.1f, 1.0f, -0.02f}});
  SameNormal(bumpNormal(n, mapped, dx, dy, 0.205f, -0.33f),
             normalize({{-0.22f, 1.0f, 0.05f}}));
  return Report();
}
