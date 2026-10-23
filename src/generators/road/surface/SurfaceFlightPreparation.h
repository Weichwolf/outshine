#ifndef OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACEFLIGHTPREPARATION_H
#define OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACEFLIGHTPREPARATION_H

#include "SurfaceFlightPlan.h"
#include "SurfaceHeightTopology.h"
#include "PlanarStationMesh.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <variant>

namespace outshine::Generators {
struct SurfaceFlightPreparationBudget {
  SurfaceHeightTopologyBudget Topology;
  SurfaceFlightBudget Flights;
  PlanarStationBudget Stations;
};

struct SurfaceFlightPreparation {
  PlanarArrangement Arrangement;
  SurfaceHeightTopology Heights;
  SurfaceFlightPlan Plan;
  SurfaceHeightTopologyWork InitialTopologyWork;
  PlanarStationWork StationWork;
  size_t StationBindings = 0;
};

using SurfaceFlightPreparationError =
    std::variant<SurfaceHeightTopologyError, SurfaceFlightError, PlanarMeshError>;

[[nodiscard]] std::expected<SurfaceFlightPreparation, SurfaceFlightPreparationError>
PrepareSurfaceFlights(const PlanarArrangement &arrangement,
                      uint32_t domainCount,
                      std::span<const SurfaceHeightPort> ports,
                      double joinDistanceM,
                      std::span<const SurfaceHeightRecipe> sources,
                      std::span<const SurfaceFlightEndpoints> flights,
                      SurfaceFlightPreparationBudget budget = {},
                      const std::stop_token &stop = {});
}
#endif
