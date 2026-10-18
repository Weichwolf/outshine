#include "WaterSurfaceMesh.h"
#include "Check.h"
#include "Geodesy.h"
#include "TangentFrame.h"
#include "math/Vec3.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

std::vector<Vec3> Plane(double height, double eastGrade, double northGrade, double radius) {
  std::vector<Vec3> vertices;
  for (const auto xy : std::array{std::array{-radius, -radius},
                                  std::array{radius, -radius},
                                  std::array{radius, radius},
                                  std::array{-radius, radius}}) {
    vertices.push_back({{xy[0], xy[1], height + eastGrade * xy[0] + northGrade * xy[1]}});
  }
  return vertices;
}

constexpr std::array<WaterSurfaceMesh::Face, 2> kFaces{
    WaterSurfaceMesh::Face{.Vertices = {0, 1, 2}, .Surface = 7},
    WaterSurfaceMesh::Face{.Vertices = {0, 2, 3}, .Surface = 7}};

void CheckPlane(LongitudeLatitude anchor) {
  constexpr double kHeight = 123.456789123;
  constexpr double kEastGrade = .03, kNorthGrade = -.02;
  const auto mesh = WaterSurfaceMesh::Build(
      anchor, Plane(kHeight, kEastGrade, kNorthGrade, 50000), {kFaces.begin(), kFaces.end()});
  CHECK(mesh, "a finite upward conforming plane has a canonical contact mesh");
  if (!mesh) { return; }
  const auto &index = mesh->Hierarchy();
  const auto restored =
      WaterSurfaceMesh::Restore(anchor,
                                {mesh->VerticesEnuM().begin(), mesh->VerticesEnuM().end()},
                                {mesh->Faces().begin(), mesh->Faces().end()},
                                {index.Nodes().begin(), index.Nodes().end()},
                                {index.SourceOrder().begin(), index.SourceOrder().end()});
  CHECK(restored && restored->Faces()[0].Surface == 7,
        "restoring a canonical mesh retains original surface identity");
  const auto frame = TangentFrame::At(anchor);
  for (const double east : {-.001, 0., .001}) {
    for (const double north : {-.001, 0., .001}) {
      const LongitudeLatitude at{.LongitudeDeg = anchor.LongitudeDeg + east,
                                 .LatitudeDeg = anchor.LatitudeDeg + north};
      const LongitudeLatitudeHeight geographic{.LongitudeDeg = at.LongitudeDeg,
                                               .LatitudeDeg = at.LatitudeDeg};
      const auto sea = frame.ToLocalPosition(geographic);
      const auto up = frame.ToLocalDirection(EnuAxesEcef(geographic).Up);
      const double expected =
          (kHeight + kEastGrade * sea.EastM + kNorthGrade * sea.NorthM - sea.UpM) /
          (up.UpM - kEastGrade * up.EastM - kNorthGrade * up.NorthM);
      const auto level = mesh->LevelAt(at);
      CHECK(level && std::abs(*level - expected) < 1e-8,
            "geodetic vertical intersects the actual facet, including both sides of its seam");
      const auto decoded = restored ? restored->LevelAt(at) : std::nullopt;
      CHECK(decoded && level && *decoded == *level,
            "stored index replay keeps canonical contact heights bitwise identical");
    }
  }
  CHECK(!mesh->LevelAt({.LongitudeDeg = anchor.LongitudeDeg, .LatitudeDeg = 91}),
        "invalid geographic requests do not enter traversal");
  CHECK(
      !mesh->LevelAt({.LongitudeDeg = 0, .LatitudeDeg = std::numeric_limits<double>::quiet_NaN()}),
      "nonfinite coordinates do not produce a contact");
}
}

int main() {
  for (const auto anchor :
       std::array{LongitudeLatitude{},
                  LongitudeLatitude{.LongitudeDeg = 179.99, .LatitudeDeg = 45},
                  LongitudeLatitude{.LongitudeDeg = -71, .LatitudeDeg = -60},
                  LongitudeLatitude{.LongitudeDeg = 120, .LatitudeDeg = 89.8}}) {
    CheckPlane(anchor);
  }
  const auto submerged =
      WaterSurfaceMesh::Build({}, Plane(-20.125, 0, 0, 1000), {kFaces.begin(), kFaces.end()});
  const auto level = submerged ? submerged->LevelAt({}) : std::nullopt;
  CHECK(level && std::abs(*level + 20.125) < 1e-10,
        "water contacts can lie below the reference ellipsoid without a zero-height clamp");
  auto layered = Plane(-20, 0, 0, 1000);
  const auto upper = Plane(7, 0, 0, 1000);
  layered.insert(layered.end(), upper.begin(), upper.end());
  const auto overlapping = WaterSurfaceMesh::Build(
      {},
      std::move(layered),
      {kFaces[0], kFaces[1], {.Vertices = {4, 5, 6}}, {.Vertices = {4, 6, 7}}});
  const auto top = overlapping ? overlapping->LevelAt({}) : std::nullopt;
  CHECK(top && std::abs(*top - 7) < 1e-10, "a contact selects the topmost overlapping surface");
  return Report();
}
