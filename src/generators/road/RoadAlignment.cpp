#include "RoadAlignment.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>

namespace outshine::Generators {

namespace {

constexpr double kStationRoundingToleranceM = 1e-9;
constexpr double kHermiteEndpointDerivativeCoefficient = 6.0;

}

Vec3 RoadAlignment::Evaluate(const Cubic &curve, double parameter) noexcept {
  const double squared = parameter * parameter;
  const double cubed = squared * parameter;
  return curve.StartM * (2.0 * cubed - 3.0 * squared + 1.0) +
         curve.StartDerivativeM * (cubed - 2.0 * squared + parameter) +
         curve.EndM * (-2.0 * cubed + 3.0 * squared) + curve.EndDerivativeM * (cubed - squared);
}

Vec3 RoadAlignment::Derivative(const Cubic &curve, double parameter) noexcept {
  const double squared = parameter * parameter;
  return curve.StartM * (kHermiteEndpointDerivativeCoefficient * squared -
                         kHermiteEndpointDerivativeCoefficient * parameter) +
         curve.StartDerivativeM * (3.0 * squared - 4.0 * parameter + 1.0) +
         curve.EndM * (-kHermiteEndpointDerivativeCoefficient * squared +
                       kHermiteEndpointDerivativeCoefficient * parameter) +
         curve.EndDerivativeM * (3.0 * squared - 2.0 * parameter);
}

std::optional<size_t> RoadAlignment::FindEdgeIndex(World::TransportEdgeId id) const noexcept {
  const auto found = std::ranges::lower_bound(
      EdgeIndicesById_, id, {}, [this](uint32_t index) { return Edges_[index].SourceEdge; });
  if (found == EdgeIndicesById_.end() || Edges_[*found].SourceEdge != id) { return std::nullopt; }
  return static_cast<size_t>(*found);
}

const RoadAlignmentEdge *RoadAlignment::FindEdge(World::TransportEdgeId id) const noexcept {
  const std::optional<size_t> index = FindEdgeIndex(id);
  return index ? &Edges_[*index] : nullptr;
}

std::optional<RoadAlignmentPose> RoadAlignment::SampleEdge(EdgeStation request) const noexcept {
  const size_t edgeIndex = request.EdgeIndex;
  double edgeStationM = request.DistanceM;
  const RoadAlignmentEdge &edge = Edges_[edgeIndex];
  const ArcRange range = ArcRanges_[edgeIndex];
  const ArcSample *const first = ArcSamples_.data() + range.Begin;
  const ArcSample *const last = first + range.Count;
  const double lengthM = (last - 1)->DistanceM;
  if (!std::isfinite(edgeStationM) || edgeStationM < 0.0 ||
      edgeStationM > lengthM + kStationRoundingToleranceM) {
    return std::nullopt;
  }
  edgeStationM = std::min(edgeStationM, lengthM);
  const ArcSample *const upper =
      std::lower_bound(first + 1, last, edgeStationM, [](const ArcSample &sample, double station) {
        return sample.DistanceM < station;
      });
  if (upper == last) { return std::nullopt; }
  const ArcSample &lower = *(upper - 1);
  const double spanM = upper->DistanceM - lower.DistanceM;
  const double ratio = spanM > 0.0 ? (edgeStationM - lower.DistanceM) / spanM : 0.0;
  const double parameter = std::lerp(lower.Parameter, upper->Parameter, ratio);
  const Vec3 position = Evaluate(Curves_[edgeIndex], parameter);
  Vec3 tangent = Derivative(Curves_[edgeIndex], parameter);
  if (!Normalise(tangent)) { return std::nullopt; }
  const double width = std::lerp(edge.StartWidthM, edge.EndWidthM, edgeStationM / lengthM);
  return RoadAlignmentPose{
      .SourceEdge = edge.SourceEdge,
      .StationM = edge.StartStationM + edgeStationM,
      .EdgeStationM = edgeStationM,
      .PositionM = {.EastM = position[0], .NorthM = position[1], .UpM = position[2]},
      .TangentEnu = tangent,
      .WidthM = width,
      .BankRad = 0.0,
      .Surface = edge.Surface};
}

std::optional<RoadAlignmentPose> RoadAlignment::AtEdgeStation(World::TransportEdgeId id,
                                                              double edgeStationM) const noexcept {
  const std::optional<size_t> index = FindEdgeIndex(id);
  return index ? SampleEdge({.EdgeIndex = *index, .DistanceM = edgeStationM}) : std::nullopt;
}

std::optional<RoadAlignmentPose> RoadAlignment::AtStation(double stationM) const noexcept {
  if (!std::isfinite(stationM) || stationM < 0.0 || Edges_.empty()) { return std::nullopt; }
  if (Closed_) {
    stationM = std::fmod(stationM, LengthM_);
  } else if (stationM > LengthM_) {
    return std::nullopt;
  }
  const auto upper =
      std::ranges::upper_bound(Edges_, stationM, {}, &RoadAlignmentEdge::StartStationM);
  const size_t index =
      upper == Edges_.begin() ? 0 : static_cast<size_t>(upper - Edges_.begin() - 1);
  return SampleEdge({.EdgeIndex = index, .DistanceM = stationM - Edges_[index].StartStationM});
}

}
