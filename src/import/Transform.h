#ifndef OUTSHINE_IMPORT_TRANSFORM_H
#define OUTSHINE_IMPORT_TRANSFORM_H

#include "Quat.h"
#include "Vec3.h"

namespace outshine::Gltf {

struct Transform {
  double M[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

  static Transform Identity() { return {}; }

  static Transform FromTrs(const Vec3 &translation, const Quat &rotation, const Vec3 &scale);
  static Transform FromColumnMajor(const double m[16]);

  Transform operator*(const Transform &after) const;

  void Point(const Vec3 &point, Vec3 &out) const;
  void Direction(const Vec3 &direction, Vec3 &out) const;

  [[nodiscard]] bool Normal(const Vec3 &normal, Vec3 &out) const;

  [[nodiscard]] double LinearDeterminant() const;

  [[nodiscard]] bool Inverse(Transform &out) const;
};

} // namespace outshine::Gltf
#endif
