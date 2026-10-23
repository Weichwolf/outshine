#include "Check.h"
#include "PlanarStationMesh.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

double Area(const PlanarStationMesh &mesh) {
  double sum = 0;
  for (const auto face : mesh.Mesh.Faces) {
    const auto a = mesh.Mesh.Points[face[0]];
    const auto b = mesh.Mesh.Points[face[1]];
    const auto c = mesh.Mesh.Points[face[2]];
    const double area = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
    CHECK(area > 0, "every returned subtriangle has positive winding and finite nonzero area");
    sum += area / 2;
  }
  return sum;
}

void RepeatedCutIsStable() {
  const std::array points{PlanarPoint{0, 0}, PlanarPoint{4, 0}, PlanarPoint{0, 4}};
  const std::array<PlanarFace, 1> faces{{{0, 1, 2}}};
  const std::array once{PlanarStationAxis{.Direction = {1, 0}, .Cuts = {1}}};
  auto twice = once;
  twice[0].Cuts.push_back(1);
  const auto a = CutPlanarStations(points, faces, once);
  const auto b = CutPlanarStations(points, faces, twice);
  CHECK(a && b, "cuts through a boundary vertex remain valid");
  if (a && b) {
    CHECK(a->Mesh.Points == b->Mesh.Points && a->Mesh.Faces == b->Mesh.Faces &&
              a->Mesh.FaceParents == b->Mesh.FaceParents && a->Stations == b->Stations,
          "a repeated identical plane adds no vertex, face or station drift");
    CHECK(Area(*a) == 8, "clipping preserves the full original triangle footprint");
  }
}

void IndependentAxesKeepInterpolatedStations() {
  const std::array points{
      PlanarPoint{0, 0}, PlanarPoint{4, 0}, PlanarPoint{4, 4}, PlanarPoint{0, 4}};
  const std::array faces{PlanarFace{0, 1, 2}, PlanarFace{0, 2, 3}};
  const std::array axes{PlanarStationAxis{.Direction = {1, 0}, .Cuts = {1, 3}},
                        PlanarStationAxis{.Direction = {0, 1}, .Cuts = {2}}};
  const auto mesh = CutPlanarStations(points, faces, axes);
  CHECK(mesh && mesh->Stations.size() == 2, "both independently oriented station axes survive");
  if (!mesh) { return; }
  CHECK(Area(*mesh) == 16, "all original coverage survives consecutive independent cuts");
  for (size_t at = 0; at < mesh->Mesh.Points.size(); ++at) {
    CHECK(mesh->Stations[0][at] == mesh->Mesh.Points[at][0] &&
              mesh->Stations[1][at] == mesh->Mesh.Points[at][1],
          "new edge vertices interpolate every existing axis rather than reprojecting them");
  }
}
}

int main() {
  RepeatedCutIsStable();
  IndependentAxesKeepInterpolatedStations();
  const std::array points{
      PlanarPoint{0, 0}, PlanarPoint{4, 0}, PlanarPoint{4, 4}, PlanarPoint{0, 4}};
  const std::array faces{PlanarFace{0, 1, 2}, PlanarFace{0, 2, 3}};
  const std::array cuts{PlanarStationAxis{.Direction = {1, 0}, .Cuts = {2}}};
  const auto mesh = CutPlanarStations(points, faces, cuts);
  CHECK(mesh && mesh->Mesh.Points.size() == 7 && mesh->Mesh.Faces.size() == 6,
        "the common diagonal is split once, with no duplicate seam vertex");
  if (!mesh) { return Report(); }
  CHECK(mesh->Mesh.PointRemap == std::vector<uint32_t>({0, 1, 2, 3}),
        "all original input coordinates and identities remain available unchanged");
  CHECK(std::equal(points.begin(), points.end(), mesh->Mesh.Points.begin()),
        "station cuts never move a fixed original profile vertex");
  CHECK(Area(*mesh) == 16 && mesh->Work.EdgeIntersections == 3 && mesh->Work.FaceVisits == 2,
        "actual intersection and visited-face counts match the two-triangle cut");
  for (size_t at = 0; at < mesh->Mesh.Faces.size(); ++at) {
    const auto parent = mesh->Mesh.FaceParents[at];
    CHECK(parent < faces.size(), "every subface retains its original planar parent");
    const auto face = mesh->Mesh.Faces[at];
    const auto values = std::array{
        mesh->Stations[0][face[0]], mesh->Stations[0][face[1]], mesh->Stations[0][face[2]]};
    CHECK(std::ranges::all_of(values, [](double x) { return x <= 2; }) ||
              std::ranges::all_of(values, [](double x) { return x >= 2; }),
          "every returned triangle stays entirely on one side of the station plane");
  }
  return Report();
}
