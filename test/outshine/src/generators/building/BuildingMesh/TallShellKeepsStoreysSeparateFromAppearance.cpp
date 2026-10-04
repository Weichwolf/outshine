#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "src/generators/building/BuildingMesh.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array<double, 8> ring{47.0, 9.0, 47.0, 9.0003, 47.0003, 9.0003, 47.0003, 9.0};
  StructurePlan plan;
  plan.RingLatLon = ring;
  plan.HeightM = 400.0;
  plan.HeightMeasured = true;
  plan.PitchedShare = 0.0;
  plan.Coarseness = LevelOfDetail::Shell;
  Generators::BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  Raised result;
  CHECK(mesher.Mesh(plan, *scratch, result).has_value(), "a tall measured shell meshes");
  float bottom = std::numeric_limits<float>::infinity();
  float top = -std::numeric_limits<float>::infinity();
  size_t wallTriangles = 0;
  for (size_t at = 0; at + 2 < result.WallRun.size(); at += 3) {
    const auto a = result.WallCorners[result.WallRun[at]].uv();
    const auto b = result.WallCorners[result.WallRun[at + 1]].uv();
    const auto c = result.WallCorners[result.WallRun[at + 2]].uv();
    if (a[0] < 0.0f || b[0] < 0.0f || c[0] < 0.0f) { continue; }
    ++wallTriangles;
    const float group = std::floor(a[0] / 256.0f);
    CHECK(group == std::floor(b[0] / 256.0f) && group == std::floor(c[0] / 256.0f),
          "facade appearance is constant over every tall wall triangle");
    bottom = std::min({bottom, a[1], b[1], c[1]});
    top = std::max({top, a[1], b[1], c[1]});
  }
  CHECK(wallTriangles > 0, "the tall shell retains window-bearing walls");
  CHECK(bottom >= 0.0f && bottom <= 1.1f,
        "ground storeys do not contain an appearance-dependent vertical offset");
  CHECK(top - bottom > 64.0f, "the shell exercises the old 64-storey alias boundary");
  return Report();
}
