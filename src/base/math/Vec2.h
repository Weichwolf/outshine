#ifndef OUTSHINE_MATH_VEC2_H
#define OUTSHINE_MATH_VEC2_H

#include <array>
#include <cstddef>
#include <span>

namespace outshine {

template <typename Number> struct Vector2 {

  std::array<Number, 2> Axis = {Number{0}, Number{0}};

  [[nodiscard]] constexpr Number operator[](size_t axis) const { return Axis[axis]; }

  [[nodiscard]] constexpr Number &operator[](size_t axis) { return Axis[axis]; }

  [[nodiscard]] constexpr std::span<const Number, 2> Row() const { return Axis; }

  [[nodiscard]] constexpr std::span<Number, 2> Row() { return Axis; }

  [[nodiscard]] constexpr auto begin() const { return Axis.begin(); }

  [[nodiscard]] constexpr auto end() const { return Axis.end(); }

  [[nodiscard]] constexpr auto begin() { return Axis.begin(); }

  [[nodiscard]] constexpr auto end() { return Axis.end(); }

  [[nodiscard]] constexpr const Number *data() const { return Axis.data(); }

  [[nodiscard]] constexpr Number *data() { return Axis.data(); }

  [[nodiscard]] constexpr bool operator==(const Vector2 &) const = default;
};

using Vec2 = Vector2<double>;

using Vec2f = Vector2<float>;

static_assert(sizeof(Vec2) == 2 * sizeof(double) && alignof(Vec2) == alignof(double),
              "a pair is two doubles and nothing else");

static_assert(sizeof(Vec2f) == 2 * sizeof(float) && alignof(Vec2f) == alignof(float),
              "and two floats on the device side, which is what a uv stream reads");

template <typename Number>
[[nodiscard]] constexpr Vector2<Number> operator+(const Vector2<Number> &a,
                                                  const Vector2<Number> &b) {
  return {{a[0] + b[0], a[1] + b[1]}};
}

template <typename Number>
[[nodiscard]] constexpr Vector2<Number> operator-(const Vector2<Number> &a,
                                                  const Vector2<Number> &b) {
  return {{a[0] - b[0], a[1] - b[1]}};
}

template <typename Number>
[[nodiscard]] constexpr Vector2<Number> operator*(const Vector2<Number> &v, Number by) {
  return {{v[0] * by, v[1] * by}};
}

}

#endif
