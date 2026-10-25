#include "BridgeInputRequirements.h"
#include "TransportNetworkSnapshot.h"
#include "CppAllocationProbe.h"
#include "Earth.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::World;
using namespace outshine::Test;
constexpr BridgeInputLimits kLimits{.SourceEdgesMost = 64,
                                    .ContainerCapacityMost = size_t{16} * 1024};
constexpr double kLeftRunM = 20000;
constexpr double kRightRunM = 30000;
constexpr double kLocalRingM = 2000;
constexpr double kFarGradeM = 2000;
constexpr double kMaximumGradient = 0.05;
constexpr double kDegreesPerRadian = 180.0 / std::numbers::pi;
constexpr double kRoadWidthM = 8;
constexpr uint64_t kLeftGroundPath = 100;
constexpr uint64_t kRightGroundPath = 300;
constexpr std::array<uint64_t, 3> kCyclePaths{401, 402, 403};

struct FixtureOptions {
  bool Reversed = false;
  bool Detached = false;
  bool Tunnel = false;
  bool Covered = false;
};

TransportEdge Edge(uint64_t id, uint64_t from, uint64_t to, bool bridge) {
  return {.Id = {.PathId = id, .SegmentOrdinal = 0, .Direction = EdgeDirection::Forward},
          .FromNodeId = from,
          .ToNodeId = to,
          .Modes = static_cast<uint8_t>(TransportMode::Motor),
          .Facility = TransportFacility::LocalStreet,
          .Surface = TransportSurface::Asphalt,
          .WidthM = kRoadWidthM,
          .LaneCount = 2,
          .Layer = bridge ? 1 : 0,
          .Bridge = bridge,
          .Tunnel = false,
          .Access = TransportAccess::Public};
}

std::shared_ptr<const TransportNetworkSnapshot> Source(FixtureOptions options = {}) {
  constexpr std::array east{
      -kLeftRunM - 1000, -kLeftRunM, -200.0, 200.0, kRightRunM, kRightRunM + 1000};
  std::vector<TransportNode> nodes;
  nodes.reserve(east.size() + 1);
  for (size_t at = 0; at < east.size(); ++at) {
    nodes.push_back({.SourceNodeId = at + 1,
                     .LatitudeDeg = 0,
                     .LongitudeDeg = east[at] * kDegreesPerRadian / kEarthMeanRadiusM});
  }
  if (options.Detached) {
    auto separate = nodes[4];
    separate.SourceNodeId = 9;
    nodes.push_back(separate);
  }
  std::vector<TransportEdge> edges{Edge(kLeftGroundPath, 1, 2, false),
                                   Edge(kRightGroundPath, 6, options.Detached ? 9 : 5, false)};
  edges[1].Tunnel = options.Tunnel;
  constexpr std::array<std::array<uint64_t, 3>, 3> bridge{{{201, 2, 3}, {202, 3, 4}, {203, 4, 5}}};
  for (const auto part : bridge) {
    auto edge = Edge(part[0], part[1], part[2], true);
    edge.Tunnel = options.Covered;
    edges.push_back(edge);
    edge.Id.Direction = EdgeDirection::Reverse;
    std::swap(edge.FromNodeId, edge.ToNodeId);
    edges.push_back(edge);
  }
  if (options.Reversed) {
    std::ranges::reverse(nodes);
    std::ranges::reverse(edges);
  }
  TransportTopology graph(
      {.DatasetId = "analytical-bridge", .Revision = "r1"}, std::move(nodes), std::move(edges));
  return std::make_shared<const TransportNetworkSnapshot>(
      ResolvedTransport{.Graph = std::move(graph), .Routes = {}},
      std::vector<GeographicBounds>{},
      TransportLoadMetrics{});
}

std::vector<TransportEdgeId> Ids(const BridgeInputRequirements &scope) {
  std::vector<TransportEdgeId> ids;
  ids.reserve(scope.BridgeEdges.size());
  for (const auto at : scope.BridgeEdges) {
    ids.push_back(scope.Source->Topology().Edges()[at].Id);
  }
  return ids;
}

void CheckFarConditions() {
  const auto source = Source();
  const std::array seeds{TransportEdgeId{.PathId = 202}};
  BeginCppAllocationProbe();
  const auto scope = SelectBridgeInputRequirements(source, seeds, kLimits);
  const auto allocations = EndCppAllocationProbe();
  CHECK(scope.has_value(), "the bounded native bridge source selection succeeds");
  if (!scope) { return; }
  const std::vector expected{TransportEdgeId{.PathId = 201},
                             TransportEdgeId{.PathId = 202},
                             TransportEdgeId{.PathId = 203}};
  CHECK(scope->Source.get() == source.get() && Ids(*scope) == expected &&
            scope->GroundNodes == std::vector<uint64_t>({2, 5}) && scope->Unqualified.empty(),
        "physical segments and both far source-node conditions retain the original immutable "
        "snapshot");
  const auto &graph = source->Topology();
  const auto left = graph.OutgoingFrom(2);
  const auto right = graph.OutgoingFrom(5);
  CHECK(left.size() == 1 && right.size() == 1 && graph.Edges()[left[0].EdgeIndex].Bridge &&
            graph.Edges()[right[0].EdgeIndex].Bridge,
        "both actual ground connections are incoming-only and absent from outgoing navigation");
  const auto *far = graph.FindNode(scope->GroundNodes.back());
  const double farRunM = far->LongitudeDeg * kEarthMeanRadiusM / kDegreesPerRadian;
  const double lowerGradeM = kFarGradeM - kMaximumGradient * farRunM;
  const double upperGradeM = kMaximumGradient * kLeftRunM;
  CHECK(farRunM > kLocalRingM && lowerGradeM > 0 && lowerGradeM <= upperGradeM,
        "the far g*L condition excludes local zero grade while a feasible interval remains");
  CHECK(scope->Work.SourceEdges == graph.Edges().size() && scope->Work.SeedLookups == 2 &&
            scope->Work.PhysicalSegments == 5 && scope->Work.BridgeSegments == 3 &&
            scope->Work.BridgeVisits == 3 && scope->Work.AdjacencyEntries == 6 &&
            scope->Work.AdjacencyVisits <= scope->Work.AdjacencyEntries,
        "paired traffic directions do not duplicate physical bridge traversal or fitting inputs");
  CHECK(allocations.Bytes == scope->Work.PeakContainerCapacityBytes &&
            allocations.Bytes <= kLimits.ContainerCapacityMost,
        "only the reported bounded index and result containers allocate; no snapshot clone or "
        "transient growth");
  Note("bridge source edges inspected", static_cast<double>(scope->Work.SourceEdges), "edges");
  Note("bridge physical segments selected",
       static_cast<double>(scope->Work.BridgeVisits),
       "segments");
  Note("bridge C++ allocations", static_cast<double>(allocations.Calls), "calls");
  Note("bridge C++ requested capacity", static_cast<double>(allocations.Bytes), "bytes");
  Note("far g*L lower grade condition", lowerGradeM, "m");
}

void CheckOrderingAndUnqualified() {
  const std::array seeds{TransportEdgeId{.PathId = 203, .Direction = EdgeDirection::Reverse},
                         TransportEdgeId{.PathId = 201},
                         TransportEdgeId{.PathId = 202}};
  const auto first = SelectBridgeInputRequirements(Source(), seeds, kLimits);
  auto reversedSeeds = seeds;
  std::ranges::reverse(reversedSeeds);
  const auto reversed =
      SelectBridgeInputRequirements(Source({.Reversed = true}), reversedSeeds, kLimits);
  CHECK(first && reversed && Ids(*first) == Ids(*reversed) &&
            first->GroundNodes == reversed->GroundNodes &&
            first->Unqualified == reversed->Unqualified &&
            first->Work.BridgeVisits == reversed->Work.BridgeVisits,
        "source arrival and seed direction/order do not change physical source requirements");
  const auto covered = SelectBridgeInputRequirements(Source({.Covered = true}), seeds, kLimits);
  CHECK(first && covered && Ids(*first) == Ids(*covered) &&
            first->GroundNodes == covered->GroundNodes && covered->Unqualified.empty() &&
            covered->Source->Topology().Edges()[covered->BridgeEdges[0]].Tunnel,
        "covered bridge flags remain intact and do not remove their physical source conditions");
  const auto detached = SelectBridgeInputRequirements(Source({.Detached = true}), seeds, kLimits);
  const auto tunnel = SelectBridgeInputRequirements(Source({.Tunnel = true}), seeds, kLimits);
  const std::vector unqualified{
      BridgeConditionRequest{.Node = 5, .Reason = BridgeConditionReason::NoGroundConnection}};
  CHECK(detached && detached->GroundNodes == std::vector<uint64_t>({2}) &&
            detached->Unqualified == unqualified,
        "a coincident node from a different source node ID never supplies the bridge landing");
  CHECK(tunnel && tunnel->GroundNodes == std::vector<uint64_t>({2}) &&
            tunnel->Unqualified == unqualified,
        "a tunnel connection requests qualification instead of inventing a DEM landing");
}

void CheckUngroundedCycle() {
  std::vector<TransportNode> nodes{{.SourceNodeId = 1, .LatitudeDeg = 0, .LongitudeDeg = 0},
                                   {.SourceNodeId = 2, .LatitudeDeg = 0, .LongitudeDeg = 1},
                                   {.SourceNodeId = 3, .LatitudeDeg = 1, .LongitudeDeg = 0}};
  std::vector<TransportEdge> edges{Edge(kCyclePaths[0], 1, 2, true),
                                   Edge(kCyclePaths[1], 2, 3, true),
                                   Edge(kCyclePaths[2], 3, 1, true)};
  TransportTopology graph(
      {.DatasetId = "analytical-cycle", .Revision = "r1"}, std::move(nodes), std::move(edges));
  const auto source = std::make_shared<const TransportNetworkSnapshot>(
      ResolvedTransport{.Graph = std::move(graph), .Routes = {}},
      std::vector<GeographicBounds>{},
      TransportLoadMetrics{});
  const std::array seeds{TransportEdgeId{.PathId = 401}};
  const auto selected = SelectBridgeInputRequirements(source, seeds, kLimits);
  const std::vector expected{
      BridgeConditionRequest{.Node = 1, .Reason = BridgeConditionReason::NoGroundAnchor}};
  CHECK(selected && selected->GroundNodes.empty() && selected->Unqualified == expected &&
            selected->Work.BridgeVisits == 3,
        "a bridge cycle terminates with an explicit missing ground condition, never an invented "
        "height");
}

void CheckAdmission() {
  const auto source = Source();
  const std::array seeds{TransportEdgeId{.PathId = 202}};
  BeginCppAllocationProbe();
  const auto denied = SelectBridgeInputRequirements(
      source, seeds, {.SourceEdgesMost = kLimits.SourceEdgesMost, .ContainerCapacityMost = 0});
  const auto deniedAllocations = EndCppAllocationProbe();
  CHECK(!denied && denied.error().Code == BridgeInputErrorCode::CapacityExceeded &&
            deniedAllocations.Calls == 0,
        "container admission fails before any source index allocation");
  BeginCppAllocationProbe();
  const auto empty = SelectBridgeInputRequirements(source, {}, {});
  const auto emptyAllocations = EndCppAllocationProbe();
  CHECK(empty && empty->BridgeEdges.empty() && empty->Work.SourceEdges == 0 &&
            emptyAllocations.Calls == 0,
        "an empty demand does no metadata traversal or allocation");
  const std::array missing{TransportEdgeId{.PathId = 999}};
  BeginCppAllocationProbe();
  const auto unknown = SelectBridgeInputRequirements(source, missing, kLimits);
  const auto unknownAllocations = EndCppAllocationProbe();
  CHECK(!unknown && unknown.error().Code == BridgeInputErrorCode::MissingSeed &&
            unknown.error().Edge == missing[0] && unknownAllocations.Calls == 0,
        "a missing source seed is explicit before index work");
  const std::array ground{TransportEdgeId{.PathId = 100}};
  const auto wrong = SelectBridgeInputRequirements(source, ground, kLimits);
  CHECK(!wrong && wrong.error().Code == BridgeInputErrorCode::InvalidSeed,
        "a ground road cannot silently become a bridge-demand seed");
  const auto missingSource = SelectBridgeInputRequirements({}, seeds, kLimits);
  CHECK(!missingSource && missingSource.error().Code == BridgeInputErrorCode::MissingSource,
        "missing source ownership cannot be a satisfied native request");
  std::stop_source canceled;
  CHECK(canceled.request_stop(), "the caller cancels the native source selection");
  BeginCppAllocationProbe();
  const auto stopped = SelectBridgeInputRequirements(source, seeds, kLimits, canceled.get_token());
  const auto stoppedAllocations = EndCppAllocationProbe();
  CHECK(!stopped && stopped.error().Code == BridgeInputErrorCode::Canceled &&
            stoppedAllocations.Calls == 0,
        "cancellation cannot masquerade as a satisfied source requirement");
}
}

int main() {
  CheckFarConditions();
  CheckOrderingAndUnqualified();
  CheckUngroundedCycle();
  CheckAdmission();
  return Report();
}
