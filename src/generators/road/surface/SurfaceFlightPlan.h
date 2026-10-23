#ifndef OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACEFLIGHTPLAN_H
#define OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACEFLIGHTPLAN_H

#include "SurfaceHeightProgram.h"
#include "PlanarStationMesh.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine::Generators {
struct SurfaceFlightEndpoints {
  uint32_t Source = 0, Start = 0, End = 0;
  int Direction = 1;
};

struct SurfaceFlightBudget {
  static constexpr double kDefaultStraightnessM = 1e-8;
  size_t Vertices = 4096;
  size_t Flights = 16;
  size_t VertexVisits = 262144;
  double StraightnessM = kDefaultStraightnessM;
  double LandingSearchM = 1.0 / 1024;
};

struct SurfaceFlightWork {
  size_t PointProjections = 0;
  size_t VertexVisits = 0;
  size_t GapCandidates = 0;
};

struct SurfaceFlightPlan {
  std::vector<SurfaceHeightFlight> Flights;
  std::vector<PlanarStationAxis> Axes;
  SurfaceFlightWork Work;
};

enum class SurfaceFlightError : uint8_t {
  InvalidInput,
  CapacityExceeded,
  Canceled,
  MissingLanding,
  CurvedAxis,
  NoSlopedRun,
  ConflictingOwners
};

[[nodiscard]] std::expected<SurfaceFlightPlan, SurfaceFlightError>
BuildSurfaceFlightPlan(const SurfaceHeightTopology &topology,
                       std::span<const SurfaceHeightRecipe> sources,
                       std::span<const SurfaceFlightEndpoints> flights,
                       SurfaceFlightBudget budget = {},
                       const std::stop_token &stop = {});
}
#endif
