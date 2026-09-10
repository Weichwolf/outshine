#ifndef OUTSHINE_MATH_VEC4_H
#define OUTSHINE_MATH_VEC4_H

#include <array>
#include <cstddef>
#include <span>

namespace outshine {

/// Owned 4-component value; units and coordinate frame are determined by the caller.
/// @tparam Number Component type; arithmetic follows that type without saturation or validation.
/// Views, pointers and references borrow this object's fixed storage until its lifetime ends;
/// assignment changes the observed values without relocating storage. Moving/copying the
/// value does not retarget existing views. Serialize writes with all access to the same value.
/// No allocation or implicit coordinate conversion occurs for the float/double aliases.
template <typename Number> struct Vector4 {
  /// The four components in binding order.
  std::array<Number, 4> Axis = {Number{0}, Number{0}, Number{0}, Number{0}};

  /// Reads one component.
  /// @param axis Component index; requires axis < 4. No bounds check is performed.
  /// @return That component.
  [[nodiscard]] constexpr Number operator[](size_t axis) const { return Axis[axis]; }

  /// Reaches one component for writing.
  /// @param axis Component index; requires axis < 4. No bounds check is performed.
  /// @return A reference to that component.
  [[nodiscard]] constexpr Number &operator[](size_t axis) { return Axis[axis]; }

  /// The four components as a fixed-extent view.
  /// @return A span over all four, in order.
  [[nodiscard]] constexpr std::span<const Number, 4> Row() const { return Axis; }

  /// The four components as a writable fixed-extent view.
  /// @return A writable span over all four, in order.
  [[nodiscard]] constexpr std::span<Number, 4> Row() { return Axis; }

  /// The first component, so a row reads in a range-for.
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

  /// The components as contiguous storage, for the device.
  /// @return A pointer to the first component.
  [[nodiscard]] constexpr const Number *data() const { return Axis.data(); }

  /// The components as writable contiguous storage.
  /// @return A writable pointer to the first component.
  [[nodiscard]] constexpr Number *data() { return Axis.data(); }

  /// Two rows are the same row when their components are.
  /// @return True when all components compare equal; no tolerance is applied.
  [[nodiscard]] constexpr bool operator==(const Vector4 &) const = default;
};

/// Four single-precision components; GPU buffer alignment is specified by the enclosing layout.
using Vec4f = Vector4<float>;

/// Four double-precision components.
using Vec4 = Vector4<double>;

static_assert(sizeof(Vec4f) == 4 * sizeof(float) && alignof(Vec4f) == alignof(float),
              "a device row is four floats and nothing else -- a struct that binds one unpadded "
              "keeps the size its own static_assert states");

static_assert(sizeof(Vec4) == 4 * sizeof(double) && alignof(Vec4) == alignof(double),
              "and four doubles where the scene keeps one");

}

#endif
