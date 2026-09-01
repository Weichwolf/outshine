#ifndef OUTSHINE_MATH_MAT4_H
#define OUTSHINE_MATH_MAT4_H

#include <array>
#include <cstddef>
#include <span>

namespace outshine {

/// A 4x4 transform, stored COLUMN-MAJOR.
///
/// The order is not a preference: glTF states it for `node.matrix`, Metal states it for a bound
/// uniform, and the tree already held these as sixteen loose doubles in that order. Writing it in
/// the member's name is what stops the next reader from transposing a matrix that was already
/// right -- the commonest way a scene ends up mirrored.
template <typename Number> struct Matrix4 {
  /// The sixteen components, four columns of four, column 0 first.
  std::array<Number, 16> Column = {Number{1},
                                   Number{0},
                                   Number{0},
                                   Number{0},
                                   Number{0},
                                   Number{1},
                                   Number{0},
                                   Number{0},
                                   Number{0},
                                   Number{0},
                                   Number{1},
                                   Number{0},
                                   Number{0},
                                   Number{0},
                                   Number{0},
                                   Number{1}};

  /// Reads one component in storage order.
  [[nodiscard]] constexpr Number operator[](size_t at) const { return Column[at]; }

  /// Reaches one component for writing.
  [[nodiscard]] constexpr Number &operator[](size_t at) { return Column[at]; }

  /// The component at @p row of @p column, for a reader who thinks in rows and columns.
  [[nodiscard]] constexpr Number At(size_t row, size_t column) const {
    return Column[column * 4 + row];
  }

  /// The component at @p row of @p column, for writing.
  [[nodiscard]] constexpr Number &At(size_t row, size_t column) { return Column[column * 4 + row]; }

  /// The sixteen components as a fixed-extent view.
  [[nodiscard]] constexpr std::span<const Number, 16> Row() const { return Column; }

  /// The sixteen components as a writable fixed-extent view.
  [[nodiscard]] constexpr std::span<Number, 16> Row() { return Column; }

  /// The components as contiguous storage, for the one boundary that takes a pointer: the device.
  [[nodiscard]] constexpr const Number *data() const { return Column.data(); }

  /// The components as writable contiguous storage.
  [[nodiscard]] constexpr Number *data() { return Column.data(); }

  /// Two transforms are the same transform when their components are.
  [[nodiscard]] constexpr bool operator==(const Matrix4 &) const = default;
};

/// The scene's transform: 64-bit, on the scene side of the camera.
using Mat4 = Matrix4<double>;

/// The device's transform: 32-bit, on the renderer's side of it.
using Mat4f = Matrix4<float>;

static_assert(sizeof(Mat4) == 16 * sizeof(double) && alignof(Mat4) == alignof(double),
              "a transform is sixteen doubles and nothing else");

static_assert(sizeof(Mat4f) == 16 * sizeof(float) && alignof(Mat4f) == alignof(float),
              "and sixteen floats on the device side, which is what a uniform binding reads");

/// Applies @p left after @p right, the way the column-major convention composes them.
template <typename Number>
[[nodiscard]] constexpr Matrix4<Number> operator*(const Matrix4<Number> &left,
                                                  const Matrix4<Number> &right) {
  Matrix4<Number> made;
  for (size_t column = 0; column < 4; ++column) {
    for (size_t row = 0; row < 4; ++row) {
      Number summed = Number{0};
      for (size_t step = 0; step < 4; ++step) {
        summed += left.At(row, step) * right.At(step, column);
      }
      made.At(row, column) = summed;
    }
  }
  return made;
}

static_assert((Mat4{} * Mat4{}) == Mat4{}, "the identity composed with itself is the identity");

} // namespace outshine

#endif
