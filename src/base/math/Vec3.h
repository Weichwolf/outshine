#ifndef OUTSHINE_BASE_MATH_VEC3_H
#define OUTSHINE_BASE_MATH_VEC3_H

#include <array>
#include <algorithm>
#include <concepts>
#include <cmath>
#include <cstddef>
#include <span>

namespace outshine {

template <typename Number> struct Vector3 {

  std::array<Number, 3> Axis = {Number{0}, Number{0}, Number{0}};

  [[nodiscard]] constexpr Number operator[](size_t axis) const { return Axis[axis]; }

  [[nodiscard]] constexpr Number &operator[](size_t axis) { return Axis[axis]; }

  [[nodiscard]] constexpr std::span<const Number, 3> Row() const { return Axis; }

  [[nodiscard]] constexpr std::span<Number, 3> Row() { return Axis; }

  [[nodiscard]] constexpr auto begin() const { return Axis.begin(); }

  [[nodiscard]] constexpr auto end() const { return Axis.end(); }

  [[nodiscard]] constexpr auto begin() { return Axis.begin(); }

  [[nodiscard]] constexpr auto end() { return Axis.end(); }

  [[nodiscard]] constexpr const Number *data() const { return Axis.data(); }

  [[nodiscard]] constexpr Number *data() { return Axis.data(); }

  [[nodiscard]] constexpr bool operator==(const Vector3 &) const = default;
};

using Vec3 = Vector3<double>;

using Vec3f = Vector3<float>;

static_assert(sizeof(Vec3) == 3 * sizeof(double) && alignof(Vec3) == alignof(double),
              "a vector is three doubles and nothing else -- alignment is the record's decision");

static_assert(sizeof(Vec3f) == 3 * sizeof(float) && alignof(Vec3f) == alignof(float),
              "the single-precision row is what a device reads: three floats and nothing else, so "
              "a record that hands it to a shader hands over exactly what it declares");

template <typename Number>
[[nodiscard]] constexpr Vector3<Number> operator+(const Vector3<Number> &a,
                                                  const Vector3<Number> &b) {
  return {{a[0] + b[0], a[1] + b[1], a[2] + b[2]}};
}

template <typename Number>
[[nodiscard]] constexpr Vector3<Number> operator-(const Vector3<Number> &a,
                                                  const Vector3<Number> &b) {
  return {{a[0] - b[0], a[1] - b[1], a[2] - b[2]}};
}

template <typename Number>
[[nodiscard]] constexpr Vector3<Number> operator*(const Vector3<Number> &v, Number by) {
  return {{v[0] * by, v[1] * by, v[2] * by}};
}

template <typename Number>
[[nodiscard]] constexpr Number Dot(const Vector3<Number> &a, const Vector3<Number> &b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

template <typename Number>
[[nodiscard]] constexpr Vector3<Number> Cross(const Vector3<Number> &a, const Vector3<Number> &b) {
  return {{a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}};
}

inline constexpr Vec3 kProofLeft = {{1.0, 2.0, 3.0}};
inline constexpr Vec3 kProofRight = {{4.0, 5.0, 6.0}};

static_assert(Dot(kProofLeft, kProofRight) == kProofLeft[0] * kProofRight[0] +
                                                  kProofLeft[1] * kProofRight[1] +
                                                  kProofLeft[2] * kProofRight[2],
              "the scalar product is checked where it is written, not where it is used");

static_assert(Cross(Vec3{{1.0, 0.0, 0.0}}, Vec3{{0.0, 1.0, 0.0}})[2] == 1.0 &&
                  Cross(Vec3{{0.0, 1.0, 0.0}}, Vec3{{1.0, 0.0, 0.0}})[2] == -1.0,
              "east crossed into up gives south and not its negative -- a swapped sign here turns "
              "every normal in the tree inside out, and this is the one place it can be caught "
              "without running anything");

static_assert((kProofLeft - Vec3{{1.0, 1.0, 1.0}}) * 2.0 == Vec3{{0.0, 2.0, 4.0}},
              "difference and scale compose the way the arithmetic they replace did");

template <std::floating_point Number>
[[nodiscard]] Number Length(const Vector3<Number> &v) noexcept {
  return std::hypot(std::hypot(v[0], v[1]), v[2]);
}

template <std::floating_point Number> [[nodiscard]] bool Normalise(Vector3<Number> &v) noexcept {
  if (!std::isfinite(v[0]) || !std::isfinite(v[1]) || !std::isfinite(v[2])) { return false; }
  const Number scale = std::max({std::abs(v[0]), std::abs(v[1]), std::abs(v[2])});
  if (scale == Number{0}) { return false; }
  Vector3<Number> direction{{v[0] / scale, v[1] / scale, v[2] / scale}};
  const Number length = Length(direction);
  for (Number &component : direction) { component /= length; }
  v = direction;
  return true;
}

}

#endif
