#include "Check.h"
#include "PlanarStationMesh.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {
using namespace outshine;
constexpr std::array<PlanarPoint, 7> kPoints{
    {{0, 0}, {4, 0}, {4, 4}, {0, 4}, {0, 10}, {4, 10}, {0, 14}}};
constexpr std::array<PlanarFace, 3> kFaces{{{0, 1, 2}, {0, 2, 3}, {4, 5, 6}}};

void CheckParents(const PlanarStationMesh &output) {
  std::array<double, kFaces.size()> areas{};
  size_t remote = 0;
  for (size_t at = 0; at < output.Mesh.Faces.size(); ++at) {
    const auto parent = output.Mesh.FaceParents[at];
    const auto face = output.Mesh.Faces[at];
    const auto a = output.Mesh.Points[face[0]];
    const auto b = output.Mesh.Points[face[1]];
    const auto c = output.Mesh.Points[face[2]];
    const double area = ((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])) / 2;
    CHECK(area > 0 && parent < areas.size(), "every local child keeps a positive owned area");
    if (parent < areas.size()) { areas[parent] += area; }
    if (parent == 2) {
      ++remote;
      CHECK(face == kFaces[2], "an unrelated face crossing the plane remains exactly unchanged");
    }
  }
  for (const auto area : areas) { CHECK(area == 8, "each original triangle keeps its full area"); }
  CHECK(remote == 1, "the distant original face is emitted once");
  CHECK(std::equal(kPoints.begin(), kPoints.end(), output.Mesh.Points.begin()),
        "every immutable input point retains its exact coordinates and index");
  CHECK(std::ranges::none_of(
            output.Mesh.Points,
            [](PlanarPoint point) { return point[0] > 0 && point[0] < 4 && point[1] == 4; }),
        "a neighboring boundary edge is not cut merely because it crosses the plane");
}

void LocalChildrenAndNeighborsShareTheSameNode() {
  const std::array axes{PlanarStationAxis{
      .Direction = {1, 0}, .Cuts = {2}, .InitialFaces = std::vector<uint32_t>{0}}};
  const auto output = CutPlanarStations(kPoints, kFaces, axes);
  CHECK(output && output->Mesh.Points.size() == 9 && output->Mesh.Faces.size() == 6,
        "one local cut adds two edge nodes, three selected children and two neighbor children");
  if (!output) { return; }
  CheckParents(*output);
  const auto point = std::ranges::find(output->Mesh.Points, PlanarPoint{2, 2});
  CHECK(point != output->Mesh.Points.end(), "the common diagonal has one canonical cut node");
  if (point == output->Mesh.Points.end()) { return; }
  const auto node = static_cast<uint32_t>(point - output->Mesh.Points.begin());
  uint32_t owners = 0;
  for (size_t at = 0; at < output->Mesh.Faces.size(); ++at) {
    if (std::ranges::find(output->Mesh.Faces[at], node) != output->Mesh.Faces[at].end()) {
      owners |= uint32_t{1} << output->Mesh.FaceParents[at];
    }
  }
  CHECK(owners == 3, "selected and unselected neighbors reference the same canonical edge node");
  const auto limited = CutPlanarStations(kPoints, kFaces, axes, {.FaceVisits = 3});
  CHECK(!limited && limited.error() == PlanarMeshError::CapacityExceeded,
        "neighbor conformance consumes the same bounded work budget before publication");
}

void InitialSelectionSurvivesCutsAndRejectsInvalidIds() {
  std::array axes{PlanarStationAxis{
      .Direction = {1, 0}, .Cuts = {1, 3}, .InitialFaces = std::vector<uint32_t>{0}}};
  const auto output = CutPlanarStations(kPoints, kFaces, axes);
  CHECK(output.has_value(), "later cuts select descendants by their original face identity");
  if (output) { CheckParents(*output); }
  axes[0].InitialFaces = std::vector<uint32_t>{};
  const auto empty = CutPlanarStations(kPoints, kFaces, axes);
  CHECK(empty && empty->Mesh.Faces == std::vector(kFaces.begin(), kFaces.end()) &&
            empty->Mesh.Points == std::vector(kPoints.begin(), kPoints.end()) &&
            empty->Work.FaceVisits == 0,
        "an explicitly empty selection is a no-op rather than a global cut");
  for (const auto &invalid :
       {std::vector<uint32_t>{3}, std::vector<uint32_t>{0, 0}, std::vector<uint32_t>{1, 0}}) {
    axes[0].InitialFaces = invalid;
    const auto refused = CutPlanarStations(kPoints, kFaces, axes);
    CHECK(!refused && refused.error() == PlanarMeshError::InvalidInput,
          "out-of-range, repeated and unordered initial face identities are refused");
  }
}
}

int main() {
  LocalChildrenAndNeighborsShareTheSameNode();
  InitialSelectionSurvivesCutsAndRejectsInvalidIds();
  return outshine::Test::Report();
}
