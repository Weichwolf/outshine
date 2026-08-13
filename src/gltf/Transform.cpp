#include "Transform.h"

#include <cmath>
#include <cstring>

namespace outshine::Gltf {

Transform Transform::FromColumnMajor(const double m[16]) {
  Transform t;
  std::memcpy(t.M, m, sizeof t.M);
  return t;
}

Transform Transform::FromTrs(const double translation[3], const double rotation[4],
                             const double scale[3]) {
  const double norm = std::sqrt(rotation[0] * rotation[0] + rotation[1] * rotation[1] +
                                rotation[2] * rotation[2] + rotation[3] * rotation[3]);
  const double unit = (norm > 0.0 && std::isfinite(norm)) ? 1.0 / norm : 1.0;
  const double x = rotation[0] * unit, y = rotation[1] * unit, z = rotation[2] * unit,
               w = rotation[3] * unit;
  const double basis[9] = {
      1 - 2 * (y * y + z * z), 2 * (x * y + z * w),     2 * (x * z - y * w),
      2 * (x * y - z * w),     1 - 2 * (x * x + z * z), 2 * (y * z + x * w),
      2 * (x * z + y * w),     2 * (y * z - x * w),     1 - 2 * (x * x + y * y),
  };
  Transform t;
  for (int column = 0; column < 3; ++column) {
    for (int row = 0; row < 3; ++row) {
      t.M[column * 4 + row] = basis[column * 3 + row] * scale[column];
    }
    t.M[column * 4 + 3] = 0;
  }
  t.M[12] = translation[0];
  t.M[13] = translation[1];
  t.M[14] = translation[2];
  t.M[15] = 1;
  return t;
}

Transform Transform::operator*(const Transform &after) const {
  Transform out;
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      double sum = 0;
      for (int k = 0; k < 4; ++k) { sum += M[k * 4 + row] * after.M[column * 4 + k]; }
      out.M[column * 4 + row] = sum;
    }
  }
  return out;
}

void Transform::Point(const double point[3], double out[3]) const {
  const double w = M[3] * point[0] + M[7] * point[1] + M[11] * point[2] + M[15];
  const double scale = (w != 0.0) ? 1.0 / w : 1.0;
  for (int row = 0; row < 3; ++row) {
    out[row] =
        (M[row] * point[0] + M[4 + row] * point[1] + M[8 + row] * point[2] + M[12 + row]) * scale;
  }
}

double Transform::LinearDeterminant() const {
  return M[0] * (M[5] * M[10] - M[9] * M[6]) - M[4] * (M[1] * M[10] - M[9] * M[2]) +
         M[8] * (M[1] * M[6] - M[5] * M[2]);
}

void Transform::Direction(const double direction[3], double out[3]) const {
  for (int row = 0; row < 3; ++row) {
    out[row] = M[row] * direction[0] + M[4 + row] * direction[1] + M[8 + row] * direction[2];
  }
}

bool Transform::Normal(const double normal[3], double out[3]) const {
  Transform inverted;
  if (!Inverse(inverted)) { return false; }
  /* The transpose of the inverse's linear part, applied: row `r` of the transpose is column `r` of
   * the inverse, so this reads the inverse by rows where `Direction` reads it by columns. */
  for (int row = 0; row < 3; ++row) {
    out[row] = inverted.M[row * 4] * normal[0] + inverted.M[row * 4 + 1] * normal[1] +
               inverted.M[row * 4 + 2] * normal[2];
  }
  return true;
}

bool Transform::Inverse(Transform &out) const {
  /* Cofactor expansion, general: a node may carry any matrix its file wants, including a shear no
   * affine shortcut would survive. */
  const double *m = M;
  double inv[16];
  inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] +
           m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
  inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] -
           m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
  inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] +
           m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
  inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] -
            m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
  inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] -
           m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
  inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] +
           m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
  inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] -
           m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
  inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] +
            m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
  inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] +
           m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
  inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] -
           m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
  inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] +
            m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
  inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] -
            m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
  inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
           m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
  inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] +
           m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
  inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] -
            m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
  inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] +
            m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

  const double determinant = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
  if (!std::isfinite(determinant) || determinant == 0.0) { return false; }
  const double scale = 1.0 / determinant;
  for (int i = 0; i < 16; ++i) { out.M[i] = inv[i] * scale; }
  return true;
}

} // namespace outshine::Gltf
