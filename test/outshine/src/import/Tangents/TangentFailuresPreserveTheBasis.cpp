#include <array>
#include <limits>
#include <vector>
#include "Check.h"
#include "Tangents.h"

int main() {
  using namespace outshine::Gltf;
  using namespace outshine::Test;
  std::array<double, 9> positions{0, 0, 0, 1, 0, 0, 0, 1, 0};
  std::array<double, 9> normals{0, 0, 1, 0, 0, 1, 0, 0, 1};
  std::array<double, 6> uv{0, 0, 1, 0, 0, 1};
  std::array<uint32_t, 3> indices{0, 1, 2};
  const TangentSubject subject{positions, normals, uv, indices};
  std::vector<double> basis;
  CHECK(GenerateTangents(subject, basis).has_value(), "analytic triangle produces a basis");
  CHECK(basis.size() == 12, "one tangent and handedness per corner");
  for (size_t corner = 0; corner < 3; ++corner) {
    CHECK_NEAR(basis[corner * 4], 1, 1e-12, "unit", "tangent follows increasing U");
    CHECK_NEAR(basis[corner * 4 + 1], 0, 1e-12, "unit", "tangent has no Y component");
    CHECK_NEAR(basis[corner * 4 + 2], 0, 1e-12, "unit", "tangent is perpendicular to normal");
    CHECK(basis[corner * 4 + 3] == -1, "existing importer flips V before basis construction");
  }
  const auto saved = basis;
  const auto *storage = basis.data();
  const auto reject = [&](const TangentSubject &invalid) {
    CHECK(!GenerateTangents(invalid, basis), "invalid tangent input is rejected");
    CHECK(basis == saved && basis.data() == storage, "rejection preserves the published basis");
  };
  auto invalid = subject;
  invalid.PositionsM = invalid.PositionsM.first(8);
  reject(invalid);
  invalid = subject;
  invalid.Normals = invalid.Normals.first(6);
  reject(invalid);
  invalid = subject;
  invalid.Uv = invalid.Uv.first(5);
  reject(invalid);
  invalid = subject;
  invalid.Indices = invalid.Indices.first(2);
  reject(invalid);
  reject({});
  indices[2] = 3;
  reject(subject);
  indices[2] = 2;
  for (const double number :
       {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    positions[0] = number;
    reject(subject);
    positions[0] = 0;
    normals[0] = number;
    reject(subject);
    normals[0] = 0;
    uv[0] = number;
    reject(subject);
    uv[0] = 0;
  }
  uv[2] = -1;
  CHECK(GenerateTangents(subject, basis).has_value(), "mirrored UVs produce a basis");
  for (size_t corner = 0; corner < 3; ++corner) {
    CHECK_NEAR(basis[corner * 4], -1, 1e-12, "unit", "mirrored U reverses the tangent");
    CHECK(basis[corner * 4 + 3] == 1, "mirrored UV reverses handedness");
  }
  const std::array<double, 18> seamPositions{0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0};
  const std::array<double, 18> seamNormals{0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
  const std::array<double, 12> seamUv{0, 0, 1, 0, 0, 1, -1, 0, -1, 1, 0, 1};
  const std::array<uint32_t, 9> seamIndices{0, 1, 2, 3, 4, 5, 0, 0, 0};
  CHECK(GenerateTangents({seamPositions, seamNormals, seamUv, seamIndices}, basis).has_value(),
        "mirrored seam and degenerate triangle generate together");
  CHECK(basis.size() == 36, "every seam and degenerate corner receives a basis");
  for (size_t corner = 0; corner < 9; ++corner) {
    const double direction = corner >= 3 && corner < 6 ? -1 : 1;
    CHECK_NEAR(basis[corner * 4],
               direction,
               1e-12,
               "unit",
               "seams retain tangent direction and degenerates inherit");
    CHECK(basis[corner * 4 + 3] == -direction, "seams retain handedness and degenerates inherit");
  }
  std::vector<double> unrelatedPositions(positions.begin(), positions.end());
  std::vector<double> unrelatedNormals(normals.begin(), normals.end());
  std::vector<double> unrelatedUv(uv.begin(), uv.end());
  unrelatedPositions.insert(unrelatedPositions.end(), 3, std::numeric_limits<double>::quiet_NaN());
  unrelatedNormals.insert(unrelatedNormals.end(), 3, std::numeric_limits<double>::quiet_NaN());
  unrelatedUv.insert(unrelatedUv.end(), 2, std::numeric_limits<double>::quiet_NaN());
  CHECK(GenerateTangents({unrelatedPositions, unrelatedNormals, unrelatedUv, indices}, basis)
            .has_value(),
        "a primitive only reads referenced vertices from shared attribute storage");
  return Report();
}
