#ifndef OUTSHINE_BASE_MATH_VEC3_H
#define OUTSHINE_BASE_MATH_VEC3_H

#include <array>
#include <cmath>
#include <cstddef>
#include <span>

namespace outshine {

struct Vec3 {
  std::array<double, 3> Axis = {0.0, 0.0, 0.0};

  [[nodiscard]] constexpr double operator[](size_t axis) const { return Axis[axis]; }

  [[nodiscard]] constexpr double &operator[](size_t axis) { return Axis[axis]; }

  [[nodiscard]] constexpr std::span<const double, 3> Row() const { return Axis; }

  [[nodiscard]] constexpr std::span<double, 3> Row() { return Axis; }

  [[nodiscard]] constexpr const double *data() const { return Axis.data(); }

  [[nodiscard]] constexpr double *data() { return Axis.data(); }
};

static_assert(sizeof(Vec3) == 3 * sizeof(double) && alignof(Vec3) == alignof(double),
              "a vector is three doubles and nothing else -- where a record wants its rows on a "
              "128-bit boundary for whole-row NEON loads it says alignas(16) on the MEMBER, so the "
              "alignment is a decision of the record rather than a tax on every vector");

[[nodiscard]] constexpr Vec3 operator+(const Vec3 &a, const Vec3 &b) {
  return {{a[0] + b[0], a[1] + b[1], a[2] + b[2]}};
}

[[nodiscard]] constexpr Vec3 operator-(const Vec3 &a, const Vec3 &b) {
  return {{a[0] - b[0], a[1] - b[1], a[2] - b[2]}};
}

[[nodiscard]] constexpr Vec3 operator*(const Vec3 &v, double by) {
  return {{v[0] * by, v[1] * by, v[2] * by}};
}

struct Vec3f {
  std::array<float, 3> Axis = {0.0f, 0.0f, 0.0f};

  [[nodiscard]] constexpr float operator[](size_t axis) const { return Axis[axis]; }

  [[nodiscard]] constexpr float &operator[](size_t axis) { return Axis[axis]; }

  [[nodiscard]] constexpr std::span<const float, 3> Row() const { return Axis; }

  [[nodiscard]] constexpr std::span<float, 3> Row() { return Axis; }

  [[nodiscard]] constexpr const float *data() const { return Axis.data(); }

  [[nodiscard]] constexpr float *data() { return Axis.data(); }
};

static_assert(sizeof(Vec3f) == 3 * sizeof(float) && alignof(Vec3f) == alignof(float),
              "the single-precision row is what a device reads; it is three floats and nothing "
              "else, so a record that hands it to a shader hands over exactly what it declares");

[[nodiscard]] inline double Dot(std::span<const double, 3> a, std::span<const double, 3> b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

inline void
Cross(std::span<const double, 3> a, std::span<const double, 3> b, std::span<double, 3> out) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

[[nodiscard]] inline double Length(std::span<const double, 3> v) {
  return std::sqrt(Dot(v, v));
}

[[nodiscard]] inline double Dot(const Vec3 &a, const Vec3 &b) {
  return Dot(a.Row(), b.Row());
}

inline void Cross(const Vec3 &a, const Vec3 &b, Vec3 &out) {
  Cross(a.Row(), b.Row(), out.Row());
}

[[nodiscard]] inline double Length(const Vec3 &v) {
  return Length(v.Row());
}

[[nodiscard]] inline bool Normalise(std::span<double, 3> v) {
  const double length = Length(v);
  if (!(length > 0.0)) { return false; }
  for (int axis = 0; axis < 3; ++axis) { v[axis] /= length; }
  return true;
}

[[nodiscard]] inline bool Normalise(Vec3 &v) {
  return Normalise(v.Row());
}

} // namespace outshine

#endif
