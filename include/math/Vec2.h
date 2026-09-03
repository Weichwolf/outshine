#ifndef OUTSHINE_MATH_VEC2_H
#define OUTSHINE_MATH_VEC2_H

#include <array>
#include <cstddef>
#include <span>

namespace outshine {

/// A two-component vector: a texture coordinate, a raster point, a pair of half-angles.
template <typename Number> struct Vector2 {
  /// The two components, in the order the reader of the field names them.
  std::array<Number, 2> Axis = {Number{0}, Number{0}};

  /// Reads one component.
  /// @param axis Which axis, counting from 0.
  /// @return That component.
  [[nodiscard]] constexpr Number operator[](size_t axis) const { return Axis[axis]; }

  /// Reaches one component for writing.
  /// @param axis Which axis, counting from 0.
  /// @return A reference to that component.
  [[nodiscard]] constexpr Number &operator[](size_t axis) { return Axis[axis]; }

  /// The two components as a fixed-extent view.
  /// @return A span over all two, in order.
  [[nodiscard]] constexpr std::span<const Number, 2> Row() const { return Axis; }

  /// The two components as a writable fixed-extent view.
  /// @return A writable span over all two, in order.
  [[nodiscard]] constexpr std::span<Number, 2> Row() { return Axis; }

  /// The first component, so a pair reads in a range-for.
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

  /// The components as contiguous storage, for a boundary that takes a pointer.
  /// @return A pointer to the first component.
  [[nodiscard]] constexpr const Number *data() const { return Axis.data(); }

  /// The components as writable contiguous storage.
  /// @return A writable pointer to the first component.
  [[nodiscard]] constexpr Number *data() { return Axis.data(); }

  /// Two pairs are the same pair when their components are.
  [[nodiscard]] constexpr bool operator==(const Vector2 &) const = default;
};

/// The scene's pair: 64-bit.
using Vec2 = Vector2<double>;

/// The device's pair: 32-bit.
using Vec2f = Vector2<float>;

static_assert(sizeof(Vec2) == 2 * sizeof(double) && alignof(Vec2) == alignof(double),
              "a pair is two doubles and nothing else");

static_assert(sizeof(Vec2f) == 2 * sizeof(float) && alignof(Vec2f) == alignof(float),
              "and two floats on the device side, which is what a uv stream reads");

/// Adds two pairs component by component.
template <typename Number>
[[nodiscard]] constexpr Vector2<Number> operator+(const Vector2<Number> &a,
                                                  const Vector2<Number> &b) {
  return {{a[0] + b[0], a[1] + b[1]}};
}

/// Subtracts @p b from @p a component by component.
template <typename Number>
[[nodiscard]] constexpr Vector2<Number> operator-(const Vector2<Number> &a,
                                                  const Vector2<Number> &b) {
  return {{a[0] - b[0], a[1] - b[1]}};
}

/// Scales a pair.
template <typename Number>
[[nodiscard]] constexpr Vector2<Number> operator*(const Vector2<Number> &v, Number by) {
  return {{v[0] * by, v[1] * by}};
}

} // namespace outshine

#endif
