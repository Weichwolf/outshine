#ifndef OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADGROUNDFACETAUDIT_H
#define OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADGROUNDFACETAUDIT_H

#include "PlanarMesh.h"
#include "RoadGroundStrip.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine::Generators {
struct RoadGroundFacetBudget {
  size_t Vertices = 131072;
  size_t Faces = 262144;
  size_t ContainerBytes = 33554432;
};

struct RoadGroundFacetAudit {
  std::vector<uint8_t> Refinement;
  double MaximumHeightChordErrorM = 0;
  double MaximumNormalizedContinuousGrade = 0;
  double MaximumNormalizedFiniteGrade = 0;
  double MaximumContactGradientJump = 0;
  size_t SharedContactSegments = 0;
  size_t BadFaces = 0;
  size_t ContainerCapacityBytes = 0;

  [[nodiscard]] bool NumericalQualityMet() const noexcept;
};

[[nodiscard]] std::expected<RoadGroundFacetAudit, RoadGroundError>
AuditRoadGroundFacets(const RoadGroundField &source,
                      const RoadPortField &field,
                      const PlanarMesh &mesh,
                      std::span<const double> heightsM,
                      RoadGroundFacetBudget budget = {},
                      const std::stop_token &stop = {});
}
#endif
