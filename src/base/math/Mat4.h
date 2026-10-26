#ifndef OUTSHINE_BASE_MATH_MAT4_H
#define OUTSHINE_BASE_MATH_MAT4_H

#include <array>
#include <cstddef>
#include <span>

#include "Vec3.h"

namespace outshine {

template <typename Number> struct Matrix4 {

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

  [[nodiscard]] constexpr Number operator[](size_t at) const { return Column[at]; }

  [[nodiscard]] constexpr Number &operator[](size_t at) { return Column[at]; }

  [[nodiscard]] constexpr Number At(size_t row, size_t column) const {
    return Column[column * 4 + row];
  }

  [[nodiscard]] constexpr Number &At(size_t row, size_t column) { return Column[column * 4 + row]; }

  [[nodiscard]] constexpr auto begin() const { return Column.begin(); }

  [[nodiscard]] constexpr auto end() const { return Column.end(); }

  [[nodiscard]] constexpr auto begin() { return Column.begin(); }

  [[nodiscard]] constexpr auto end() { return Column.end(); }

  [[nodiscard]] constexpr std::span<const Number, 16> Elements() const { return Column; }

  [[nodiscard]] constexpr std::span<Number, 16> Elements() { return Column; }

  [[nodiscard]] constexpr const Number *data() const { return Column.data(); }

  [[nodiscard]] constexpr Number *data() { return Column.data(); }

  [[nodiscard]] constexpr Vector3<Number> Translation() const {
    return {{Column[12], Column[13], Column[14]}};
  }

  constexpr void SetTranslation(const Vector3<Number> &at) {
    Column[12] = at[0];
    Column[13] = at[1];
    Column[14] = at[2];
  }

  [[nodiscard]] constexpr Vector3<Number> TransformPoint(const Vector3<Number> &point) const {
    Vector3<Number> out{};
    for (size_t axis = 0; axis < 3; ++axis) {
      out[axis] = Column[axis] * point[0] + Column[4 + axis] * point[1] +
                  Column[8 + axis] * point[2] + Column[12 + axis];
    }
    return out;
  }

  [[nodiscard]] constexpr Vector3<Number> TransformDirection(const Vector3<Number> &way) const {
    Vector3<Number> out{};
    for (size_t axis = 0; axis < 3; ++axis) {
      out[axis] = Column[axis] * way[0] + Column[4 + axis] * way[1] + Column[8 + axis] * way[2];
    }
    return out;
  }

  [[nodiscard]] constexpr bool operator==(const Matrix4 &) const = default;
};

using Mat4 = Matrix4<double>;

using Mat4f = Matrix4<float>;

static_assert(sizeof(Mat4) == 16 * sizeof(double) && alignof(Mat4) == alignof(double),
              "a transform is sixteen doubles and nothing else");

static_assert(sizeof(Mat4f) == 16 * sizeof(float) && alignof(Mat4f) == alignof(float),
              "and sixteen floats on the device side, which is what a uniform binding reads");

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

}

#endif
