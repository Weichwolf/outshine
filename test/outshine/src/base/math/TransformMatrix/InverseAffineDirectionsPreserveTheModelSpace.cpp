#include "TransformMatrix.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Mat4 model;
  model[0] = 2;
  model[4] = 1;
  model[5] = 3;
  model[9] = 1;
  model[10] = -4;
  model[12] = 123;
  model[13] = -456;
  model[14] = 789;
  Vec3 local{{99, 99, 99}};
  CHECK(InverseAffineDirection(model, {{7, 14, -20}}, local) && local == Vec3({{2, 3, 5}}),
        "inverse direction handles scale, shear and reflection without translation");
  for (const auto &direction : {Vec3{}, Vec3{{-5, 8, -13}}, Vec3{{0.1, 0.2, 0.3}}}) {
    CHECK(InverseAffineDirection(model, model.TransformDirection(direction), local),
          "forward affine direction has an inverse");
    for (size_t axis = 0; axis < 3; ++axis) {
      CHECK(std::abs(local[axis] - direction[axis]) < 1e-12,
            "roundtrip preserves the independently chosen local vector");
    }
  }
  Mat4 tiny;
  tiny[0] = tiny[5] = tiny[10] = 1e-6;
  CHECK(InverseAffineDirection(tiny, {{1e-6, 2e-6, 3e-6}}, local) &&
            std::abs(local[0] - 1) < 1e-12 && std::abs(local[2] - 3) < 1e-12,
        "small invertible assets are not rejected by an absolute determinant cutoff");
  std::array invalid{model, model, model};
  invalid[0][0] = 0;
  invalid[1][3] = 1;
  invalid[2][0] = std::numeric_limits<double>::infinity();
  for (const auto &matrix : invalid) {
    local = {{99, 99, 99}};
    CHECK(!InverseAffineDirection(matrix, {{1, 2, 3}}, local) && local == Vec3({{99, 99, 99}}),
          "singular, projective and nonfinite inputs preserve the output on rejection");
  }
  return Report();
}
