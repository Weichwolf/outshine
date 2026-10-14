#include "Check.h"
#include <array>
#include <cmath>
#include <cstdint>

namespace Shader {

template <typename Number> struct Pair {
  Number x, y;

  Pair(Number a, Number b) : x(a), y(b) {}

  template <typename Other>
  explicit Pair(Pair<Other> other)
      : x(static_cast<Number>(other.x)), y(static_cast<Number>(other.y)) {}
};

template <typename Number> struct Triple {
  Number x, y, z;

  Triple(Number a, Number b, Number c) : x(a), y(b), z(c) {}

  Triple(Pair<Number> pair, Number c) : x(pair.x), y(pair.y), z(c) {}

  template <typename Other>
  explicit Triple(Triple<Other> other)
      : x(static_cast<Number>(other.x)),
        y(static_cast<Number>(other.y)),
        z(static_cast<Number>(other.z)) {}
};

using uint = uint32_t;
using vec2 = Pair<float>;
using vec3 = Triple<float>;
using ivec2 = Pair<int32_t>;
using ivec3 = Triple<int32_t>;
using uvec3 = Triple<uint>;

struct vec4 {
  float x, y, z, w;

  vec4(float a, float b, float c, float d) : x(a), y(b), z(c), w(d) {}
};

template <typename V> V operator+(V a, V b) {
  if constexpr (requires { a.z; }) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
  } else {
    return {a.x + b.x, a.y + b.y};
  }
}

template <typename V> V operator-(V a, V b) {
  if constexpr (requires { a.z; }) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
  } else {
    return {a.x - b.x, a.y - b.y};
  }
}

template <typename V> V operator-(float a, V b) {
  if constexpr (requires { b.z; }) {
    return {a - b.x, a - b.y, a - b.z};
  } else {
    return {a - b.x, a - b.y};
  }
}

template <typename V> V operator*(V a, V b) {
  if constexpr (requires { a.z; }) {
    return {a.x * b.x, a.y * b.y, a.z * b.z};
  } else {
    return {a.x * b.x, a.y * b.y};
  }
}

template <typename V> V operator*(float a, V b) {
  if constexpr (requires { b.z; }) {
    return {a * b.x, a * b.y, a * b.z};
  } else {
    return {a * b.x, a * b.y};
  }
}

template <typename V> V floor(V value) {
  if constexpr (requires { value.z; }) {
    return {std::floor(value.x), std::floor(value.y), std::floor(value.z)};
  } else {
    return {std::floor(value.x), std::floor(value.y)};
  }
}

template <typename V> V fract(V value) {
  return value - floor(value);
}

float mix(float a, float b, float weight) {
  return a * (1.0f - weight) + b * weight;
}

#include "src/render/shaders/surfaceNoise.glsl"
}

double Reference(std::array<double, 3> at) {
  std::array<int32_t, 3> cell{};
  std::array<double, 3> smooth{};
  for (size_t axis = 0; axis < 3; ++axis) {
    cell[axis] = static_cast<int32_t>(std::floor(at[axis]));
    const double t = at[axis] - cell[axis];
    smooth[axis] = t * t * (3.0 - 2.0 * t);
  }
  double total = 0;
  for (int z = 0; z < 2; ++z) {
    for (int y = 0; y < 2; ++y) {
      for (int x = 0; x < 2; ++x) {
        const double weight = (x ? smooth[0] : 1 - smooth[0]) * (y ? smooth[1] : 1 - smooth[1]) *
                              (z ? smooth[2] : 1 - smooth[2]);
        total += weight * Shader::surfaceValue({cell[0] + x, cell[1] + y, cell[2] + z});
      }
    }
  }
  return total;
}

int main() {
  using namespace outshine::Test;
  constexpr double step = 1e-6;
  for (size_t sample = 0; sample < 512; ++sample) {
    const std::array<double, 3> point{static_cast<float>(-7.3 + sample * .047),
                                      static_cast<float>(1770.773 + sample * .0013),
                                      static_cast<float>(-3.5 + sample * .029)};
    const auto field = Shader::surfaceNoise3(
        {static_cast<float>(point[0]), static_cast<float>(point[1]), static_cast<float>(point[2])});
    const auto planar =
        Shader::surfaceNoise2({static_cast<float>(point[0]), static_cast<float>(point[1])});
    const std::array gradients{field.x, field.y, field.z};
    CHECK_NEAR(
        field.w, Reference(point), 1e-6, "lattice value", "eight-corner double interpolation");
    CHECK_NEAR(planar.z,
               Reference({point[0], point[1], 0}),
               1e-6,
               "planar value",
               "the four-corner field is the zero-height slice");
    for (size_t axis = 0; axis < 3; ++axis) {
      auto low = point;
      auto high = point;
      low[axis] -= step;
      high[axis] += step;
      CHECK_NEAR(gradients[axis],
                 (Reference(high) - Reference(low)) / (2 * step),
                 4e-6,
                 "lattice slope",
                 "independent double differences, including cubic cell joins");
      if (axis < 2) {
        low[2] = high[2] = 0;
        const float actual = axis == 0 ? planar.x : planar.y;
        CHECK_NEAR(actual,
                   (Reference(high) - Reference(low)) / (2 * step),
                   4e-6,
                   "planar slope",
                   "independent slice differences");
      }
    }
  }
  const auto corner = Shader::surfaceNoise3({-1, 1770, 0});
  CHECK(corner.x == 0 && corner.y == 0 && corner.z == 0,
        "cubic lattice joins have exactly zero first derivative");
  return Report();
}
