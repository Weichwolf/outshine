#ifndef OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADGROUNDMESH_H
#define OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADGROUNDMESH_H

#include "RoadGroundFacetAudit.h"
#include <cstddef>
#include <expected>
#include <stop_token>
#include <vector>

namespace outshine::Generators {
struct RoadGroundMeshBudget {
  size_t Vertices = 8192;
  size_t Faces = 16384;
  size_t StationsPerPatch = 512;
  size_t CacheEntries = 262144;
  size_t ContainerBytes = 33554432;
};

struct RoadGroundMeshWork {
  size_t Rounds = 0;
  size_t SourceFaceVisits = 0;
  size_t IntervalVisits = 0;
  size_t EdgeQueries = 0;
  size_t NewVertices = 0;
  size_t CopiedSourcePointBytes = 0;
  size_t ContainerCapacityBytes = 0;
  double MaximumRootAreaErrorM2 = 0;
  double CoverageErrorM2 = 0;
};

struct RoadGroundMesh {
  PlanarMesh Surface;
  std::vector<double> HeightsM;
  RoadGroundFacetAudit Audit;
  RoadGroundMeshWork Work;
};

[[nodiscard]] std::expected<RoadGroundMesh, RoadGroundError>
BuildRoadGroundMesh(const RoadGroundField &source,
                    const RoadPortField &field,
                    RoadGroundMeshBudget budget = {},
                    const std::stop_token &stop = {});
}
#endif
