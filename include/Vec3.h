#ifndef OUTSHINE_VEC3_H
#define OUTSHINE_VEC3_H

#include <array>
#include <cmath>
#include <cstddef>
#include <span>

namespace outshine {

/// A three-component vector, written once and instantiated for each precision the door speaks.
///
/// Precision has one boundary and it is the camera: the scene keeps @ref Vec3 (64-bit) and the
/// renderer hands the device @ref Vec3f (32-bit). The type carries no alignment of its own -- a
/// record that wants its rows on a 128-bit boundary for whole-row NEON loads says `alignas(16)`
/// on the MEMBER, so the padding is that record's decision rather than a tax on every vector.
template <typename Number> struct Vector3 {
  /// The three components, in the order the engine's frame names them: east, up, south.
  std::array<Number, 3> Axis = {Number{0}, Number{0}, Number{0}};

  /// Reads one component.
  [[nodiscard]] constexpr Number operator[](size_t axis) const { return Axis[axis]; }

  /// Reaches one component for writing.
  [[nodiscard]] constexpr Number &operator[](size_t axis) { return Axis[axis]; }

  /// The three components as a fixed-extent view, for an algorithm that takes a row.
  [[nodiscard]] constexpr std::span<const Number, 3> Row() const { return Axis; }

  /// The three components as a writable fixed-extent view.
  [[nodiscard]] constexpr std::span<Number, 3> Row() { return Axis; }

  /// The first component, so a vector reads in a range-for.
  [[nodiscard]] constexpr auto begin() const { return Axis.begin(); }

  /// One past the last component.
  [[nodiscard]] constexpr auto end() const { return Axis.end(); }

  /// The first component, writable.
  [[nodiscard]] constexpr auto begin() { return Axis.begin(); }

  /// One past the last component, writable.
  [[nodiscard]] constexpr auto end() { return Axis.end(); }

  /// The components as contiguous storage, for the one boundary that takes a pointer: the device.
  [[nodiscard]] constexpr const Number *data() const { return Axis.data(); }

  /// The components as writable contiguous storage.
  [[nodiscard]] constexpr Number *data() { return Axis.data(); }
};

/// The scene's vector: 64-bit, because a world position in a `float` is a defect.
using Vec3 = Vector3<double>;

/// The device's vector: 32-bit, because a `double` reaching a shader is a different defect.
using Vec3f = Vector3<float>;

static_assert(sizeof(Vec3) == 3 * sizeof(double) && alignof(Vec3) == alignof(double),
              "a vector is three doubles and nothing else -- alignment is the record's decision");

static_assert(sizeof(Vec3f) == 3 * sizeof(float) && alignof(Vec3f) == alignof(float),
              "the single-precision row is what a device reads: three floats and nothing else, so "
              "a record that hands it to a shader hands over exactly what it declares");

/// Adds two vectors component by component.
template <typename Number>
[[nodiscard]] constexpr Vector3<Number> operator+(const Vector3<Number> &a,
                                                  const Vector3<Number> &b) {
  return {{a[0] + b[0], a[1] + b[1], a[2] + b[2]}};
}

/// Subtracts @p b from @p a component by component.
template <typename Number>
[[nodiscard]] constexpr Vector3<Number> operator-(const Vector3<Number> &a,
                                                  const Vector3<Number> &b) {
  return {{a[0] - b[0], a[1] - b[1], a[2] - b[2]}};
}

/// Scales a vector.
template <typename Number>
[[nodiscard]] constexpr Vector3<Number> operator*(const Vector3<Number> &v, Number by) {
  return {{v[0] * by, v[1] * by, v[2] * by}};
}

/// The scalar product of two rows.
[[nodiscard]] inline double Dot(std::span<const double, 3> a, std::span<const double, 3> b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

/// Writes the vector product of @p a and @p b into @p out.
inline void
Cross(std::span<const double, 3> a, std::span<const double, 3> b, std::span<double, 3> out) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

/// The euclidean length of a row.
[[nodiscard]] inline double Length(std::span<const double, 3> v) {
  return std::sqrt(Dot(v, v));
}

/// Divides a row by its own length, and refuses a row that has none.
[[nodiscard]] inline bool Normalise(std::span<double, 3> v) {
  const double length = Length(v);
  if (!(length > 0.0)) { return false; }
  for (int axis = 0; axis < 3; ++axis) { v[axis] /= length; }
  return true;
}

/// The scalar product of two vectors.
[[nodiscard]] inline double Dot(const Vec3 &a, const Vec3 &b) {
  return Dot(a.Row(), b.Row());
}

/// Writes the vector product of @p a and @p b into @p out.
inline void Cross(const Vec3 &a, const Vec3 &b, Vec3 &out) {
  Cross(a.Row(), b.Row(), out.Row());
}

/// The euclidean length of a vector.
[[nodiscard]] inline double Length(const Vec3 &v) {
  return Length(v.Row());
}

/// Divides a vector by its own length, and refuses a vector that has none.
[[nodiscard]] inline bool Normalise(Vec3 &v) {
  return Normalise(v.Row());
}

} // namespace outshine

#endif
