#ifndef OUTSHINE_BASE_MATH_VEC4_H
#define OUTSHINE_BASE_MATH_VEC4_H

#include <array>
#include <cstddef>
#include <span>

namespace outshine {

template <typename Number> struct Vector4 {

  std::array<Number, 4> Axis = {Number{0}, Number{0}, Number{0}, Number{0}};

  [[nodiscard]] constexpr Number operator[](size_t axis) const { return Axis[axis]; }

  [[nodiscard]] constexpr Number &operator[](size_t axis) { return Axis[axis]; }

  [[nodiscard]] constexpr std::span<const Number, 4> Row() const { return Axis; }

  [[nodiscard]] constexpr std::span<Number, 4> Row() { return Axis; }

  [[nodiscard]] constexpr auto begin() const { return Axis.begin(); }

  [[nodiscard]] constexpr auto end() const { return Axis.end(); }

  [[nodiscard]] constexpr auto begin() { return Axis.begin(); }

  [[nodiscard]] constexpr auto end() { return Axis.end(); }

  [[nodiscard]] constexpr const Number *data() const { return Axis.data(); }

  [[nodiscard]] constexpr Number *data() { return Axis.data(); }

  [[nodiscard]] constexpr bool operator==(const Vector4 &) const = default;
};

using Vec4f = Vector4<float>;

using Vec4 = Vector4<double>;

static_assert(sizeof(Vec4f) == 4 * sizeof(float) && alignof(Vec4f) == alignof(float),
              "a device row is four floats and nothing else -- a struct that binds one unpadded "
              "keeps the size its own static_assert states");

static_assert(sizeof(Vec4) == 4 * sizeof(double) && alignof(Vec4) == alignof(double),
              "and four doubles where the scene keeps one");

}

#endif
