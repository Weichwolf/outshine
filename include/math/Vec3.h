#ifndef OUTSHINE_VEC3_H
#define OUTSHINE_VEC3_H

#include <array>
#include <cmath>
#include <cstddef>
#include <span>

namespace outshine {

/// A three-component vector, written once and instantiated for each precision the door speaks.
///
/// Precision has one boundary and it is the camera: the scene keeps `Vec3` (64-bit) and the
/// renderer hands the device `Vec3f` (32-bit). The type carries no alignment of its own -- a
/// record that wants its rows on a 128-bit boundary for whole-row NEON loads says `alignas(16)`
/// on the MEMBER, so the padding is that record's decision rather than a tax on every vector.
template <typename Number> struct Vector3 {
  /// The three components, in the order the engine's frame names them: east, up, south.
  std::array<Number, 3> Axis = {Number{0}, Number{0}, Number{0}};

  /// Reads one component.
  /// @param axis Which axis, counting from 0.
  /// @return That component.
  [[nodiscard]] constexpr Number operator[](size_t axis) const { return Axis[axis]; }

  /// Reaches one component for writing.
  /// @param axis Which axis, counting from 0.
  /// @return A reference to that component.
  [[nodiscard]] constexpr Number &operator[](size_t axis) { return Axis[axis]; }

  /// The three components as a fixed-extent view, for an algorithm that takes a row.
  /// @return A span over all three, in order.
  [[nodiscard]] constexpr std::span<const Number, 3> Row() const { return Axis; }

  /// The three components as a writable fixed-extent view.
  /// @return A writable span over all three, in order.
  [[nodiscard]] constexpr std::span<Number, 3> Row() { return Axis; }

  /// The first component, so a vector reads in a range-for.
  /// @return An iterator to the first component.
  [[nodiscard]] constexpr auto begin() const { return Axis.begin(); }

  /// One past the last component.
  /// @return An iterator one past the last component.
  [[nodiscard]] constexpr auto end() const { return Axis.end(); }

  /// The first component, writable.
  /// @return A writable iterator to the first component.
  [[nodiscard]] constexpr auto begin() { return Axis.begin(); }

  /// One past the last component, writable.
  /// @return A writable iterator one past the last component.
  [[nodiscard]] constexpr auto end() { return Axis.end(); }

  /// The components as contiguous storage, for the one boundary that takes a pointer: the device.
  /// @return A pointer to the first component.
  [[nodiscard]] constexpr const Number *data() const { return Axis.data(); }

  /// The components as writable contiguous storage.
  /// @return A writable pointer to the first component.
  [[nodiscard]] constexpr Number *data() { return Axis.data(); }

  /// Two vectors are the same vector when their components are.
  /// @return True when every component agrees.
  [[nodiscard]] constexpr bool operator==(const Vector3 &) const = default;
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

/// The scalar product of two vectors.
template <typename Number>
[[nodiscard]] constexpr Number Dot(const Vector3<Number> &a, const Vector3<Number> &b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

/// The vector product of @p a and @p b.
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

/// The euclidean length of a vector.
template <typename Number> [[nodiscard]] Number Length(const Vector3<Number> &v) {
  return std::sqrt(Dot(v, v));
}

/// Divides a vector by its own length, and refuses a vector that has none.
template <typename Number> [[nodiscard]] bool Normalise(Vector3<Number> &v) {
  const Number length = Length(v);
  if (!(length > Number{0})) { return false; }
  for (int axis = 0; axis < 3; ++axis) { v[axis] /= length; }
  return true;
}

} // namespace outshine

#endif
