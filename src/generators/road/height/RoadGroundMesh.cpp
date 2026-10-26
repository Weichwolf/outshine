#include "RoadGroundMeshInternal.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr size_t kInitialSegments = 8;
constexpr size_t kRoundsMost = 7;

std::expected<std::vector<double>, RoadGroundError>
Heights(const RoadGroundField &source, const RoadPortField &field, const PlanarMesh &mesh) {
  std::vector<uint32_t> owners(mesh.Points.size(), std::numeric_limits<uint32_t>::max());
  for (size_t face = 0; face < mesh.Faces.size(); ++face) {
    const auto patch = source.Faces[mesh.FaceParents[face]].Patch;
    for (const auto vertex : mesh.Faces[face]) { owners[vertex] = std::min(owners[vertex], patch); }
  }
  std::vector<double> heights;
  heights.reserve(mesh.Points.size());
  for (size_t at = 0; at < mesh.Points.size(); ++at) {
    const auto &point = mesh.Points[at];
    const auto sample = field.Evaluate(owners[at], {.EastM = point[0], .NorthM = point[1]});
    if (!sample) { return std::unexpected(RoadGroundError::InvalidGeometry); }
    heights.push_back(sample->HeightM);
  }
  return heights;
}

bool Refine(const RoadGroundField &source,
            const PlanarMesh &mesh,
            const RoadGroundFacetAudit &audit,
            std::span<size_t> segments) {
  std::vector<uint8_t> marked(segments.size());
  for (size_t face = 0; face < audit.Refinement.size(); ++face) {
    if (audit.Refinement[face] != 0) { marked[source.Faces[mesh.FaceParents[face]].Patch] = 1; }
  }
  bool changed = false;
  for (size_t at = 0; at < segments.size(); ++at) {
    if (marked[at] != 0 && source.Patches[at].Parents[0] != source.Patches[at].Parents[1]) {
      segments[at] *= 2;
      changed = true;
    }
  }
  return changed;
}
}

std::expected<RoadGroundMesh, RoadGroundError> BuildRoadGroundMesh(const RoadGroundField &source,
                                                                   const RoadPortField &field,
                                                                   RoadGroundMeshBudget budget,
                                                                   const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(RoadGroundError::Canceled); }
  budget.Vertices = std::min(budget.Vertices, size_t{131072});
  budget.Faces = std::min(budget.Faces, size_t{262144});
  budget.StationsPerPatch = std::min(budget.StationsPerPatch, size_t{512});
  budget.CacheEntries = std::min(budget.CacheEntries, size_t{262144});
  if (const auto admitted = RoadGround::AdmitMeshSource(source, budget); !admitted) {
    return std::unexpected(admitted.error());
  }
  std::vector<size_t> segments(source.Patches.size(), kInitialSegments);
  RoadGroundMeshWork work;
  for (size_t round = 0; round < kRoundsMost; ++round) {
    auto mesh = RoadGround::SliceMesh(source, segments, budget, work, stop);
    if (!mesh) { return std::unexpected(mesh.error()); }
    auto heights = Heights(source, field, *mesh);
    if (!heights) { return std::unexpected(heights.error()); }
    auto audit = AuditRoadGroundFacets(source,
                                       field,
                                       *mesh,
                                       *heights,
                                       {.Vertices = budget.Vertices,
                                        .Faces = budget.Faces,
                                        .ContainerBytes = budget.ContainerBytes},
                                       stop);
    if (!audit) { return std::unexpected(audit.error()); }
    ++work.Rounds;
    if (audit->NumericalQualityMet()) {
      if (const auto coverage = RoadGround::CheckMeshCoverage(source, *mesh, work, stop);
          !coverage) {
        return std::unexpected(coverage.error());
      }
      return RoadGroundMesh{.Surface = std::move(*mesh),
                            .HeightsM = std::move(*heights),
                            .Audit = std::move(*audit),
                            .Work = work};
    }
    if (!Refine(source, *mesh, *audit, segments)) {
      return std::unexpected(RoadGroundError::InvalidGeometry);
    }
  }
  return std::unexpected(RoadGroundError::CapacityExceeded);
}
}
