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
#include <optional>
#include <stop_token>
#include <tuple>
#include <utility>
#include <vector>

namespace outshine::Generators {
GroundPortFieldRequest::GroundPortFieldRequest(GroundPortSourceChain source,
                                               RoadGroundStrip strip,
                                               std::optional<GroundPortFieldScopeInfo> scope)
    : Source_(std::move(source)), Strip_(std::move(strip)), Scope_(std::move(scope)) {}

std::expected<GroundPortFieldRequest, GroundPortFieldError>
GroundPortFieldRequest::Prepare(const GroundPortSources &sources,
                                World::TransportEdgeId seed,
                                GroundPortChainLimits chainLimits,
                                RoadGroundBudget groundBudget,
                                const std::stop_token &stop) {
  auto chain = sources.Chain(seed, chainLimits, stop);
  if (!chain) { return std::unexpected(GroundPortFieldError{chain.error()}); }
  return PrepareSource(std::move(*chain), {}, groundBudget, stop);
}

std::expected<GroundPortFieldRequest, GroundPortFieldError> GroundPortFieldRequest::PrepareScoped(
    GroundPortFieldScope scope, RoadGroundBudget groundBudget, const std::stop_token &stop) {
  if (stop.stop_requested()) {
    return std::unexpected(GroundPortFieldError{RoadGroundError::Canceled});
  }
  if (!scope.Source.Source || scope.Info.Input.DatasetId.empty() ||
      scope.Info.Input.Version.empty() || scope.Info.Input.ItemId.empty() ||
      scope.Info.Input.Revision != scope.Source.Source->SourceIdentity().Revision) {
    return std::unexpected(GroundPortFieldError{GroundPortSourceError::InvalidSource});
  }
  const auto &input = scope.Info.Input;
  size_t bytes = scope.Info.Exits.capacity() * sizeof(GroundPortFieldExit) +
                 scope.Info.Ends.capacity() * sizeof(TransportSourceEndTopology) +
                 input.DatasetId.capacity() + input.Version.capacity() + input.Revision.capacity() +
                 input.ItemId.capacity() + 4;
  for (const auto &end : scope.Info.Ends) {
    bytes += end.Relations.capacity() * sizeof(TransportSourceMember);
  }
  if (bytes > groundBudget.ContainerBytes) {
    return std::unexpected(GroundPortFieldError{RoadGroundError::CapacityExceeded});
  }
  groundBudget.ContainerBytes -= bytes;
  return PrepareSource(std::move(scope.Source), std::move(scope.Info), groundBudget, stop);
}

std::expected<GroundPortFieldRequest, GroundPortFieldError>
GroundPortFieldRequest::PrepareSource(GroundPortSourceChain source,
                                      std::optional<GroundPortFieldScopeInfo> scope,
                                      RoadGroundBudget groundBudget,
                                      const std::stop_token &stop) {
  if (!source.Source || source.SourceEdges.empty() ||
      source.Nodes.size() != source.SourceEdges.size() + 1) {
    return std::unexpected(GroundPortFieldError{GroundPortSourceError::InvalidSource});
  }
  if (source.Nodes.size() > groundBudget.AxisPoints ||
      source.Nodes.size() > groundBudget.ContainerBytes / sizeof(LongitudeLatitude)) {
    return std::unexpected(GroundPortFieldError{RoadGroundError::CapacityExceeded});
  }
  const auto nodes = source.Source->Topology().Nodes();
  std::vector<LongitudeLatitude> axis;
  axis.reserve(source.Nodes.size());
  for (const auto at : source.Nodes) {
    if (stop.stop_requested()) {
      return std::unexpected(GroundPortFieldError{RoadGroundError::Canceled});
    }
    if (at >= nodes.size()) {
      return std::unexpected(GroundPortFieldError{GroundPortSourceError::InvalidSource});
    }
    axis.push_back({.LongitudeDeg = nodes[at].LongitudeDeg, .LatitudeDeg = nodes[at].LatitudeDeg});
  }
  auto strip =
      PrepareRoadGroundStrip(axis, source.WidthM, source.MaximumGradient, groundBudget, stop);
  if (!strip) { return std::unexpected(GroundPortFieldError{strip.error()}); }
  return GroundPortFieldRequest(std::move(source), std::move(*strip), std::move(scope));
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
  if (domain->TriangleAxisSegments.empty() ||
      static_cast<size_t>(domain->TriangleAxisSegments.back()) + 1 !=
          request.Source_.SourceEdges.size()) {
    return std::unexpected(GroundPortFieldError{RoadGroundError::InvalidGeometry});
  }
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
                          .Members = std::move(members),
                          .Scope = std::move(request.Scope_)});
}

std::expected<World::TransportEdgeId, RoadGroundError>
GroundPortField::SourceEdgeOfFace(size_t face) const noexcept {
  if (face >= Data_.Geometry.Surface.FaceParents.size()) {
    return std::unexpected(RoadGroundError::InvalidGeometry);
  }
  const uint32_t original = Data_.Geometry.Surface.FaceParents[face];
  if (original >= Data_.Domain.TriangleAxisSegments.size()) {
    return std::unexpected(RoadGroundError::InvalidGeometry);
  }
  const uint32_t segment = Data_.Domain.TriangleAxisSegments[original];
  if (segment >= Data_.Source.SourceEdges.size()) {
    return std::unexpected(RoadGroundError::InvalidGeometry);
  }
  const auto edges = Data_.Source.Source->Topology().Edges();
  const uint32_t edge = Data_.Source.SourceEdges[segment];
  if (edge >= edges.size()) { return std::unexpected(RoadGroundError::InvalidGeometry); }
  return edges[edge].Id;
}

}
