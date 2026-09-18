#include "AffineTransform.h"

#include "math/Mat4.h"
#include "math/TransformMatrix.h"
#include "math/Vec3.h"
#include "math/Quat.h"

#include <cstddef>

namespace outshine {

AffineTransform AffineTransform::FromColumnMajor(const Mat4 &m) {
  return AffineTransform{.M = m};
}

AffineTransform
AffineTransform::FromTrs(const Vec3 &translation, const Quat &rotation, const Vec3 &scale) {
  return AffineTransform{.M = TransformMatrix(translation, rotation, scale)};
}

AffineTransform AffineTransform::operator*(const AffineTransform &after) const {
  return {.M = M * after.M};
}

void AffineTransform::Point(const Vec3 &point, Vec3 &out) const {
  out = M.TransformPoint(point);
}

double AffineTransform::LinearDeterminant() const {
  return M[0] * (M[5] * M[10] - M[9] * M[6]) - M[4] * (M[1] * M[10] - M[9] * M[2]) +
         M[8] * (M[1] * M[6] - M[5] * M[2]);
}

void AffineTransform::Direction(const Vec3 &direction, Vec3 &out) const {
  out = M.TransformDirection(direction);
}

bool AffineTransform::Normal(const Vec3 &normal, Vec3 &out) const {
  AffineTransform inverted;
  if (!Inverse(inverted)) { return false; }

  for (int row = 0; row < 3; ++row) {
    out[row] = inverted.M[static_cast<size_t>(row) * 4] * normal[0] +
               inverted.M[row * 4 + 1] * normal[1] + inverted.M[row * 4 + 2] * normal[2];
  }
  return true;
}

bool AffineTransform::Inverse(AffineTransform &out) const {
  return InverseMatrix(M, out.M);
}

}
