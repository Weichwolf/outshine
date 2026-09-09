#include "math/Mat4.h"
#include "math/TransformMatrix.h"
#include "math/Vec3.h"
#include "Transform.h"
#include "math/Quat.h"

#include <array>
#include <cmath>
#include <cstring>

namespace outshine::Gltf {

Transform Transform::FromColumnMajor(const Mat4 &m) {
  return Transform{.M = m};
}

Transform Transform::FromTrs(const Vec3 &translation, const Quat &rotation, const Vec3 &scale) {
  return Transform{.M = TransformMatrix(translation, rotation, scale)};
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

void Transform::Point(const Vec3 &point, Vec3 &out) const {
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

void Transform::Direction(const Vec3 &direction, Vec3 &out) const {
  for (int row = 0; row < 3; ++row) {
    out[row] = M[row] * direction[0] + M[4 + row] * direction[1] + M[8 + row] * direction[2];
  }
}

bool Transform::Normal(const Vec3 &normal, Vec3 &out) const {
  Transform inverted;
  if (!Inverse(inverted)) { return false; }

  for (int row = 0; row < 3; ++row) {
    out[row] = inverted.M[static_cast<size_t>(row) * 4] * normal[0] +
               inverted.M[row * 4 + 1] * normal[1] + inverted.M[row * 4 + 2] * normal[2];
  }
  return true;
}

bool Transform::Inverse(Transform &out) const {
  return InverseMatrix(M, out.M);
}

}
