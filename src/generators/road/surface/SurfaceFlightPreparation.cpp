#include "SurfaceFlightPreparation.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Generators {
std::expected<SurfaceFlightPreparation, SurfaceFlightPreparationError>
PrepareSurfaceFlights(const PlanarArrangement &arrangement,
                      uint32_t domainCount,
                      std::span<const SurfaceHeightPort> ports,
                      double joinDistanceM,
                      std::span<const SurfaceHeightRecipe> sources,
                      std::span<const SurfaceFlightEndpoints> flights,
                      SurfaceFlightPreparationBudget budget,
                      const std::stop_token &stop) {
  using Error = SurfaceFlightPreparationError;
  const auto initial = BuildSurfaceHeightTopology(
      arrangement, domainCount, ports, joinDistanceM, budget.Topology, stop);
  if (!initial) { return std::unexpected(Error{initial.error()}); }
  auto plan = BuildSurfaceFlightPlan(*initial, sources, flights, budget.Flights, stop);
  if (!plan) { return std::unexpected(Error{plan.error()}); }
  for (size_t flight = 0; flight < plan->Flights.size(); ++flight) {
    plan->Axes[flight].InitialFaces = initial->Sources[plan->Flights[flight].Source].PlanarFaces;
  }
  auto cuts = CutPlanarStations(
      arrangement.Mesh.Points, arrangement.Mesh.Faces, plan->Axes, budget.Stations, stop);
  if (!cuts) { return std::unexpected(Error{cuts.error()}); }
  PlanarArrangement refined;
  refined.FaceDomains.reserve(cuts->Mesh.FaceParents.size());
  for (const auto parent : cuts->Mesh.FaceParents) {
    assert(parent < arrangement.FaceDomains.size());
    refined.FaceDomains.push_back(arrangement.FaceDomains[parent]);
  }
  refined.Mesh = std::move(cuts->Mesh);
  auto heights =
      BuildSurfaceHeightTopology(refined, domainCount, ports, joinDistanceM, budget.Topology, stop);
  if (!heights) { return std::unexpected(Error{heights.error()}); }
  size_t bindings = 0;
  for (size_t flight = 0; flight < plan->Flights.size(); ++flight) {
    auto &stations = plan->Flights[flight].Stations;
    stations.clear();
    stations.reserve(heights->PlanarVertices.size());
    for (const auto vertex : heights->PlanarVertices) {
      if (stop.stop_requested()) { return std::unexpected(Error{SurfaceFlightError::Canceled}); }
      assert(vertex < cuts->Stations[flight].size());
      stations.push_back(cuts->Stations[flight][vertex]);
      ++bindings;
    }
  }
  return SurfaceFlightPreparation{.Arrangement = std::move(refined),
                                  .Heights = std::move(*heights),
                                  .Plan = std::move(*plan),
                                  .InitialTopologyWork = initial->Work,
                                  .StationWork = cuts->Work,
                                  .StationBindings = bindings};
}
}
