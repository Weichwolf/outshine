#ifndef OUTSHINE_MATH_MAT4_H
#define OUTSHINE_MATH_MAT4_H

#include <array>
#include <cstddef>
#include <span>

#include "math/Vec3.h"

namespace outshine {

/// A 4x4 transform, stored COLUMN-MAJOR.
///
/// The order is not a preference: GLSL and SPIR-V's std140 layout state it for a bound uniform,
/// and the tree already held these as sixteen loose doubles in that order. Writing it in the
/// member's name is what stops the next reader from transposing a matrix that was already right
/// -- the commonest way a scene ends up mirrored.
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
  /// @param at Which of the sixteen, column-major.
  /// @return That component.
  [[nodiscard]] constexpr Number operator[](size_t at) const { return Column[at]; }

  /// Reaches one component for writing.
  /// @param at Which of the sixteen, column-major.
  /// @return A reference to that component.
  [[nodiscard]] constexpr Number &operator[](size_t at) { return Column[at]; }

  /// The component at @p row of @p column, for a reader who thinks in rows and columns.
  /// @param row Which row, 0 to 3.
  /// @param column Which column, 0 to 3.
  /// @return That component.
  [[nodiscard]] constexpr Number At(size_t row, size_t column) const {
    return Column[column * 4 + row];
  }

  /// The component at @p row of @p column, for writing.
  /// @param row Which row, 0 to 3.
  /// @param column Which column, 0 to 3.
  /// @return A reference to that component.
  [[nodiscard]] constexpr Number &At(size_t row, size_t column) { return Column[column * 4 + row]; }

  /// The first component, so a transform reads in a range-for.
  /// @return An iterator to the first of the sixteen.
  [[nodiscard]] constexpr auto begin() const { return Column.begin(); }

  /// One past the last component.
  /// @return An iterator one past the sixteenth.
  [[nodiscard]] constexpr auto end() const { return Column.end(); }

  /// The first component, writable.
  /// @return A writable iterator to the first of the sixteen.
  [[nodiscard]] constexpr auto begin() { return Column.begin(); }

  /// One past the last component, writable.
  /// @return A writable iterator one past the sixteenth.
  [[nodiscard]] constexpr auto end() { return Column.end(); }

  /// The sixteen components as a fixed-extent view.
  /// @return A span over all sixteen, in storage order.
  [[nodiscard]] constexpr std::span<const Number, 16> Row() const { return Column; }

  /// The sixteen components as a writable fixed-extent view.
  /// @return A writable span over all sixteen, in storage order.
  [[nodiscard]] constexpr std::span<Number, 16> Row() { return Column; }

  /// The components as contiguous storage, for the one boundary that takes a pointer: the device.
  /// @return A pointer to the first of the sixteen.
  [[nodiscard]] constexpr const Number *data() const { return Column.data(); }

  /// The components as writable contiguous storage.
  /// @return A writable pointer to the first of the sixteen.
  [[nodiscard]] constexpr Number *data() { return Column.data(); }

  /// Where this transform puts the origin -- its translation, the fourth column.
  ///
  /// Reading it by hand is `[12 + axis]` in a loop, which is how six places in this tree asked
  /// "where does this stand". The column index is storage order and belongs to the type.
  /// @return Where the origin lands under this transform.
  [[nodiscard]] constexpr Vector3<Number> Translation() const {
    return {{Column[12], Column[13], Column[14]}};
  }

  /// Moves this transform's origin to @p at, leaving its rotation and scale as they were.
  /// @param at Where the origin is to land.
  constexpr void SetTranslation(const Vector3<Number> &at) {
    Column[12] = at[0];
    Column[13] = at[1];
    Column[14] = at[2];
  }

  /// @p point through this transform, translation INCLUDED.
  ///
  /// A point and a direction are not the same argument: a direction must not pick up the
  /// translation, or a normal moves when the object does. @ref TransformDirection is the other
  /// one, and a caller that cannot say which it holds does not yet know what it is transforming.
  /// @param point A position in this transform's source space.
  /// @return That position in its target space.
  [[nodiscard]] constexpr Vector3<Number> TransformPoint(const Vector3<Number> &point) const {
    Vector3<Number> out{};
    for (size_t axis = 0; axis < 3; ++axis) {
      out[axis] = Column[axis] * point[0] + Column[4 + axis] * point[1] +
                  Column[8 + axis] * point[2] + Column[12 + axis];
    }
    return out;
  }

  /// @p direction through this transform, translation EXCLUDED.
  /// @param way A direction in this transform's source space.
  /// @return That direction in its target space, unmoved by the translation.
  [[nodiscard]] constexpr Vector3<Number> TransformDirection(const Vector3<Number> &way) const {
    Vector3<Number> out{};
    for (size_t axis = 0; axis < 3; ++axis) {
      out[axis] = Column[axis] * way[0] + Column[4 + axis] * way[1] + Column[8 + axis] * way[2];
    }
    return out;
  }

  /// Two transforms are the same transform when their components are.
  /// @return True when every one of the sixteen agrees.
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
      auto summed = Number{0};
      for (size_t step = 0; step < 4; ++step) {
        summed += left.At(row, step) * right.At(step, column);
      }
      made.At(row, column) = summed;
    }
  }
  return made;
}

static_assert((Mat4{} * Mat4{}) == Mat4{}, "the identity composed with itself is the identity");

constexpr Vec3 kMat4Point{{2.0, 3.0, 5.0}};
constexpr Vec3 kMat4Shift{{10.0, 20.0, 30.0}};

constexpr Mat4 Mat4ShiftedBy(const Vec3 &at) {
  Mat4 out;
  out.SetTranslation(at);
  return out;
}

static_assert(Mat4{}.Translation() == Vec3{}, "the identity stands at the origin");
static_assert(Mat4ShiftedBy(kMat4Shift).Translation() == kMat4Shift,
              "and a transform stands where its translation was set");
static_assert(Mat4{}.TransformPoint(kMat4Point) == kMat4Point, "the identity moves no point");
static_assert(Mat4ShiftedBy(kMat4Shift).TransformPoint(kMat4Point) ==
                  Vec3{{kMat4Point[0] + kMat4Shift[0],
                        kMat4Point[1] + kMat4Shift[1],
                        kMat4Point[2] + kMat4Shift[2]}},
              "a POINT picks the translation up");
static_assert(Mat4ShiftedBy(kMat4Shift).TransformDirection(kMat4Point) == kMat4Point,
              "and a DIRECTION does not -- a normal that moved with its object is the defect this "
              "pair of names exists to make impossible to write by accident");

} // namespace outshine

#endif
