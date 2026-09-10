#include <scene/Geometry.h>
#include "Check.h"
#include <array>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Geometry geometry;
  const int part = geometry.addPart("triangle", {});
  constexpr std::array positions{0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
  constexpr std::array indices{0u, 1u, 2u};
  CHECK(geometry.setPositions(part, positions) && geometry.setTriangles(part, indices),
        "mesh builds");
  Mat4 previous;
  previous.SetTranslation({{3, 5, 7}});
  CHECK(geometry.setPlacement(part, previous).has_value(), "initial placement accepted");
  const auto *storage = geometry.positionsOf(part).data();
  const auto reject = [&](const Mat4 &matrix) {
    const auto result = geometry.setPlacement(part, matrix);
    CHECK(!result && result.error() == PlacementError::InvalidTransform,
          "invalid placement rejected");
    CHECK(geometry.placementOf(part) == previous && geometry.positionsOf(part).data() == storage &&
              geometry.wellFormed(),
          "failure preserves placement and borrowed mesh data");
  };
  for (size_t component = 0; component < 16; ++component) {
    for (const double value : {std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity()}) {
      Mat4 invalid;
      invalid[component] = value;
      reject(invalid);
    }
  }
  for (size_t column = 0; column < 4; ++column) {
    Mat4 projective;
    projective.At(3, column) = column == 3 ? 2.0 : 0.01;
    reject(projective);
  }
  for (const double scale : {0.0, -2.0, 3.0}) {
    Mat4 valid;
    valid.At(0, 0) = scale;
    valid.At(0, 1) = 0.5;
    valid.SetTranslation({{1000000, -25, 8}});
    CHECK(geometry.setPlacement(part, valid) && geometry.placementOf(part) == valid,
          "zero scale, reflection, shear and large translation remain valid");
    CHECK(geometry.positionsOf(part).data() == storage && geometry.positionsOf(part)[3] == 1,
          "placement does not bake or relocate mesh attributes");
  }
  for (const int absent : {-1, geometry.parts()}) {
    const auto result = geometry.setPlacement(absent, {});
    CHECK(!result && result.error() == PlacementError::MissingPart, "absent parts rejected");
  }
  return Report();
}
