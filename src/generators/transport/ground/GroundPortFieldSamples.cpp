#include "GroundPortField.h"
#include "TransportNetworkSnapshot.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <stop_token>
#include <tuple>
#include <utility>

namespace outshine::Generators {
namespace {
using Error = RoadPortQueryError;
constexpr double kCoordinateRoundoffUnits = 32;
constexpr double kDegreesPerTurn = 360;

bool OnSegment(LongitudeLatitude point, const std::array<LongitudeLatitude, 2> &segment) {
  const auto &a = segment[0];
  const auto &b = segment[1];
  const double dx = std::remainder(b.LongitudeDeg - a.LongitudeDeg, kDegreesPerTurn);
  const double dy = b.LatitudeDeg - a.LatitudeDeg;
  const double x = std::remainder(point.LongitudeDeg - a.LongitudeDeg, kDegreesPerTurn);
  const double y = point.LatitudeDeg - a.LatitudeDeg;
  const double scale = std::max({1.,
                                 std::abs(a.LongitudeDeg),
                                 std::abs(b.LongitudeDeg),
                                 std::abs(a.LatitudeDeg),
                                 std::abs(b.LatitudeDeg)});
  const double roundoff = kCoordinateRoundoffUnits * std::numeric_limits<double>::epsilon() * scale;
  const double length = std::max(std::abs(dx), std::abs(dy));
  if (!std::isfinite(x) || !std::isfinite(y) || length == 0) { return false; }
  const double fraction = std::abs(dx) >= std::abs(dy) ? x / dx : y / dy;
  return fraction >= -roundoff / length && fraction <= 1 + roundoff / length &&
         std::abs(x - fraction * dx) <= roundoff && std::abs(y - fraction * dy) <= roundoff;
}

std::expected<std::array<LongitudeLatitude, 2>, Error>
OriginalSegment(const World::TransportTopology &graph, const World::TransportEdge &edge) {
  const auto *from = graph.FindNode(edge.FromNodeId);
  const auto *to = graph.FindNode(edge.ToNodeId);
  if (from == nullptr || to == nullptr) { return std::unexpected(Error::InvalidInput); }
  std::array<LongitudeLatitude, 2> result{
      {{.LongitudeDeg = from->LongitudeDeg, .LatitudeDeg = from->LatitudeDeg},
       {.LongitudeDeg = to->LongitudeDeg, .LatitudeDeg = to->LatitudeDeg}}};
  if (edge.Id.Direction == World::EdgeDirection::Reverse) { std::ranges::reverse(result); }
  return result;
}
}

std::expected<GroundPortFieldSamples, GroundPortFieldError>
GroundPortField::Sample(std::span<const GroundPortFieldRequestPoint> points,
                        GroundPortFieldQueryLimits limits,
                        const std::stop_token &stop) const {
  if (stop.stop_requested()) { return std::unexpected(GroundPortFieldError{Error::Canceled}); }
  if (points.size() > limits.Ports ||
      points.size() > limits.ContainerBytes / sizeof(RoadPortFrameSample)) {
    return std::unexpected(GroundPortFieldError{Error::CapacityExceeded});
  }
  if (points.empty()) { return GroundPortFieldSamples{}; }
  auto query = RoadPortFieldQuery::Make(Data_.Fit.Field(), Data_.Domain.Faces, limits.Field);
  if (!query) { return std::unexpected(GroundPortFieldError{query.error()}); }
  GroundPortFieldSamples result;
  result.Work.ValidatedFaces = Data_.Domain.Faces.size();
  result.Ports.reserve(points.size());
  const auto &graph = Data_.Source.Source->Topology();
  const auto edges = graph.Edges();
  for (const auto &point : points) {
    if (stop.stop_requested()) { return std::unexpected(GroundPortFieldError{Error::Canceled}); }
    if (point.Edge.Direction != World::EdgeDirection::Forward &&
        point.Edge.Direction != World::EdgeDirection::Reverse) {
      return std::unexpected(GroundPortFieldError{Error::InvalidInput});
    }
    ++result.Work.MembershipLookups;
    const auto key = std::tie(point.Edge.PathId, point.Edge.SegmentOrdinal);
    const auto found = std::ranges::lower_bound(Data_.Members, key, {}, [&edges](uint32_t at) {
      return std::tie(edges[at].Id.PathId, edges[at].Id.SegmentOrdinal);
    });
    if (found == Data_.Members.end() ||
        std::tie(edges[*found].Id.PathId, edges[*found].Id.SegmentOrdinal) != key) {
      return std::unexpected(GroundPortFieldError{Error::InvalidInput});
    }
    const auto original = OriginalSegment(graph, edges[*found]);
    if (!original || !OnSegment(point.Anchor, *original)) {
      return std::unexpected(GroundPortFieldError{Error::InvalidInput});
    }
    ++result.Work.TerrainSamples;
    const auto height = Data_.Terrain->At(point.Anchor);
    if (!height) {
      return std::unexpected(GroundPortFieldError{GroundPortTerrainError::MissingTerrain});
    }
    const auto sampled =
        SampleRoadPortFrame(*query,
                            {.SourceAnchor = Data_.Domain.Anchor,
                             .PortAnchor = point.Anchor,
                             .OriginalSegment = *original,
                             .HalfWidthM = Data_.Source.WidthM / 2,
                             .InitialModelHeightM = *height,
                             .MaximumLongitudinalGrade = Data_.Source.MaximumGradient},
                            stop);
    if (!sampled) { return std::unexpected(GroundPortFieldError{sampled.error()}); }
    result.Ports.push_back(*sampled);
  }
  result.Work.Field = query->Work();
  result.Work.ContainerCapacityBytes = result.Ports.capacity() * sizeof(RoadPortFrameSample);
  if (result.Work.ContainerCapacityBytes > limits.ContainerBytes) {
    return std::unexpected(GroundPortFieldError{Error::CapacityExceeded});
  }
  return result;
}
}
