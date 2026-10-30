#ifndef OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADGROUNDFACETINTERNAL_H
#define OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADGROUNDFACETINTERNAL_H

#include "RoadGroundFacetAudit.h"
#include <cstdint>
#include <expected>
#include <span>

namespace outshine::Generators::RoadGround {
struct Facet {
  RoadPlane Gradient;
  uint32_t Patch = 0;
};

[[nodiscard]] size_t FacetAuditBytes(size_t faces) noexcept;

[[nodiscard]] std::expected<Facet, RoadGroundError> CheckFacet(const RoadPortFace &source,
                                                               const RoadPortField &field,
                                                               const PlanarFace &indices,
                                                               std::span<const PlanarPoint> points,
                                                               std::span<const double> heightsM,
                                                               RoadGroundFacetAudit &audit,
                                                               size_t at) noexcept;
}
#endif
