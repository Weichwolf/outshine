#include "TransportTopology.h"
#include "CppAllocationProbe.h"
#include "Check.h"
#include <algorithm>
#include <cstddef>
#include <chrono>
#include <cstdint>
#include <limits>
#include <print>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::World;
using namespace outshine::Test;
constexpr double kWidthM = 8;
constexpr size_t kUnclassified = 7;
constexpr uint64_t kFirstPath = 100;
constexpr uint64_t kSecondPath = 200;

PreparedTransportTopology Data() {
  PreparedTransportTopology data;
  data.SourceIdentity = {.DatasetId = "native-prepared", .Revision = "v2"};
  data.Nodes = {{.SourceNodeId = 1, .LatitudeDeg = 0, .LongitudeDeg = 0},
                {.SourceNodeId = 2, .LatitudeDeg = 0, .LongitudeDeg = 0},
                {.SourceNodeId = 3, .LatitudeDeg = 0, .LongitudeDeg = 1}};
  TransportEdge edge{
      .Id = {.PathId = kFirstPath, .SegmentOrdinal = 0, .Direction = EdgeDirection::Forward},
      .FromNodeId = 1,
      .ToNodeId = 2,
      .Modes =
          static_cast<uint8_t>(TransportMode::Motor) | static_cast<uint8_t>(TransportMode::Cycle),
      .Facility = TransportFacility::LocalStreet,
      .Surface = TransportSurface::Gravel,
      .WidthM = kWidthM,
      .LaneCount = 2,
      .Layer = 1,
      .Bridge = true,
      .Tunnel = true,
      .Access = TransportAccess::Restricted};
  data.Edges.push_back(edge);
  edge.Id.Direction = EdgeDirection::Reverse;
  std::swap(edge.FromNodeId, edge.ToNodeId);
  data.Edges.push_back(edge);
  edge.Id = {.PathId = kSecondPath, .SegmentOrdinal = 0, .Direction = EdgeDirection::Reverse};
  edge.FromNodeId = 3;
  edge.ToNodeId = 2;
  edge.Modes = static_cast<uint8_t>(TransportMode::Walk);
  edge.Facility = TransportFacility::Walkway;
  edge.Surface = TransportSurface::Paved;
  edge.Layer = -1;
  edge.Bridge = false;
  edge.Access = TransportAccess::Forbidden;
  data.Edges.push_back(edge);
  data.Outgoing = {
      {.NodeId = 1, .EdgeIndex = 0}, {.NodeId = 2, .EdgeIndex = 1}, {.NodeId = 3, .EdgeIndex = 2}};
  data.UnclassifiedPathCount = kUnclassified;
  return data;
}

void CheckRestoration() {
  auto data = Data();
  const auto *nodes = data.Nodes.data();
  const auto *edges = data.Edges.data();
  const auto *outgoing = data.Outgoing.data();
  BeginCppAllocationProbe();
  const auto restored = TransportTopology::RestorePrepared(std::move(data));
  const auto allocations = EndCppAllocationProbe();
  CHECK(restored && allocations.Calls == 0,
        "valid prepared topology adopts its existing storage without rebuilding allocations");
  if (!restored) { return; }
  CHECK(restored->Nodes().data() == nodes && restored->Edges().data() == edges &&
            restored->Outgoing().data() == outgoing &&
            restored->UnclassifiedPathCount() == kUnclassified &&
            restored->SourceIdentity().DatasetId == "native-prepared" &&
            restored->SourceIdentity().Revision == "v2",
        "nodes, directed edges, saved adjacency and source identity keep the same owned backing");
  const auto &first = restored->Edges()[0];
  const auto &reverse = restored->Edges()[1];
  const auto &forbidden = restored->Edges()[2];
  CHECK(first.Id.PathId == kFirstPath && first.Id.Direction == EdgeDirection::Forward &&
            reverse.Id.Direction == EdgeDirection::Reverse &&
            first.FromNodeId == reverse.ToNodeId && first.ToNodeId == reverse.FromNodeId &&
            first.WidthM == kWidthM && first.LaneCount == 2 &&
            first.Facility == TransportFacility::LocalStreet &&
            first.Surface == TransportSurface::Gravel && first.Layer == 1 && first.Bridge &&
            first.Tunnel && first.Access == TransportAccess::Restricted &&
            first.Allows(TransportMode::Motor) && first.Allows(TransportMode::Cycle),
        "both flags and all actual physical, facility, access and directional mode fields survive "
        "the prepared boundary");
  CHECK(forbidden.Tunnel && !forbidden.Bridge && forbidden.Layer == -1 &&
            forbidden.Surface == TransportSurface::Paved &&
            !forbidden.Allows(TransportMode::Walk) &&
            restored->FindNode(1) != restored->FindNode(2) &&
            restored->FindNode(1)->LongitudeDeg == restored->FindNode(2)->LongitudeDeg,
        "forbidden traffic and distinct coincident source-node identities stay distinct");
  CHECK(restored->OutgoingFrom(3).size() == 1 && restored->OutgoingFrom(3)[0].EdgeIndex == 2 &&
            ValidateTransportTopology(*restored).has_value() &&
            restored->CapacityBytes() >= sizeof(*restored),
        "the stored index answers native queries directly and has separately reported container "
        "capacity");
}

void CheckPreparedWork() {
  constexpr uint32_t nodeCount = 10000;
  PreparedTransportTopology saved;
  saved.SourceIdentity = {.DatasetId = "native-linear", .Revision = "1"};
  saved.Nodes.reserve(nodeCount);
  saved.Edges.reserve(nodeCount - 1);
  saved.Outgoing.reserve(nodeCount - 1);
  for (uint32_t at = 0; at < nodeCount; ++at) {
    saved.Nodes.push_back({.SourceNodeId = at + uint64_t{1}});
    if (at + 1 < nodeCount) {
      saved.Edges.push_back({.Id = {.PathId = kFirstPath, .SegmentOrdinal = at},
                             .FromNodeId = at + uint64_t{1},
                             .ToNodeId = at + uint64_t{2}});
      saved.Outgoing.push_back({.NodeId = at + uint64_t{1}, .EdgeIndex = at});
    }
  }
  auto original = saved;
  const auto started = std::chrono::steady_clock::now();
  BeginCppAllocationProbe();
  const TransportTopology rebuilt(
      std::move(original.SourceIdentity), std::move(original.Nodes), std::move(original.Edges));
  const auto checked = ValidateTransportTopology(rebuilt);
  const auto rebuildAllocations = EndCppAllocationProbe();
  const auto rebuiltAt = std::chrono::steady_clock::now();
  BeginCppAllocationProbe();
  const auto restored = TransportTopology::RestorePrepared(std::move(saved));
  const auto restoreAllocations = EndCppAllocationProbe();
  const auto restoredAt = std::chrono::steady_clock::now();
  CHECK(
      checked && restored && restored->Edges().size() == rebuilt.Edges().size() &&
          restored->OutgoingFrom(nodeCount - 1)[0].EdgeIndex == nodeCount - 2 &&
          restoreAllocations.Calls == 0 && restoreAllocations.Bytes == 0 &&
          rebuildAllocations.Calls == 1 &&
          rebuildAllocations.Bytes == (nodeCount - 1) * sizeof(OutgoingTransportEdge),
      "a saved large topology directly retains its index instead of allocating and rebuilding it");
  std::println(
      "TRANSPORT_TOPOLOGY_RESTORE nodes={} edges={} rebuild_cpp_allocations={} "
      "rebuild_requested_bytes={} restore_cpp_allocations={} restore_requested_bytes={} "
      "validated_rebuild_ns={} validated_restore_ns={}",
      nodeCount,
      rebuilt.Edges().size(),
      rebuildAllocations.Calls,
      rebuildAllocations.Bytes,
      restoreAllocations.Calls,
      restoreAllocations.Bytes,
      std::chrono::duration_cast<std::chrono::nanoseconds>(rebuiltAt - started).count(),
      std::chrono::duration_cast<std::chrono::nanoseconds>(restoredAt - rebuiltAt).count());
}

void CheckInvalidData() {
  auto unsorted = Data();
  std::swap(unsorted.Nodes[0], unsorted.Nodes[1]);
  const auto nodes = TransportTopology::RestorePrepared(std::move(unsorted));
  CHECK(!nodes && nodes.error() == TransportTopologyError::InvalidNodes,
        "a prepared loader rejects unsorted nodes instead of silently sorting them");
  auto reordered = Data();
  std::swap(reordered.Edges[0], reordered.Edges[1]);
  const auto edges = TransportTopology::RestorePrepared(std::move(reordered));
  CHECK(!edges && edges.error() == TransportTopologyError::InvalidEdges,
        "a prepared loader rejects reordered source edges instead of renumbering the saved index");
  auto missing = Data();
  missing.Outgoing.pop_back();
  const auto missingIndex = TransportTopology::RestorePrepared(std::move(missing));
  CHECK(!missingIndex && missingIndex.error() == TransportTopologyError::InvalidOutgoing,
        "incomplete saved adjacency is never rebuilt into an apparently valid hit");
  auto duplicate = Data();
  duplicate.Outgoing[1] = duplicate.Outgoing[0];
  const auto repeated = TransportTopology::RestorePrepared(std::move(duplicate));
  CHECK(!repeated && repeated.error() == TransportTopologyError::InvalidOutgoing,
        "duplicate or misbound adjacency cannot replace another edge");
  auto absent = Data();
  absent.Edges[0].FromNodeId = std::numeric_limits<uint64_t>::max();
  const auto badReference = TransportTopology::RestorePrepared(std::move(absent));
  CHECK(!badReference && badReference.error() == TransportTopologyError::InvalidEdges,
        "an absent source node cannot become an implicit connection");
  auto unknown = Data();
  unknown.Edges[0].Modes = uint8_t{128};
  CHECK(!TransportTopology::RestorePrepared(std::move(unknown)),
        "unsupported mode bits cannot be accepted as native transport");
  unknown = Data();
  unknown.Edges[0].Facility = static_cast<TransportFacility>(std::numeric_limits<uint8_t>::max());
  CHECK(!TransportTopology::RestorePrepared(std::move(unknown)),
        "unknown facility enumeration values stay invalid");
  auto invalid = Data();
  invalid.Edges[0].WidthM = std::numeric_limits<double>::quiet_NaN();
  const auto nonfinite = TransportTopology::RestorePrepared(std::move(invalid));
  CHECK(!nonfinite && nonfinite.error() == TransportTopologyError::InvalidEdges,
        "invalid physical fields remain invalid native data");
}
}

int main() {
  CheckRestoration();
  CheckPreparedWork();
  CheckInvalidData();
  return Report();
}
