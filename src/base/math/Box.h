#ifndef OUTSHINE_MATH_BOX_H
#define OUTSHINE_MATH_BOX_H

#include <algorithm>
#include <cstddef>
#include <limits>

#include "Mat4.h"
#include "Vec3.h"

namespace outshine {

template <typename Number> struct BoxOf {

  static constexpr Number kBeyond = std::numeric_limits<Number>::infinity();

  Vector3<Number> Min = {{kBeyond, kBeyond, kBeyond}};
  Vector3<Number> Max = {{-kBeyond, -kBeyond, -kBeyond}};

  constexpr void Cover(const Vector3<Number> &point) {
    for (size_t axis = 0; axis < 3; ++axis) {
      Min[axis] = std::min(Min[axis], point[axis]);
      Max[axis] = std::max(Max[axis], point[axis]);
    }
  }

  constexpr void Cover(const BoxOf &other) {
    if (other.Empty()) { return; }
    Cover(other.Min);
    Cover(other.Max);
  }

  [[nodiscard]] constexpr bool Empty() const { return !(Min[0] <= Max[0]); }

  [[nodiscard]] constexpr bool Holds(const Vector3<Number> &point) const {
    for (size_t axis = 0; axis < 3; ++axis) {
      if (!(point[axis] >= Min[axis]) || !(point[axis] <= Max[axis])) { return false; }
    }
    return true;
  }

  [[nodiscard]] constexpr Vector3<Number> Span() const {
    if (Empty()) { return {}; }
    return {{Max[0] - Min[0], Max[1] - Min[1], Max[2] - Min[2]}};
  }

  [[nodiscard]] constexpr Vector3<Number> Middle() const {
    if (Empty()) { return {}; }
    const Number half = Number{1} / Number{2};
    return {{(Min[0] + Max[0]) * half, (Min[1] + Max[1]) * half, (Min[2] + Max[2]) * half}};
  }

  [[nodiscard]] constexpr Number HalfArea() const {
    const Vector3<Number> span = Span();
    return span[0] * span[1] + span[1] * span[2] + span[2] * span[0];
  }

  [[nodiscard]] constexpr Vector3<Number> Corner(unsigned which) const {
    return {{((which & 1u) != 0) ? Max[0] : Min[0],
             ((which & 2u) != 0) ? Max[1] : Min[1],
             ((which & 4u) != 0) ? Max[2] : Min[2]}};
  }

  [[nodiscard]] constexpr BoxOf Through(const Matrix4<Number> &placed) const {
    if (Empty()) { return {}; }
    BoxOf out;
    for (unsigned which = 0; which < 8u; ++which) {
      out.Cover(placed.TransformPoint(Corner(which)));
    }
    return out;
  }

  [[nodiscard]] constexpr bool operator==(const BoxOf &) const = default;
};

using Box = BoxOf<double>;
using Boxf = BoxOf<float>;

}
#endif
