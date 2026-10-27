#include "GroundPortField.h"
#include "GroundPortTerrain.h"
#include "TransportNetworkSnapshot.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <stop_token>
#include <tuple>
#include <utility>
#include <vector>

namespace outshine::Generators {
GroundPortFieldRequest::GroundPortFieldRequest(GroundPortSourceChain source, RoadGroundStrip strip)
    : Source_(std::move(source)), Strip_(std::move(strip)) {}

std::expected<GroundPortFieldRequest, GroundPortFieldError>
GroundPortFieldRequest::Prepare(const GroundPortSources &sources,
                                World::TransportEdgeId seed,
                                GroundPortChainLimits chainLimits,
                                RoadGroundBudget groundBudget,
                                const std::stop_token &stop) {
  auto chain = sources.Chain(seed, chainLimits, stop);
  if (!chain) { return std::unexpected(GroundPortFieldError{chain.error()}); }
  if (chain->Nodes.size() > groundBudget.AxisPoints ||
      chain->Nodes.size() > groundBudget.ContainerBytes / sizeof(LongitudeLatitude)) {
    return std::unexpected(GroundPortFieldError{RoadGroundError::CapacityExceeded});
  }
  const auto nodes = chain->Source->Topology().Nodes();
  std::vector<LongitudeLatitude> axis;
  axis.reserve(chain->Nodes.size());
  for (const auto at : chain->Nodes) {
    if (stop.stop_requested()) {
      return std::unexpected(GroundPortFieldError{RoadGroundError::Canceled});
    }
    axis.push_back({.LongitudeDeg = nodes[at].LongitudeDeg, .LatitudeDeg = nodes[at].LatitudeDeg});
  }
  auto strip =
      PrepareRoadGroundStrip(axis, chain->WidthM, chain->MaximumGradient, groundBudget, stop);
  if (!strip) { return std::unexpected(GroundPortFieldError{strip.error()}); }
  return GroundPortFieldRequest(std::move(*chain), std::move(*strip));
}

GroundPortField::GroundPortField(Data data) : Data_(std::move(data)) {}

std::expected<GroundPortField, GroundPortFieldError>
GroundPortField::Make(GroundPortFieldRequest request,
                      std::shared_ptr<const GroundPortTerrain> terrain,
                      double maximumPriorChangeM,
                      GroundPortFieldBudget budget,
                      const std::stop_token &stop) {
  if (stop.stop_requested()) {
    return std::unexpected(GroundPortFieldError{RoadGroundError::Canceled});
  }
  if (!terrain) {
    return std::unexpected(GroundPortFieldError{GroundPortTerrainError::MissingTerrain});
  }
  if (!std::isfinite(maximumPriorChangeM) || maximumPriorChangeM <= 0) {
    return std::unexpected(GroundPortFieldError{RoadProfiles::HeightFitError::InvalidInput});
  }
  if (budget.BindingBytes == 0) {
    return std::unexpected(GroundPortFieldError{TransportPlanCodecError::CapacityExceeded});
  }
  if (budget.Mesh.Vertices == 0 || budget.Mesh.Faces == 0 || budget.Mesh.ContainerBytes == 0) {
    return std::unexpected(GroundPortFieldError{RoadGroundError::CapacityExceeded});
  }
  const auto &queries = request.Strip_.TerrainQueries;
  if (queries.size() > budget.Ground.TerrainSamples ||
      queries.size() > budget.Ground.ContainerBytes / sizeof(double)) {
    return std::unexpected(GroundPortFieldError{RoadGroundError::CapacityExceeded});
  }
  std::vector<double> heights;
  heights.reserve(queries.size());
  for (const auto query : queries) {
    if (stop.stop_requested()) {
      return std::unexpected(GroundPortFieldError{RoadGroundError::Canceled});
    }
    const auto height = terrain->At(query);
    if (!height) {
      return std::unexpected(GroundPortFieldError{GroundPortTerrainError::MissingTerrain});
    }
    heights.push_back(*height);
  }
  auto domain = BindRoadGroundField(request.Strip_, heights, budget.Ground, stop);
  if (!domain) { return std::unexpected(GroundPortFieldError{domain.error()}); }
  auto fitted = FitRoadGround(*domain, maximumPriorChangeM, budget.Ground, budget.Fit, stop);
  if (!fitted) { return std::unexpected(GroundPortFieldError{fitted.error()}); }
  auto geometry = BuildRoadGroundMesh(*domain, fitted->Field(), budget.Mesh, stop);
  if (!geometry) { return std::unexpected(GroundPortFieldError{geometry.error()}); }
  const std::array inputs{fitted->RecipeInput, terrain->Input()};
  const auto binding = TransportPlanInputBinding(inputs, budget.BindingBytes);
  if (!binding) { return std::unexpected(GroundPortFieldError{binding.error()}); }
  auto members = request.Source_.SourceEdges;
  const auto edges = request.Source_.Source->Topology().Edges();
  std::ranges::sort(members, [&edges](uint32_t a, uint32_t b) {
    return std::tie(edges[a].Id.PathId, edges[a].Id.SegmentOrdinal) <
           std::tie(edges[b].Id.PathId, edges[b].Id.SegmentOrdinal);
  });
  if (stop.stop_requested()) {
    return std::unexpected(GroundPortFieldError{RoadGroundError::Canceled});
  }
  return GroundPortField({.Source = std::move(request.Source_),
                          .Terrain = std::move(terrain),
                          .Domain = std::move(*domain),
                          .Fit = std::move(*fitted),
                          .Geometry = std::move(*geometry),
                          .ProcedureAndTerrainBinding = *binding,
                          .Members = std::move(members)});
}
}
