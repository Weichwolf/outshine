#ifndef OUTSHINE_IMPORT_TRANSFORM_H
#define OUTSHINE_IMPORT_TRANSFORM_H

namespace outshine::Gltf {

struct Transform {
  double M[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

  static Transform Identity() { return {}; }

  static Transform
  FromTrs(const double translation[3], const double rotation[4], const double scale[3]);
  static Transform FromColumnMajor(const double m[16]);

  Transform operator*(const Transform &after) const;

  void Point(const double point[3], double out[3]) const;
  void Direction(const double direction[3], double out[3]) const;

  [[nodiscard]] bool Normal(const double normal[3], double out[3]) const;

  [[nodiscard]] double LinearDeterminant() const;

  [[nodiscard]] bool Inverse(Transform &out) const;
};

} // namespace outshine::Gltf
#endif
