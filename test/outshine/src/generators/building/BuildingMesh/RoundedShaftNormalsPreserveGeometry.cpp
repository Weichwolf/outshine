#include "BuildingMesh.h"
#include "BuildingScratch.h"
#include "BuildingSurface.h"
#include "BuildingWallNormals.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <numbers>
#include <span>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr double latitude = 48.0;
  constexpr double longitude = 16.0;
  constexpr double metresPerDegree = 111320.0;
  std::vector<double> ring;
  for (int corner = 0; corner < 8; ++corner) {
    const double angle = corner * std::numbers::pi / 4.0;
    ring.push_back(latitude + 4.0 * std::sin(angle) / metresPerDegree);
    ring.push_back(longitude +
                   4.0 * std::cos(angle) /
                       (metresPerDegree * std::cos(latitude * std::numbers::pi / 180.0)));
  }
  StructurePlan plan;
  plan.RingLatLon = ring;
  plan.HeightM = 80;
  plan.HeightMeasured = true;
  plan.PitchedShare = 0;
  plan.Coarseness = LevelOfDetail::Shell;
  BuildingScratch scratch;
  BuildingMesh mesher;
  Raised curved, flat;
  CHECK(mesher.Mesh(plan, scratch, curved).has_value(), "round closed shaft meshes");
  const auto source = BuildingSurface::Prepare(plan, scratch);
  CHECK(source && source->Shapes().size() == 1 && HasCurvedShaftWalls(source->Shapes().front()),
        "source curvature selects a round service shaft without inventing its usage");
  if (!source || source->Shapes().empty()) { return Report(); }
  plan.Facade = FacadeStyle::Block;
  CHECK(mesher.Mesh(plan, scratch, flat).has_value(), "unrounded reference envelope meshes");
  const auto sameGeometry = [](std::span<const StoredVertex> a,
                               std::span<const uint32_t> ai,
                               std::span<const StoredVertex> b,
                               std::span<const uint32_t> bi) {
    if (ai.size() != bi.size()) { return false; }
    for (size_t at = 0; at < ai.size(); ++at) {
      for (size_t axis = 0; axis < 3; ++axis) {
        if (a[ai[at]].pos[axis] != b[bi[at]].pos[axis]) { return false; }
      }
    }
    return true;
  };
  CHECK(sameGeometry(curved.WallCorners, curved.WallRun, flat.WallCorners, flat.WallRun) &&
            sameGeometry(curved.RoofCorners, curved.RoofRun, flat.RoofCorners, flat.RoofRun),
        "curved shading adds no triangles and preserves all source envelope positions");
  size_t changedNormals = 0;
  for (size_t at = 0; at < curved.WallRun.size(); ++at) {
    const auto &point = curved.WallCorners[curved.WallRun[at]];
    const auto normal = point.norm();
    CHECK(std::isfinite(normal[0]) && std::isfinite(normal[1]) && std::isfinite(normal[2]),
          "every shaft normal remains finite");
    if (point.normWord != flat.WallCorners[flat.WallRun[at]].normWord) { ++changedNormals; }
  }
  CHECK(changedNormals > 0, "curved wall vertices replace faceted envelope normals");
  auto square = source->Shapes().front();
  square.Ring.resize(4);
  CHECK(!HasCurvedShaftWalls(square), "a sparse square shaft is not declared cylindrical");
  auto occupied = source->Shapes().front();
  occupied.OpeningStyle = FacadeStyle::Block;
  CHECK(!HasCurvedShaftWalls(occupied), "occupied towers keep their facade plane normals");
  auto closed = source->Shapes().front();
  closed.OpeningStyle = FacadeStyle::Outbuilding;
  CHECK(HasCurvedShaftWalls(closed),
        "native closed opening plans preserve curvature without source-specific facade classes");
  return Report();
}
