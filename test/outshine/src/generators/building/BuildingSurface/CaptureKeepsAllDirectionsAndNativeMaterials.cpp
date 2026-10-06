#include "BuildingMesh.h"
#include "BuildingScratch.h"
#include "Check.h"
#include "Geodesy.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  std::vector<std::array<double, 8>> rings(64);
  std::vector<StructurePlan> plans;
  Vec3 anchor;
  GeoToEcef({.LongitudeDeg = 16, .LatitudeDeg = 48, .HeightM = 100}, anchor);
  const auto axes = EnuAxesEcef({.LongitudeDeg = 16, .LatitudeDeg = 48});
  for (size_t at = 0; at < rings.size(); ++at) {
    const double lat = 48 + static_cast<double>(at / 8) * 0.0002;
    const double lon = 16 + static_cast<double>(at % 8) * 0.0003;
    rings[at] = {lat, lon, lat, lon + 0.00015, lat + 0.0001, lon + 0.00015, lat + 0.0001, lon};
    plans.push_back({.RingLatLon = rings[at],
                     .BaseAslM = 100,
                     .SeatAslM = 100,
                     .FootAslM = 100,
                     .HeightM = 10,
                     .HeightMeasured = true,
                     .AnchorEcef = anchor,
                     .Coarseness = LevelOfDetail::Shell,
                     .RecessedOpenings = false,
                     .PitchedShare = 0.0,
                     .WallColour = Vec3f{{0.4f, 0.3f, 0.2f}}});
  }
  const BuildingMesh mesher;
  BuildingScratch scratch;
  Raised shells;
  for (const auto &plan : plans) {
    CHECK(mesher.Mesh(plan, scratch, shells).has_value(), "native shell comparison builds");
  }
  for (const Vec3 &eye : {axes.East * -1000.0 + axes.Up * 80.0,
                          axes.East * 1100.0 + axes.Up * 80.0,
                          axes.North * -1000.0 + axes.Up * 80.0,
                          axes.North * 1100.0 + axes.Up * 80.0}) {
    auto product = mesher.Project(plans, eye, {.FocalPx = 256, .AllowedErrorPx = 1}, scratch);
    CHECK(product.has_value(), "the source-surface contract captures either side of a district");
    if (!product) { continue; }
    const auto &mesh = product->Mesh;
    CHECK(!mesh.WallRun.empty() && !mesh.RoofRun.empty(),
          "native roofs and facades remain present");
    CHECK(mesh.WallColours.size() == mesh.WallCorners.size() * 4,
          "wall appearance factors stay attached to every captured native vertex");
    CHECK(product->EyeRadiusM == 0, "single-layer captures claim no translation reuse");
    CHECK(mesh.WallRun.size() + mesh.RoofRun.size() < shells.WallRun.size() + shells.RoofRun.size(),
          "hidden source faces do not become triangles in a distant district");
    for (const auto &vertex : mesh.WallCorners) {
      CHECK(std::isfinite(vertex.uv()[0]) && std::isfinite(vertex.uv()[1]),
            "captured facades retain native material coordinates");
    }
    std::printf("shell %zu triangles, captured %zu triangles, %zu wall vertices\n",
                (shells.WallRun.size() + shells.RoofRun.size()) / 3,
                (mesh.WallRun.size() + mesh.RoofRun.size()) / 3,
                mesh.WallCorners.size());
  }
  return Report();
}
