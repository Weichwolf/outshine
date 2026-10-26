#include "TransportCoreFixture.h"
#include "Check.h"
#include "CppAllocationProbe.h"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <print>
#include <stop_token>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::World;
using namespace outshine::Test;
constexpr double kExtentDeg = 0.25;

void CheckOwnedOutput() {
  const auto source = BridgeAcrossCore(kExtentDeg);
  CHECK(source != nullptr, "the native source snapshot is available");
  if (!source) { return; }
  const auto *edges = source->Topology().Edges().data();
  const auto *nodes = source->Topology().Nodes().data();
  BeginCppAllocationProbe();
  const auto selected = SelectTransportCore(kTransportCore, source, kCoreLimits);
  const auto allocation = EndCppAllocationProbe();
  CHECK(selected && selected->Segments.size() == 1 && allocation.Calls == 1 &&
            allocation.Bytes == selected->Work.ContainerCapacityBytes &&
            allocation.Bytes == sizeof(TransportCoreSegment),
        "only the exactly admitted owned result allocates, without temporary geometry storage");
  if (!selected) { return; }
  CHECK(selected->Source.get() == source.get() &&
            selected->Source->Topology().Edges().data() == edges &&
            selected->Source->Topology().Nodes().data() == nodes &&
            selected->Segments[0].SourceEdge == 0 && edges[0].Id.PathId == kCoreBridgePath &&
            edges[0].Id.Direction == EdgeDirection::Forward &&
            edges[1].Id.Direction == EdgeDirection::Reverse &&
            edges[0].Allows(TransportMode::Motor) && edges[1].Allows(TransportMode::Cycle) &&
            edges[0].Access == TransportAccess::Restricted,
        "physical geometry is selected once while the same original directed graph stays owned");
  CHECK(selected->Work.SourceEdgesVisited <= 2 * source->Topology().Edges().size() &&
            selected->Work.ValidatedNodes == source->Topology().Nodes().size() &&
            selected->Work.ValidatedEdges == source->Topology().Edges().size(),
        "selection work is bounded by the explicitly admitted source, independently of camera");
  std::println("TRANSPORT_CORE_SELECT segments={} cpp_allocations={} requested_bytes={} "
               "validated_nodes={} validated_edges={} source_edges_visited={} "
               "physical_segments_visited={} node_lookups={} windows={} bounds_rejected={} "
               "clip_divisions={}",
               selected->Segments.size(),
               allocation.Calls,
               allocation.Bytes,
               selected->Work.ValidatedNodes,
               selected->Work.ValidatedEdges,
               selected->Work.SourceEdgesVisited,
               selected->Work.PhysicalSegmentsVisited,
               selected->Work.NodeLookups,
               selected->Work.WindowTests,
               selected->Work.BoundingRejections,
               selected->Work.ClipDivisions);
}

void CheckRefusedAdmission() {
  const auto source = BridgeAcrossCore(kExtentDeg);
  if (!source) { return; }
  auto limits = kCoreLimits;
  limits.SourceNodesMost = source->Topology().Nodes().size() - 1;
  BeginCppAllocationProbe();
  const auto nodes = SelectTransportCore(kTransportCore, source, limits);
  const auto nodesAllocation = EndCppAllocationProbe();
  CHECK(!nodes && nodes.error() == TransportCoreError::CapacityExceeded &&
            nodesAllocation.Calls == 0,
        "source node admission is checked before validating a potentially huge isolated graph");
  limits = kCoreLimits;
  limits.SourceEdgesMost = source->Topology().Edges().size() - 1;
  BeginCppAllocationProbe();
  const auto edges = SelectTransportCore(kTransportCore, source, limits);
  const auto edgesAllocation = EndCppAllocationProbe();
  CHECK(!edges && edges.error() == TransportCoreError::CapacityExceeded &&
            edgesAllocation.Calls == 0,
        "source edge admission fails without allocating result or scratch arrays");
  limits = kCoreLimits;
  limits.SegmentsMost = 0;
  BeginCppAllocationProbe();
  const auto count = SelectTransportCore(kTransportCore, source, limits);
  const auto countAllocation = EndCppAllocationProbe();
  CHECK(!count && count.error() == TransportCoreError::CapacityExceeded &&
            countAllocation.Calls == 0,
        "the selected segment count is admitted before allocating owned output");
  limits = kCoreLimits;
  limits.ContainerCapacityMost = sizeof(TransportCoreSegment) - 1;
  BeginCppAllocationProbe();
  const auto bytes = SelectTransportCore(kTransportCore, source, limits);
  const auto bytesAllocation = EndCppAllocationProbe();
  CHECK(!bytes && bytes.error() == TransportCoreError::CapacityExceeded &&
            bytesAllocation.Calls == 0,
        "byte admission fails before the first output allocation");
}

void CheckEmptyAndCanceled() {
  const auto source = BridgeAcrossCore(kExtentDeg);
  std::stop_source stop;
  stop.request_stop();
  BeginCppAllocationProbe();
  const auto canceled = SelectTransportCore(kTransportCore, source, kCoreLimits, stop.get_token());
  const auto canceledAllocation = EndCppAllocationProbe();
  CHECK(!canceled && canceled.error() == TransportCoreError::Canceled &&
            canceledAllocation.Calls == 0,
        "an already canceled demand performs no source or output allocation");
  auto distant = kTransportCore;
  distant.X = 0;
  BeginCppAllocationProbe();
  const auto empty = SelectTransportCore(distant, source, kCoreLimits);
  const auto emptyAllocation = EndCppAllocationProbe();
  CHECK(empty && empty->Segments.empty() && empty->Source.get() == source.get() &&
            emptyAllocation.Calls == 0 && empty->Work.ContainerCapacityBytes == 0,
        "an empty geographic selection retains its source lease without output allocation");
  const auto missing = SelectTransportCore(kTransportCore, {}, kCoreLimits);
  CHECK(!missing && missing.error() == TransportCoreError::MissingSource,
        "missing native inputs are not mistaken for a known empty region");
  const auto invalid =
      SelectTransportCore({.Level = GeoCellId::MaximumLevel + 1}, source, kCoreLimits);
  CHECK(!invalid && invalid.error() == TransportCoreError::InvalidCore,
        "an invalid geographic core cannot become an accepted demand");
}
}

int main() {
  CheckOwnedOutput();
  CheckRefusedAdmission();
  CheckEmptyAndCanceled();
  return Report();
}
