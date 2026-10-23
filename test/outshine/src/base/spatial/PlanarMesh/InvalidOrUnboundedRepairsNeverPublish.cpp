#include "Check.h"
#include "PlanarMesh.h"

#include <array>
#include <cstdint>
#include <limits>
#include <stop_token>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const auto empty = ConformPlanarMesh({}, {}, 0);
  CHECK(empty && empty->Points.empty() && empty->Faces.empty(),
        "an empty local region is a valid neutral product");
  constexpr std::array<PlanarPoint, 3> points{{{0, 0}, {1, 0}, {0, 1}}};
  constexpr std::array<PlanarFace, 1> faces{{{0, 1, 2}}};
  auto invalid = points;
  invalid[1][0] = std::numeric_limits<double>::quiet_NaN();
  CHECK(!ConformPlanarMesh(invalid, faces, 0), "nonfinite vertices cannot enter an index");
  invalid[1][0] = std::numeric_limits<double>::max();
  CHECK(!ConformPlanarMesh(invalid, faces, 0), "overflowing coordinate arithmetic is rejected");
  CHECK(!ConformPlanarMesh(points, faces, -1), "negative weld distance has no implicit default");
  auto malformed = faces;
  malformed[0][2] = std::numeric_limits<uint32_t>::max();
  CHECK(!ConformPlanarMesh(points, malformed, 0), "invalid point indices cannot publish faces");
  PlanarMeshBudget budget;
  budget.Vertices = 2;
  const auto overVertices = ConformPlanarMesh(points, faces, 0, budget);
  CHECK(!overVertices && overVertices.error() == PlanarMeshError::CapacityExceeded,
        "input size is bounded before allocating the point index");
  budget = {};
  budget.Candidates = 0;
  const auto overCandidates = ConformPlanarMesh(points, faces, 0, budget);
  CHECK(!overCandidates && overCandidates.error() == PlanarMeshError::CapacityExceeded,
        "candidate work cannot escape a zero work budget");
  std::stop_source stop;
  stop.request_stop();
  const auto canceled = ConformPlanarMesh(points, faces, 0, {}, stop.get_token());
  CHECK(!canceled && canceled.error() == PlanarMeshError::Canceled,
        "canceled work cannot publish partial topology");
  constexpr std::array<PlanarPoint, 4> chain{{{0, 0}, {0.75, 0}, {1.5, 0}, {0, 2}}};
  constexpr std::array<PlanarFace, 1> source{{{0, 2, 3}}};
  const auto stretched = ConformPlanarMesh(chain, source, 1);
  CHECK(!stretched && stretched.error() == PlanarMeshError::WeldDistanceExceeded,
        "transitive proximity cannot exceed the declared representative displacement");
  constexpr std::array<PlanarPoint, 5> joined{{{0, 0}, {2, 0}, {0, 2}, {2, -2}, {1, 0}}};
  constexpr std::array<PlanarFace, 3> junction{{{0, 1, 2}, {0, 3, 4}, {4, 3, 1}}};
  budget = {};
  budget.Faces = junction.size();
  const auto overFaces = ConformPlanarMesh(joined, junction, 0, budget);
  CHECK(!overFaces && overFaces.error() == PlanarMeshError::CapacityExceeded,
        "a required subdivision cannot overflow the output face budget");
  return Report();
}
