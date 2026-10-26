#include "RoadGroundFacetInternal.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <tuple>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr double kContactGradientJumpMost = .005;

struct Edge {
  uint32_t First = 0;
  uint32_t Last = 0;
  uint32_t Face = 0;
  bool Forward = false;
};

auto Key(const Edge &edge) noexcept {
  return std::tie(edge.First, edge.Last);
}

std::expected<void, RoadGroundError> Contacts(std::span<const RoadGround::Facet> facets,
                                              std::span<Edge> edges,
                                              RoadGroundFacetAudit &audit,
                                              const std::stop_token &stop) {
  std::ranges::sort(edges, [](const Edge &a, const Edge &b) { return Key(a) < Key(b); });
  for (size_t first = 0; first < edges.size();) {
    if (stop.stop_requested()) { return std::unexpected(RoadGroundError::Canceled); }
    size_t last = first + 1;
    while (last < edges.size() && Key(edges[last]) == Key(edges[first])) { ++last; }
    if (last - first > 2 ||
        (last - first == 2 && edges[first].Forward == edges[first + 1].Forward)) {
      return std::unexpected(RoadGroundError::InvalidGeometry);
    }
    if (last - first == 2) {
      const auto &a = facets[edges[first].Face];
      const auto &b = facets[edges[first + 1].Face];
      if (a.Patch != b.Patch) {
        const double jump = std::hypot(a.Gradient.SlopeE - b.Gradient.SlopeE,
                                       a.Gradient.SlopeN - b.Gradient.SlopeN);
        ++audit.SharedContactSegments;
        audit.MaximumContactGradientJump = std::max(audit.MaximumContactGradientJump, jump);
        if (jump > kContactGradientJumpMost) {
          audit.Refinement[edges[first].Face] = 1;
          audit.Refinement[edges[first + 1].Face] = 1;
        }
      }
    }
    first = last;
  }
  return {};
}
}

bool RoadGroundFacetAudit::NumericalQualityMet() const noexcept {
  return !Refinement.empty() && BadFaces == 0;
}

std::expected<RoadGroundFacetAudit, RoadGroundError>
AuditRoadGroundFacets(const RoadGroundField &source,
                      const RoadPortField &field,
                      const PlanarMesh &mesh,
                      std::span<const double> heightsM,
                      RoadGroundFacetBudget budget,
                      const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(RoadGroundError::Canceled); }
  const size_t count = mesh.Faces.size();
  const size_t bytesPerFace = sizeof(RoadGround::Facet) + 3 * sizeof(Edge) + sizeof(uint8_t);
  if (count > std::min(budget.Faces, size_t{262144}) ||
      mesh.Points.size() > std::min(budget.Vertices, size_t{131072}) ||
      count > budget.ContainerBytes / bytesPerFace) {
    return std::unexpected(RoadGroundError::CapacityExceeded);
  }
  if (count == 0 || mesh.Points.empty() || heightsM.size() != mesh.Points.size() ||
      mesh.FaceParents.size() != count || source.Faces.empty()) {
    return std::unexpected(RoadGroundError::InvalidGeometry);
  }
  RoadGroundFacetAudit audit;
  audit.Refinement.resize(count);
  std::vector<RoadGround::Facet> facets;
  facets.reserve(count);
  std::vector<Edge> edges;
  edges.reserve(3 * count);
  for (size_t at = 0; at < count; ++at) {
    if (stop.stop_requested()) { return std::unexpected(RoadGroundError::Canceled); }
    const auto parent = mesh.FaceParents[at];
    if (parent >= source.Faces.size()) { return std::unexpected(RoadGroundError::InvalidGeometry); }
    const auto &face = mesh.Faces[at];
    auto facet =
        RoadGround::CheckFacet(source.Faces[parent], field, face, mesh.Points, heightsM, audit, at);
    if (!facet) { return std::unexpected(facet.error()); }
    facets.push_back(*facet);
    for (size_t corner = 0; corner < 3; ++corner) {
      const auto a = face[corner];
      const auto b = face[(corner + 1) % 3];
      edges.push_back({.First = std::min(a, b),
                       .Last = std::max(a, b),
                       .Face = static_cast<uint32_t>(at),
                       .Forward = a < b});
    }
  }
  if (auto contacts = Contacts(facets, edges, audit, stop); !contacts) {
    return std::unexpected(contacts.error());
  }
  audit.BadFaces = static_cast<size_t>(std::ranges::count(audit.Refinement, uint8_t{1}));
  audit.ContainerCapacityBytes = audit.Refinement.capacity() * sizeof(uint8_t) +
                                 facets.capacity() * sizeof(RoadGround::Facet) +
                                 edges.capacity() * sizeof(Edge);
  if (audit.ContainerCapacityBytes > budget.ContainerBytes) {
    return std::unexpected(RoadGroundError::CapacityExceeded);
  }
  return audit;
}
}
