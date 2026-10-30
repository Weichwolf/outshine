#include "GroundPortCore.h"
#include "TransportCoreClipping.h"
#include "TransportNetworkSnapshot.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <stop_token>
#include <utility>

namespace outshine::Generators {
namespace {
template <class Visit>
std::expected<void, TransportCoreError> VisitSegments(const GroundPortSourceChain &source,
                                                      const TransportCoreWindow &window,
                                                      TransportCoreWork &work,
                                                      const std::stop_token &stop,
                                                      Visit visit) {
  const auto &graph = source.Source->Topology();
  const auto edges = graph.Edges();
  for (const auto at : source.SourceEdges) {
    if (stop.stop_requested()) { return std::unexpected(TransportCoreError::Canceled); }
    if (at >= edges.size()) { return std::unexpected(TransportCoreError::InvalidSource); }
    ++work.SourceEdgesVisited;
    ++work.PhysicalSegmentsVisited;
    const auto &edge = edges[at];
    work.NodeLookups += 2;
    const auto *from = graph.FindNode(edge.FromNodeId);
    const auto *to = graph.FindNode(edge.ToNodeId);
    if (from == nullptr || to == nullptr) {
      return std::unexpected(TransportCoreError::InvalidSource);
    }
    const auto clipped = ClipTransportCore(window, at, *from, *to, work);
    if (!clipped) { return std::unexpected(clipped.error()); }
    for (size_t part = 0; part < clipped->Count; ++part) {
      if (!visit(clipped->Segments[part])) {
        return std::unexpected(TransportCoreError::CapacityExceeded);
      }
    }
  }
  return {};
}
}

std::expected<TransportCoreSelection, TransportCoreError>
SelectGroundPortCore(const GroundPortField &field,
                     GeoCellId core,
                     TransportCoreLimits limits,
                     const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(TransportCoreError::Canceled); }
  const auto window = TransportWindowOf(core);
  if (!window) { return std::unexpected(window.error()); }
  const auto &source = field.Source();
  if (!source.Source) { return std::unexpected(TransportCoreError::MissingSource); }
  const auto &graph = source.Source->Topology();
  if (graph.Nodes().size() > limits.SourceNodesMost ||
      graph.Edges().size() > limits.SourceEdgesMost ||
      graph.Edges().size() > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(TransportCoreError::CapacityExceeded);
  }
  TransportCoreSelection result{.Core = core, .Source = source.Source, .Segments = {}};
  size_t count = 0;
  const size_t capacityCount = limits.ContainerCapacityMost / sizeof(TransportCoreSegment);
  const auto admitted = [&](const TransportCoreSegment &) {
    if (count >= limits.SegmentsMost || count >= capacityCount) { return false; }
    ++count;
    return true;
  };
  const auto counted = VisitSegments(source, *window, result.Work, stop, admitted);
  if (!counted) { return std::unexpected(counted.error()); }
  if (stop.stop_requested()) { return std::unexpected(TransportCoreError::Canceled); }
  if (count == 0) { return result; }
  result.Segments.resize(count);
  if (result.Segments.capacity() > capacityCount) {
    return std::unexpected(TransportCoreError::CapacityExceeded);
  }
  result.Work.ContainerCapacityBytes = result.Segments.capacity() * sizeof(TransportCoreSegment);
  size_t next = 0;
  const auto stored = [&](const TransportCoreSegment &segment) {
    result.Segments[next++] = segment;
    return true;
  };
  const auto selected = VisitSegments(source, *window, result.Work, stop, stored);
  if (!selected) { return std::unexpected(selected.error()); }
  return result;
}
}
