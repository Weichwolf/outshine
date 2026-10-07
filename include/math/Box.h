#ifndef OUTSHINE_MATH_BOX_H
#define OUTSHINE_MATH_BOX_H

#include <algorithm>
#include <cstddef>
#include <limits>

#include "math/Mat4.h"
#include "math/Vec3.h"

namespace outshine {

/// Axis-aligned bounds in the caller's coordinate frame; Number is a floating-point scalar.
/// Empty bounds use an infinity sentinel; nonempty endpoints are finite and ordered on every axis.
/// Copies allocate nothing; mutation requires exclusive access.
template <typename Number> struct BoxOf {
  /// Positive infinity used by the default empty bounds.
  static constexpr Number kBeyond = std::numeric_limits<Number>::infinity();

  Vector3<Number> Min = {{kBeyond, kBeyond, kBeyond}};    ///< Inclusive lower endpoints.
  Vector3<Number> Max = {{-kBeyond, -kBeyond, -kBeyond}}; ///< Inclusive upper endpoints.

  /// Grow the bounds to contain a finite point in the same frame.
  /// @param point Finite point in the box's coordinate frame.
  constexpr void Cover(const Vector3<Number> &point) {
    for (size_t axis = 0; axis < 3; ++axis) {
      Min[axis] = std::min(Min[axis], point[axis]);
      Max[axis] = std::max(Max[axis], point[axis]);
    }
  }

  /// Grow the bounds to contain another box; empty input leaves them unchanged.
  /// @param other Bounds in the same coordinate frame, or the canonical empty sentinel.
  constexpr void Cover(const BoxOf &other) {
    if (other.Empty()) { return; }
    Cover(other.Min);
    Cover(other.Max);
  }

  /// @return Whether the box is empty; nonempty endpoints must be ordered on every axis.
  [[nodiscard]] constexpr bool Empty() const { return !(Min[0] <= Max[0]); }

  /// Test closed containment; empty bounds and nonfinite points cannot contain one another.
  /// @param point Point in the box's coordinate frame.
  /// @return Whether all coordinates lie within the inclusive endpoints.
  [[nodiscard]] constexpr bool Holds(const Vector3<Number> &point) const {
    for (size_t axis = 0; axis < 3; ++axis) {
      if (!(point[axis] >= Min[axis]) || !(point[axis] <= Max[axis])) { return false; }
    }
    return true;
  }

  /// @return Per-axis extent in the input's units; zero for empty bounds.
  [[nodiscard]] constexpr Vector3<Number> Span() const {
    if (Empty()) { return {}; }
    return {{Max[0] - Min[0], Max[1] - Min[1], Max[2] - Min[2]}};
  }

  /// @return Centre in the input frame; the zero vector for empty bounds.
  [[nodiscard]] constexpr Vector3<Number> Middle() const {
    if (Empty()) { return {}; }
    const Number half = Number{1} / Number{2};
    return {{(Min[0] + Max[0]) * half, (Min[1] + Max[1]) * half, (Min[2] + Max[2]) * half}};
  }

  /// @return Sum of the three face areas in squared input units; zero for empty bounds.
  [[nodiscard]] constexpr Number HalfArea() const {
    const Vector3<Number> span = Span();
    return span[0] * span[1] + span[1] * span[2] + span[2] * span[0];
  }

  /// Select a nonempty box's corner using the low three bits, one per axis.
  /// @param which Three-bit choice of upper rather than lower endpoints.
  /// @return Selected corner in the box's coordinate frame.
  [[nodiscard]] constexpr Vector3<Number> Corner(unsigned which) const {
    return {{((which & 1u) != 0) ? Max[0] : Min[0],
             ((which & 2u) != 0) ? Max[1] : Min[1],
             ((which & 4u) != 0) ? Max[2] : Min[2]}};
  }

  /// Conservative bounds of all eight corners after an affine transform; empty stays empty.
  /// @param placed Finite affine transform from the box's frame to the output frame.
  /// @return Conservative bounds in the output frame, or empty for empty input.
  [[nodiscard]] constexpr BoxOf Through(const Matrix4<Number> &placed) const {
    if (Empty()) { return {}; }
    BoxOf out;
    for (unsigned which = 0; which < 8u; ++which) {
      out.Cover(placed.TransformPoint(Corner(which)));
    }
    return out;
  }

  /// @return Exact endpoint equality, including the default empty sentinel.
  [[nodiscard]] constexpr bool operator==(const BoxOf &) const = default;
};

using Box = BoxOf<double>; ///< Double world-space bounds.
using Boxf = BoxOf<float>; ///< Float bounds, usually relative to a local origin.

}
#endif
