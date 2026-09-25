#include <array>
#include <algorithm>
#include <cmath>
#include "src/generators/building/BuildingMesh.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array<double, 8> ring{47.0, 9.0, 47.0, 9.02, 47.0001, 9.02, 47.0001, 9.0};
  StructurePlan plan;
  plan.RingLatLon = ring;
  plan.HeightM = 9.0;
  plan.HeightMeasured = true;
  plan.Coarseness = LevelOfDetail::Shell;
  Generators::BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  Raised result;
  CHECK(mesher.Mesh(plan, *scratch, result).has_value(), "long simplified building meshes");
  CHECK(!result.WallCorners.empty(), "simplified building retains a wall stream");
  if (result.WallCorners.empty()) { return Report(); }
  size_t facadeTriangles = 0;
  float widestBaySpan = 0.0f;
  float widestStoreySpan = 0.0f;
  for (const StoredVertex &vertex : result.WallCorners) {
    const auto encoded = vertex.uv();
    CHECK(std::isfinite(encoded[0]) && std::isfinite(encoded[1]),
          "every simplified wall coordinate is finite");
  }
  for (size_t at = 0; at + 2 < result.WallRun.size(); at += 3) {
    const auto a = result.WallCorners[result.WallRun[at]].uv();
    const auto b = result.WallCorners[result.WallRun[at + 1]].uv();
    const auto c = result.WallCorners[result.WallRun[at + 2]].uv();
    if (a[0] < 0.0f || b[0] < 0.0f || c[0] < 0.0f) { continue; }
    ++facadeTriangles;
    const float leastBay = std::min({a[0], b[0], c[0]});
    const float mostBay = std::max({a[0], b[0], c[0]});
    const float leastFloor = std::min({a[1], b[1], c[1]});
    const float mostFloor = std::max({a[1], b[1], c[1]});
    widestBaySpan = std::max(widestBaySpan, mostBay - leastBay);
    widestStoreySpan = std::max(widestStoreySpan, mostFloor - leastFloor);
    CHECK(std::floor(leastBay / 256.0f) == std::floor(mostBay / 256.0f),
          "one wall never crosses into another facade style");
  }
  CHECK(facadeTriangles > 0 && widestBaySpan > 1.0f, "simplified box walls retain repeating bays");
  CHECK(widestStoreySpan > 1.0f, "simplified box walls retain storey height");
  return Report();
}
