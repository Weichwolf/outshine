#include "Check.h"
#include "PlanarMesh.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace {
using namespace outshine;
using namespace outshine::Test;

double SignedArea(PlanarPoint a, PlanarPoint b, PlanarPoint c) {
  return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
}

void CheckHeightJoin() {
  constexpr std::array<PlanarPoint, 5> points{{{0, 0}, {2, 0}, {0, 2}, {2, -2}, {1, 0}}};
  constexpr std::array<PlanarFace, 3> faces{{{0, 1, 2}, {0, 3, 4}, {4, 3, 1}}};
  const auto mesh = ConformPlanarMesh(points, faces, 0);
  CHECK(mesh.has_value(), "a planar T-interface is constructible");
  if (!mesh) { return; }
  CHECK(mesh->Points.size() == 5 && mesh->Faces.size() == 4,
        "the upper face splits at the existing lower height station");
  CHECK(mesh->Work.SplitEdges == 1 && mesh->Work.Passes == 1,
        "one edge repair reaches a fixed point");
  CHECK(std::ranges::count(mesh->FaceParents, 0u) == 2 &&
            std::ranges::count(mesh->FaceParents, 1u) == 1 &&
            std::ranges::count(mesh->FaceParents, 2u) == 1,
        "subdivision retains every original face owner");
  double area = 0;
  size_t joined = 0;
  for (const auto face : mesh->Faces) {
    area += SignedArea(mesh->Points[face[0]], mesh->Points[face[1]], mesh->Points[face[2]]) / 2;
    for (size_t at = 0; at < 3; ++at) {
      const auto a = face[at];
      const auto b = face[(at + 1) % 3];
      CHECK(std::min(a, b) != 0 || std::max(a, b) != 1,
            "no unsplit long edge crosses the nonaffine height station");
      if ((a == 0 && b == 4) || (a == 4 && b == 0)) { ++joined; }
    }
  }
  CHECK(area == 4 && joined == 2,
        "both sides share the same endpoint heights without adding planar area");
}

void CheckWeldAndGap() {
  constexpr double drift = 1e-10;
  constexpr std::array<PlanarPoint, 6> points{
      {{0, 0}, {2, 0}, {0, 2}, {2, -2}, {1, 0}, {1, drift}}};
  constexpr std::array<PlanarFace, 3> faces{{{0, 1, 2}, {0, 3, 4}, {5, 3, 1}}};
  const auto mesh = ConformPlanarMesh(points, faces, 1e-8);
  CHECK(mesh.has_value(), "near-identical junction stations can share an explicit identity");
  if (mesh) {
    CHECK(mesh->PointRemap[4] == mesh->PointRemap[5] && mesh->Points.size() == 5,
          "the actual weld map is available to the profile consumer");
    CHECK(mesh->Work.MaximumWeldDistance == drift,
          "observed welding displacement is reported without a new geometric error claim");
  }
  constexpr double gap = 1e-6;
  constexpr std::array<PlanarPoint, 6> separate{
      {{0, 0}, {2, 0}, {0, 2}, {0, -gap}, {2, -gap}, {2, -2}}};
  constexpr std::array<PlanarFace, 2> split{{{0, 1, 2}, {3, 5, 4}}};
  const auto apart = ConformPlanarMesh(separate, split, 1e-8);
  CHECK(apart && apart->Points.size() == 6 && apart->Faces.size() == 2,
        "an actual gap beyond the declared welding tolerance remains open");
}

void CheckIndependentOwners() {
  constexpr std::array<PlanarPoint, 6> points{{{0, 0}, {1, 0}, {0, 1}, {0, 0}, {1, 0}, {0, 1}}};
  constexpr std::array<PlanarFace, 2> faces{{{0, 1, 2}, {3, 4, 5}}};
  const auto mesh = ConformPlanarMesh(points, faces, 0);
  CHECK(mesh && mesh->Points.size() == 3 && mesh->Faces.size() == 2,
        "coincident XY data does not erase distinct input surfaces");
  if (mesh) {
    CHECK(mesh->FaceParents[0] == 0 && mesh->FaceParents[1] == 1,
          "independent height owners remain distinguishable after planar sharing");
  }
}
}

int main() {
  CheckHeightJoin();
  CheckWeldAndGap();
  CheckIndependentOwners();
  return Report();
}
