#ifndef OUTSHINE_MATH_VEC4_H
#define OUTSHINE_MATH_VEC4_H

#include <array>
#include <cstddef>
#include <span>

namespace outshine {

/// A four-component row.
///
/// This is the DEVICE's shape rather than the scene's: a uniform block binds `float4` rows and a
/// three-component value padded to four is what a shader reads. A quaternion is NOT one of these
/// -- it is @ref Quat, because a rotation whose components can be indexed is a rotation somebody
/// will index in the wrong order.
template <typename Number> struct Vector4 {
  /// The four components in binding order.
  std::array<Number, 4> Axis = {Number{0}, Number{0}, Number{0}, Number{0}};

  /// Reads one component.
  [[nodiscard]] constexpr Number operator[](size_t axis) const { return Axis[axis]; }

  /// Reaches one component for writing.
  [[nodiscard]] constexpr Number &operator[](size_t axis) { return Axis[axis]; }

  /// The four components as a fixed-extent view.
  [[nodiscard]] constexpr std::span<const Number, 4> Row() const { return Axis; }

  /// The four components as a writable fixed-extent view.
  [[nodiscard]] constexpr std::span<Number, 4> Row() { return Axis; }

  /// The first component, so a row reads in a range-for.
  [[nodiscard]] constexpr auto begin() const { return Axis.begin(); }

  /// One past the last component.
  [[nodiscard]] constexpr auto end() const { return Axis.end(); }

  /// The first component, writable.
  [[nodiscard]] constexpr auto begin() { return Axis.begin(); }

  /// One past the last component, writable.
  [[nodiscard]] constexpr auto end() { return Axis.end(); }

  /// The components as contiguous storage, for the device.
  [[nodiscard]] constexpr const Number *data() const { return Axis.data(); }

  /// The components as writable contiguous storage.
  [[nodiscard]] constexpr Number *data() { return Axis.data(); }

  /// Two rows are the same row when their components are.
  [[nodiscard]] constexpr bool operator==(const Vector4 &) const = default;
};

/// The device's row: 32-bit, the shape a uniform block binds.
using Vec4f = Vector4<float>;

/// The scene's row, where one is needed on the far side of the camera.
using Vec4 = Vector4<double>;

static_assert(sizeof(Vec4f) == 4 * sizeof(float) && alignof(Vec4f) == alignof(float),
              "a device row is four floats and nothing else -- a struct that binds one unpadded "
              "keeps the size its own static_assert states");

static_assert(sizeof(Vec4) == 4 * sizeof(double) && alignof(Vec4) == alignof(double),
              "and four doubles where the scene keeps one");

} // namespace outshine

#endif
