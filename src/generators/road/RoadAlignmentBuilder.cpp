#include "RoadAlignment.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <numeric>
#include <ranges>
#include <span>
#include <vector>

namespace outshine::Generators {

namespace {

constexpr double kMinimumChordM = 0.001;
constexpr double kMaximumArcStepM = 0.5;
constexpr size_t kMinimumArcSteps = 4;
constexpr size_t kMaximumArcSamples = 262144;
constexpr double kMinimumForwardDot = -0.8660254037844386;
constexpr double kMinimumHorizontalDirection = 0.1;

[[nodiscard]] RoadAlignmentError
Error(RoadAlignmentErrorCode code, World::TransportEdgeId edge = {}, uint64_t nodeId = 0) {
  return {.Code = code, .SourceEdge = edge, .SourceNodeId = nodeId};
}

[[nodiscard]] Vec3 PositionOf(const RoadConstraintPoint &point) {
  return {{point.TerrainLocalM.EastM, point.TerrainLocalM.NorthM, point.TerrainLocalM.UpM}};
}

std::expected<std::vector<Vec3>, RoadAlignmentError>
SolveTangents(const RoadConstraintChain &constraints, std::span<const Vec3> positions) {
  const size_t edgeCount = constraints.Edges().size();
  const bool closed = constraints.Closed();
  const size_t uniqueCount = closed ? edgeCount : edgeCount + 1;
  std::vector<Vec3> tangents;
  tangents.reserve(edgeCount + 1);
  for (size_t index = 0; index < uniqueCount; ++index) {
    const size_t previous = index == 0 ? uniqueCount - 1 : index - 1;
    const size_t next = (index + 1) % uniqueCount;
    Vec3 incoming = positions[index] - positions[previous];
    Vec3 outgoing = positions[next] - positions[index];
    if (!closed && index == 0) { incoming = outgoing; }
    if (!closed && index == uniqueCount - 1) { outgoing = incoming; }
    const double incomingLengthM = Length(incoming);
    const double outgoingLengthM = Length(outgoing);
    if (!std::isfinite(incomingLengthM) || incomingLengthM < kMinimumChordM ||
        !std::isfinite(outgoingLengthM) || outgoingLengthM < kMinimumChordM ||
        !Normalise(incoming) || !Normalise(outgoing) ||
        Dot(incoming, outgoing) < kMinimumForwardDot) {
      const size_t edgeIndex = std::min(index, edgeCount - 1);
      return std::unexpected(Error(RoadAlignmentErrorCode::SharpTurn,
                                   constraints.Edges()[edgeIndex].SourceEdge,
                                   constraints.Points()[index].SourceNodeId));
    }
    Vec3 tangent = incoming + outgoing;
    Vec3 weighted = incoming * outgoingLengthM + outgoing * incomingLengthM;
    if (!Normalise(tangent) || !Normalise(weighted)) {
      const size_t edgeIndex = std::min(index, edgeCount - 1);
      return std::unexpected(Error(RoadAlignmentErrorCode::SharpTurn,
                                   constraints.Edges()[edgeIndex].SourceEdge,
                                   constraints.Points()[index].SourceNodeId));
    }
    const double horizontal = std::hypot(weighted[0], weighted[1]);
    if (!std::isfinite(horizontal) || horizontal < kMinimumHorizontalDirection) {
      const size_t edgeIndex = std::min(index, edgeCount - 1);
      return std::unexpected(Error(RoadAlignmentErrorCode::SharpTurn,
                                   constraints.Edges()[edgeIndex].SourceEdge,
                                   constraints.Points()[index].SourceNodeId));
    }
    tangent[2] = weighted[2] * std::hypot(tangent[0], tangent[1]) / horizontal;
    tangents.push_back(tangent);
  }
  if (closed) { tangents.push_back(tangents.front()); }
  return tangents;
}

std::vector<double> NodeWidths(const RoadConstraintChain &constraints) {
  const size_t edgeCount = constraints.Edges().size();
  std::vector<double> widths(edgeCount + 1);
  for (size_t index = 0; index <= edgeCount; ++index) {
    if (!constraints.Closed() && index == 0) {
      widths[index] = constraints.Edges().front().WidthM;
    } else if (!constraints.Closed() && index == edgeCount) {
      widths[index] = constraints.Edges().back().WidthM;
    } else {
      const size_t previous = (index + edgeCount - 1) % edgeCount;
      const size_t next = index % edgeCount;
      widths[index] =
          std::midpoint(constraints.Edges()[previous].WidthM, constraints.Edges()[next].WidthM);
    }
  }
  return widths;
}

}

std::expected<void, RoadAlignmentError>
RoadAlignmentBuilder::AppendEdge(RoadAlignment &alignment,
                                 const RoadConstraintChain &constraints,
                                 std::span<const Vec3> points,
                                 std::span<const Vec3> tangents,
                                 std::span<const double> nodeWidths,
                                 size_t index) {
  const RoadConstraintEdge &source = constraints.Edges()[index];
  const double chordM = Length(points[index + 1] - points[index]);
  if (!std::isfinite(chordM) || chordM < kMinimumChordM) {
    return std::unexpected(Error(RoadAlignmentErrorCode::InvalidConstraints, source.SourceEdge));
  }
  const double requestedSteps = std::ceil(chordM / kMaximumArcStepM);
  if (!std::isfinite(requestedSteps) || requestedSteps >= kMaximumArcSamples) {
    return std::unexpected(Error(RoadAlignmentErrorCode::SampleBudgetExceeded, source.SourceEdge));
  }
  const size_t steps = std::max(kMinimumArcSteps, static_cast<size_t>(requestedSteps));
  if (steps + 1 > kMaximumArcSamples - alignment.ArcSamples_.size()) {
    return std::unexpected(Error(RoadAlignmentErrorCode::SampleBudgetExceeded, source.SourceEdge));
  }
  const RoadAlignment::Cubic curve{.StartM = points[index],
                                   .EndM = points[index + 1],
                                   .StartDerivativeM = tangents[index] * chordM,
                                   .EndDerivativeM = tangents[index + 1] * chordM};
  const auto arcBegin = static_cast<uint32_t>(alignment.ArcSamples_.size());
  alignment.ArcSamples_.push_back({.Parameter = 0.0, .DistanceM = 0.0});
  Vec3 previous = curve.StartM;
  double arcLengthM = 0.0;
  for (size_t step = 1; step <= steps; ++step) {
    const double parameter = static_cast<double>(step) / static_cast<double>(steps);
    const Vec3 position = RoadAlignment::Evaluate(curve, parameter);
    arcLengthM += Length(position - previous);
    if (!std::isfinite(arcLengthM) || arcLengthM <= alignment.ArcSamples_.back().DistanceM) {
      return std::unexpected(Error(RoadAlignmentErrorCode::InvalidArcLength, source.SourceEdge));
    }
    alignment.ArcSamples_.push_back({.Parameter = parameter, .DistanceM = arcLengthM});
    previous = position;
  }
  const double endStationM = alignment.LengthM_ + arcLengthM;
  if (!std::isfinite(endStationM) || endStationM <= alignment.LengthM_) {
    return std::unexpected(Error(RoadAlignmentErrorCode::InvalidArcLength, source.SourceEdge));
  }
  alignment.Curves_.push_back(curve);
  alignment.ArcRanges_.push_back({.Begin = arcBegin, .Count = static_cast<uint32_t>(steps + 1)});
  alignment.Edges_.push_back({.SourceEdge = source.SourceEdge,
                              .StartStationM = alignment.LengthM_,
                              .EndStationM = endStationM,
                              .SourceWidthM = source.WidthM,
                              .StartWidthM = nodeWidths[index],
                              .EndWidthM = nodeWidths[index + 1],
                              .Surface = source.Surface});
  alignment.LengthM_ = endStationM;
  return {};
}

std::expected<RoadAlignment, RoadAlignmentError>
RoadAlignmentBuilder::Build(const RoadConstraintChain &constraints) {
  const size_t edgeCount = constraints.Edges().size();
  if (edgeCount == 0 || constraints.Points().size() != edgeCount + 1) {
    return std::unexpected(Error(RoadAlignmentErrorCode::InvalidConstraints));
  }
  for (const RoadConstraintEdge &edge : constraints.Edges()) {
    if (edge.Bridge || edge.Tunnel) {
      return std::unexpected(Error(RoadAlignmentErrorCode::StructureNeedsSolver, edge.SourceEdge));
    }
  }
  std::vector<Vec3> positions;
  positions.reserve(edgeCount + 1);
  for (const RoadConstraintPoint &point : constraints.Points()) {
    positions.push_back(PositionOf(point));
  }
  const auto tangents = SolveTangents(constraints, positions);
  if (!tangents) { return std::unexpected(tangents.error()); }
  const std::vector<double> widths = NodeWidths(constraints);

  RoadAlignment alignment;
  alignment.SourceIdentity_ = constraints.SourceIdentity();
  alignment.TerrainDigest_ = constraints.TerrainDigest();
  alignment.TerrainSources_.assign(constraints.TerrainSources().begin(),
                                   constraints.TerrainSources().end());
  alignment.Anchor_ = constraints.Anchor();
  alignment.Closed_ = constraints.Closed();
  alignment.Edges_.reserve(edgeCount);
  alignment.Curves_.reserve(edgeCount);
  alignment.ArcRanges_.reserve(edgeCount);
  alignment.EdgeIndicesById_.reserve(edgeCount);
  for (size_t index = 0; index < edgeCount; ++index) {
    if (const auto appended =
            AppendEdge(alignment, constraints, positions, *tangents, widths, index);
        !appended) {
      return std::unexpected(appended.error());
    }
    alignment.EdgeIndicesById_.push_back(static_cast<uint32_t>(index));
  }
  std::ranges::sort(alignment.EdgeIndicesById_, [&alignment](uint32_t left, uint32_t right) {
    return alignment.Edges_[left].SourceEdge < alignment.Edges_[right].SourceEdge;
  });
  return alignment;
}

}
