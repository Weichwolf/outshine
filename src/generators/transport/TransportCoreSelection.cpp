#include "TransportCoreSelection.h"
#include "TransportCoreClipping.h"
#include "TransportNetworkSnapshot.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <stop_token>
#include <utility>

namespace outshine::Generators {
namespace {
bool SameSegment(const World::TransportEdgeId &a, const World::TransportEdgeId &b) {
  return a.PathId == b.PathId && a.SegmentOrdinal == b.SegmentOrdinal;
}

template <class Visit>
std::expected<void, TransportCoreError> VisitSegments(const TransportCoreWindow &window,
                                                      const World::TransportTopology &source,
                                                      TransportCoreWork &work,
                                                      const std::stop_token &stop,
                                                      Visit visit) {
  const auto edges = source.Edges();
  for (size_t at = 0; at < edges.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected(TransportCoreError::Canceled); }
    ++work.SourceEdgesVisited;
    const auto &edge = edges[at];
    if (at != 0 && SameSegment(edge.Id, edges[at - 1].Id)) {
      if (edge.FromNodeId != edges[at - 1].ToNodeId || edge.ToNodeId != edges[at - 1].FromNodeId) {
        return std::unexpected(TransportCoreError::InvalidSource);
      }
      continue;
    }
    ++work.PhysicalSegmentsVisited;
    work.NodeLookups += 2;
    const auto *from = source.FindNode(edge.FromNodeId);
    const auto *to = source.FindNode(edge.ToNodeId);
    if (from == nullptr || to == nullptr) {
      return std::unexpected(TransportCoreError::InvalidSource);
    }
    const auto clipped = ClipTransportCore(window, static_cast<uint32_t>(at), *from, *to, work);
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
SelectTransportCore(GeoCellId core,
                    std::shared_ptr<const World::TransportNetworkSnapshot> source,
                    TransportCoreLimits limits,
                    const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(TransportCoreError::Canceled); }
  const auto window = TransportWindowOf(core);
  if (!window) { return std::unexpected(window.error()); }
  if (!source) { return std::unexpected(TransportCoreError::MissingSource); }
  const auto &graph = source->Topology();
  if (graph.Nodes().size() > limits.SourceNodesMost ||
      graph.Edges().size() > limits.SourceEdgesMost ||
      graph.Edges().size() > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(TransportCoreError::CapacityExceeded);
  }
  if (source->SourceIdentity().DatasetId.empty() || source->SourceIdentity().Revision.empty() ||
      !World::ValidateTransportTopology(graph)) {
    return std::unexpected(TransportCoreError::InvalidSource);
  }
  TransportCoreSelection result{.Core = core, .Source = std::move(source), .Segments = {}};
  result.Work.ValidatedNodes = graph.Nodes().size();
  result.Work.ValidatedEdges = graph.Edges().size();
  size_t count = 0;
  const size_t capacityCount = limits.ContainerCapacityMost / sizeof(TransportCoreSegment);
  const auto admitted = [&](const TransportCoreSegment &) {
    if (count >= limits.SegmentsMost || count >= capacityCount) { return false; }
    ++count;
    return true;
  };
  const auto counted = VisitSegments(*window, graph, result.Work, stop, admitted);
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
  const auto selected = VisitSegments(*window, graph, result.Work, stop, stored);
  if (!selected) { return std::unexpected(selected.error()); }
  return result;
}
}
