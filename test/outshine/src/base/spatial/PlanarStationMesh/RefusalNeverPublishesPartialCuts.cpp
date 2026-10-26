#include "Check.h"
#include "PlanarStationMesh.h"
#include <array>
#include <cstddef>
#include <expected>
#include <limits>
#include <stop_token>

namespace {
using namespace outshine;
using namespace outshine::Test;

void Rejected(const std::expected<PlanarStationMesh, PlanarMeshError> &result,
              PlanarMeshError reason) {
  CHECK(!result && result.error() == reason, "refusal exposes no partial station mesh");
}
}

int main() {
  const std::array points{PlanarPoint{0, 0}, PlanarPoint{4, 0}, PlanarPoint{0, 4}};
  const std::array<PlanarFace, 1> faces{{{0, 1, 2}}};
  const std::array axes{PlanarStationAxis{.Direction = {1, 0}, .Cuts = {1, 3}}};
  const auto empty = CutPlanarStations({}, {}, {});
  CHECK(empty && empty->Mesh.Points.empty() && empty->Stations.empty(),
        "empty input has the neutral empty result");
  const auto unchanged = CutPlanarStations(points, faces, {});
  CHECK(unchanged && unchanged->Mesh.Faces.size() == 1 && unchanged->Mesh.Points.size() == 3,
        "an empty cut list preserves the whole input");
  auto invalidPoints = points;
  invalidPoints[0][0] = std::numeric_limits<double>::quiet_NaN();
  Rejected(CutPlanarStations(invalidPoints, faces, axes), PlanarMeshError::InvalidInput);
  auto invalidFaces = faces;
  invalidFaces[0][2] = 3;
  Rejected(CutPlanarStations(points, invalidFaces, axes), PlanarMeshError::InvalidInput);
  invalidFaces[0] = {0, 2, 1};
  Rejected(CutPlanarStations(points, invalidFaces, axes), PlanarMeshError::InvalidInput);
  auto badAxis = axes;
  badAxis[0].Direction = {};
  Rejected(CutPlanarStations(points, faces, badAxis), PlanarMeshError::InvalidInput);
  badAxis = axes;
  badAxis[0].Cuts[0] = std::numeric_limits<double>::infinity();
  Rejected(CutPlanarStations(points, faces, badAxis), PlanarMeshError::InvalidInput);
  for (size_t which = 0; which < 6; ++which) {
    PlanarStationBudget budget;
    switch (which) {
      case 0: budget.Vertices = 2; break;
      case 1: budget.Vertices = 3; break;
      case 2: budget.Faces = 1; break;
      case 3: budget.Axes = 0; break;
      case 4: budget.Cuts = 1; break;
      default: budget.FaceVisits = 0; break;
    }
    Rejected(CutPlanarStations(points, faces, axes, budget), PlanarMeshError::CapacityExceeded);
  }
  std::stop_source canceled;
  canceled.request_stop();
  Rejected(CutPlanarStations(points, faces, axes, {}, canceled.get_token()),
           PlanarMeshError::Canceled);
  CHECK(points[0] == PlanarPoint({0, 0}) && faces[0] == PlanarFace({0, 1, 2}),
        "neither successful nor refused calls mutate original source geometry");
  return Report();
}
