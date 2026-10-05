#include <array>
#include <cmath>
#include <limits>

#include "src/generators/building/BuildingMesh.h"
#include "src/generators/building/BuildingMaterials.h"
#include "src/generators/building/FacadeUv.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const std::array<double, 8> ring{47.0, 9.0, 47.0, 9.0003, 47.0003, 9.0003, 47.0003, 9.0};
  StructurePlan plan;
  plan.RingLatLon = ring;
  plan.HeightM = 9.0;
  plan.HeightMeasured = true;
  plan.Coarseness = LevelOfDetail::Shell;
  BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  Raised result;
  CHECK(mesher.Mesh(plan, *scratch, result).has_value(), "an unknown-colour shell meshes");
  CHECK(result.WallColours.empty(), "unknown paint allocates no native colour stream");
  const size_t earlier = result.WallCorners.size();
  plan.WallColour = Vec3f{{0.3f, 0.4f, 0.5f}};
  CHECK(mesher.Mesh(plan, *scratch, result).has_value(), "a painted shell appends");
  CHECK(result.WallColours.size() == result.WallCorners.size() * 4,
        "native factors cover every wall vertex after mixed appends");
  for (size_t at = 0; at < earlier * 4; ++at) {
    CHECK(result.WallColours[at] == 1.0f, "earlier walls keep neutral factors");
  }
  size_t painted = 0;
  for (size_t at = earlier; at < result.WallCorners.size(); ++at) {
    const auto uv = result.WallCorners[at].uv();
    if (uv[0] < 0.0f) { continue; }
    ++painted;
    CHECK(std::floor(uv[0] / 256.0f) < 24.0f, "supplied paint replaces the fallback palette");
    for (size_t channel = 0; channel < 3; ++channel) {
      CHECK_NEAR(result.WallColours[at * 4 + channel] * kBuildingWallColour[channel],
                 (*plan.WallColour)[channel],
                 0.000001,
                 "linear paint",
                 "native factors reproduce the supplied paint");
    }
    CHECK(result.WallColours[at * 4 + 3] == 1.0f, "wall paint does not change opacity");
  }
  CHECK(painted > 0, "the fixture exercises painted facades");
  plan.Coarseness = LevelOfDetail::Fine;
  Raised near;
  CHECK(mesher.Mesh(plan, *scratch, near).has_value(), "painted near housing meshes");
  size_t glazing = 0;
  for (size_t at = 0; at < near.WallCorners.size(); ++at) {
    if (!IsGlazingUv(near.WallCorners[at].uv()[0])) { continue; }
    ++glazing;
    for (size_t channel = 0; channel < 4; ++channel) {
      CHECK(near.WallColours[at * 4 + channel] == 1.0f,
            "supplied wall paint leaves framed glazing unchanged");
    }
  }
  CHECK(glazing > 0, "paint fixture exercises near glazing coordinates");
  const auto saved = result.WallColours;
  (*plan.WallColour)[0] = std::numeric_limits<float>::quiet_NaN();
  CHECK(!mesher.Mesh(plan, *scratch, result),
        "nonfinite paint is rejected at the generator boundary");
  CHECK(result.WallColours == saved, "failed appends preserve earlier paint");
  return Report();
}
