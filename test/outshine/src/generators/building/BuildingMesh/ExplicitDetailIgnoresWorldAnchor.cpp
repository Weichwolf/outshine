#include "src/generators/building/BuildingMesh.h"
#include "Check.h"

#include <array>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  const std::array<double, 8> ring{47.0, 9.0, 47.0, 9.0001, 47.0001, 9.0001, 47.0001, 9.0};
  Generators::BuildingMesh mesher;
  StructurePlan plan;
  plan.RingLatLon = ring;
  plan.HeightM = 12.0;
  plan.HeightMeasured = true;

  std::array<size_t, 2> fineTriangles{};
  std::array<size_t, 2> shellTriangles{};
  for (size_t at = 0; at < 2; ++at) {
    plan.AnchorEcef = at == 0 ? Vec3{{0.0, 0.0, 0.0}} : Vec3{{1000.0, 2000.0, 3000.0}};
    auto scratch = mesher.Scratch();
    Raised fine;
    plan.Coarseness = LevelOfDetail::Fine;
    CHECK(mesher.Mesh(plan, *scratch, fine).has_value(), "explicit Fine building meshes");
    scratch = mesher.Scratch();
    Raised shell;
    plan.Coarseness = LevelOfDetail::Shell;
    CHECK(mesher.Mesh(plan, *scratch, shell).has_value(), "explicit Shell building meshes");
    fineTriangles[at] = (fine.WallRun.size() + fine.RoofRun.size()) / 3u;
    shellTriangles[at] = (shell.WallRun.size() + shell.RoofRun.size()) / 3u;
    CHECK(fineTriangles[at] > shellTriangles[at],
          "Fine retains architectural detail while Shell uses the bounding proxy");
  }
  CHECK(fineTriangles[0] == fineTriangles[1] && shellTriangles[0] == shellTriangles[1],
        "world anchor changes coordinates, not the requested building detail level");
  return Report();
}
