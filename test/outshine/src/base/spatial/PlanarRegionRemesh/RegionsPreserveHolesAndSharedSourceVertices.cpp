#include "PlanarRegionRemesh.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <stop_token>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
const std::array kPoints{PlanarPoint{0, 0},
                         PlanarPoint{1, 0},
                         PlanarPoint{2, 0},
                         PlanarPoint{2, 2},
                         PlanarPoint{0, 2},
                         PlanarPoint{1, 1}};
const std::array kFaces{PlanarFace{0, 1, 5},
                        PlanarFace{1, 2, 5},
                        PlanarFace{2, 3, 5},
                        PlanarFace{3, 4, 5},
                        PlanarFace{4, 0, 5}};
const std::array kRegions{PlanarFaceRegion{0, 1, 2, 3, 4}};

bool Uses(std::span<const PlanarFace> faces, uint32_t vertex) {
  return std::ranges::any_of(
      faces, [vertex](const auto face) { return std::ranges::find(face, vertex) != face.end(); });
}

void RemoveInteriorAndUnsharedCollinearVertices() {
  const auto mesh = RemeshPlanarRegions(kPoints, kFaces, kRegions);
  CHECK(mesh.has_value(), "the region union removes redundant internal edges before refinement");
  if (!mesh) { return; }
  CHECK(mesh->Regions.size() == 1 && mesh->Regions[0].size() == 2,
        "a five-triangle coplanar region needs only two triangles");
  CHECK(!Uses(mesh->Regions[0], 1) && !Uses(mesh->Regions[0], 5),
        "unshared collinear and interior points generate no later intersections");
  CHECK(mesh->Work.RemovedVertices == 1 && mesh->Work.InputFaces == 5 &&
            mesh->Work.OutputFaces == 2 && mesh->Work.CoverageErrorM2 == 0,
        "work counters distinguish boundary simplification from removed interior triangulation");
  std::vector<PlanarPoint> points(kPoints.begin(), kPoints.end());
  points.push_back({1, -1});
  std::vector<PlanarFace> faces(kFaces.begin(), kFaces.end());
  faces.push_back({1, 0, 6});
  const auto shared = RemeshPlanarRegions(points, faces, kRegions);
  CHECK(shared && shared->Regions[0].size() == 3 && Uses(shared->Regions[0], 1),
        "unselected geometry protects its shared collinear source vertex from removal");
}

void HolesAndDisconnectedComponentsRemain() {
  const std::array points{PlanarPoint{0, 0},
                          PlanarPoint{4, 0},
                          PlanarPoint{4, 4},
                          PlanarPoint{0, 4},
                          PlanarPoint{1, 1},
                          PlanarPoint{3, 1},
                          PlanarPoint{3, 3},
                          PlanarPoint{1, 3}};
  const std::array faces{PlanarFace{0, 1, 5},
                         PlanarFace{0, 5, 4},
                         PlanarFace{1, 2, 6},
                         PlanarFace{1, 6, 5},
                         PlanarFace{2, 3, 7},
                         PlanarFace{2, 7, 6},
                         PlanarFace{3, 0, 4},
                         PlanarFace{3, 4, 7}};
  const std::array regions{PlanarFaceRegion{0, 1, 2, 3, 4, 5, 6, 7}};
  const auto ring = RemeshPlanarRegions(points, faces, regions);
  CHECK(ring && ring->Regions[0].size() == 8 && ring->Work.CoverageErrorM2 == 0,
        "constrained retriangulation retains the hole and exact region coverage");
  const std::array islands{PlanarFaceRegion{0, 4}};
  const auto disconnected = RemeshPlanarRegions(points, faces, islands);
  CHECK(disconnected && disconnected->Regions[0].size() == 2,
        "one semantic region may contain multiple disconnected polygons");
}

void RefuseWithoutPublishingPartialRegions() {
  for (size_t which = 0; which < 5; ++which) {
    PlanarRegionBudget budget;
    switch (which) {
      case 0: budget.Points = 1; break;
      case 1: budget.Faces = 1; break;
      case 2: budget.Regions = 0; break;
      case 3: budget.RingVertices = 3; break;
      default: budget.RingVisits = 0; break;
    }
    const auto refused = RemeshPlanarRegions(kPoints, kFaces, kRegions, budget);
    CHECK(!refused && refused.error() == PlanarRegionError::CapacityExceeded,
          "input, output and work budgets refuse the entire remesh");
  }
  const std::array repeated{PlanarFaceRegion{0, 0}};
  const auto overlap = RemeshPlanarRegions(kPoints, kFaces, repeated);
  CHECK(!overlap && overlap.error() == PlanarRegionError::InvalidInput,
        "a source face cannot belong to two selected regions or appear twice");
  std::stop_source stop;
  stop.request_stop();
  const auto canceled = RemeshPlanarRegions(kPoints, kFaces, kRegions, {}, stop.get_token());
  CHECK(!canceled && canceled.error() == PlanarRegionError::Canceled,
        "pre-canceled work never invokes geometry processing");
  auto points = kPoints;
  points[0][0] = std::numeric_limits<double>::quiet_NaN();
  const auto invalid = RemeshPlanarRegions(points, kFaces, kRegions);
  CHECK(!invalid && invalid.error() == PlanarRegionError::InvalidInput,
        "nonfinite coordinates are rejected at the geometry boundary");
  const auto empty = RemeshPlanarRegions({}, {}, {});
  CHECK(empty && empty->Regions.empty(), "no selected regions produce an empty neutral result");
}

void LongStraightChainsHaveBoundedQueueWork() {
  constexpr uint32_t side = 128;
  constexpr uint32_t boundary = 4 * side;
  std::vector<PlanarPoint> points;
  points.reserve(boundary + 1);
  for (uint32_t at = 0; at < side; ++at) { points.push_back({static_cast<double>(at), 0}); }
  for (uint32_t at = 0; at < side; ++at) { points.push_back({side, static_cast<double>(at)}); }
  for (uint32_t at = 0; at < side; ++at) {
    points.push_back({static_cast<double>(side - at), side});
  }
  for (uint32_t at = 0; at < side; ++at) { points.push_back({0, static_cast<double>(side - at)}); }
  points.push_back({side / 2., side / 2.});
  std::vector<PlanarFace> faces;
  PlanarFaceRegion region;
  for (uint32_t at = 0; at < boundary; ++at) {
    faces.push_back({at, (at + 1) % boundary, boundary});
    region.push_back(at);
  }
  const std::array regions{region};
  const auto result = RemeshPlanarRegions(points, faces, regions);
  CHECK(result && result->Work.InputFaces == boundary && result->Work.OutputFaces == 2,
        "long straight boundary chains disappear before any downstream height cuts");
  if (result) {
    CHECK(result->Work.RemovedVertices == boundary - 4 &&
              result->Work.RingVisits <= size_t{3} * boundary,
          "the queue starts once per boundary vertex and adds at most two visits per removal");
  }
}
}

int main() {
  RemoveInteriorAndUnsharedCollinearVertices();
  HolesAndDisconnectedComponentsRemain();
  RefuseWithoutPublishingPartialRegions();
  LongStraightChainsHaveBoundedQueueWork();
  return Report();
}
