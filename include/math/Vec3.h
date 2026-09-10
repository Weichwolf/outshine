#ifndef OUTSHINE_VEC3_H
#define OUTSHINE_VEC3_H

#include <array>
#include <algorithm>
#include <concepts>
#include <cmath>
#include <cstddef>
#include <span>

namespace outshine {

/// Owned 3-component value; units and coordinate frame are determined by the caller.
/// @tparam Number Component type; arithmetic follows that type without saturation or validation.
/// Views, pointers and references borrow this object's fixed storage until its lifetime ends;
/// assignment changes the observed values without relocating storage. Moving/copying the
/// value does not retarget existing views. Serialize writes with all access to the same value.
/// No allocation or implicit coordinate conversion occurs for the float/double aliases.
template <typename Number> struct Vector3 {
  /// The x, y, z components in the coordinate frame of the value.
  std::array<Number, 3> Axis = {Number{0}, Number{0}, Number{0}};

  /// Reads one component.
  /// @param axis Component index; requires axis < 3. No bounds check is performed.
  /// @return That component.
  [[nodiscard]] constexpr Number operator[](size_t axis) const { return Axis[axis]; }

  /// Reaches one component for writing.
  /// @param axis Component index; requires axis < 3. No bounds check is performed.
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
  /// @return True when all components compare equal; no tolerance is applied.
  [[nodiscard]] constexpr bool operator==(const Vector3 &) const = default;
};

/// Double-precision vector, suitable for world-space positions.
using Vec3 = Vector3<double>;

/// Single-precision vector, suitable for camera-relative device data.
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

/// Euclidean magnitude in the input units, without avoidable intermediate overflow/underflow.
/// @tparam Number Floating-point component type.
/// @param v Borrowed vector, unchanged. No coordinate-system conversion occurs.
/// @return Nonnegative magnitude; infinity if the magnitude is unrepresentable or any
///         component is infinite. Otherwise a NaN component produces NaN.
/// @note Constant work, no allocation. Concurrent reads of immutable vectors are safe.
template <std::floating_point Number>
[[nodiscard]] Number Length(const Vector3<Number> &v) noexcept {
  return std::hypot(std::hypot(v[0], v[1]), v[2]);
}

/// Replace a finite nonzero vector by its unit direction in the same coordinate frame.
/// @tparam Number Floating-point component type.
/// @param v Exclusively borrowed vector; no references are retained.
/// @return True on success. Zero or nonfinite input returns false without mutation.
///         The result is dimensionless; unit length is subject to floating-point rounding.
/// @note Constant work and no allocation. Requires normal IEEE floating-point semantics;
///       flushing subnormals to zero can remove a representable direction.
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
