#ifndef OUTSHINE_BASE_MATH_AFFINETRANSFORM_H
#define OUTSHINE_BASE_MATH_AFFINETRANSFORM_H

#include "math/Mat4.h"
#include "math/Quat.h"
#include "math/Vec3.h"

namespace outshine {

struct AffineTransform {
  Mat4 M = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};

  static AffineTransform Identity() { return {}; }

  static AffineTransform FromTrs(const Vec3 &translation, const Quat &rotation, const Vec3 &scale);
  static AffineTransform FromColumnMajor(const Mat4 &m);

  AffineTransform operator*(const AffineTransform &after) const;

  void Point(const Vec3 &point, Vec3 &out) const;
  void Direction(const Vec3 &direction, Vec3 &out) const;

  [[nodiscard]] bool Normal(const Vec3 &normal, Vec3 &out) const;

  [[nodiscard]] double LinearDeterminant() const;

  [[nodiscard]] bool Inverse(AffineTransform &out) const;
};

}
#endif
