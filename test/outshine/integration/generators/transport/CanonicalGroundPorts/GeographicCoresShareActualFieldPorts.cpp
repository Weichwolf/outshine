#include "CanonicalGroundFieldFixture.h"
#include "Check.h"
#include "CppAllocationProbe.h"
#include "GroundPortCore.h"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <optional>
#include <print>
#include <ratio>
#include <stop_token>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
namespace Fixture = CanonicalGroundFixture;
constexpr TransportCoreLimits kLimits{.SourceNodesMost = 16384,
                                      .SourceEdgesMost = 32768,
                                      .SegmentsMost = 4096,
                                      .ContainerCapacityMost = 1048576};

bool SameEnd(const TransportCoreEnd &a, const TransportCoreEnd &b) {
  return std::bit_cast<uint64_t>(a.Fraction) == std::bit_cast<uint64_t>(b.Fraction) &&
         std::bit_cast<uint64_t>(a.Geographic.LongitudeDeg) ==
             std::bit_cast<uint64_t>(b.Geographic.LongitudeDeg) &&
         std::bit_cast<uint64_t>(a.Geographic.LatitudeDeg) ==
             std::bit_cast<uint64_t>(b.Geographic.LatitudeDeg) &&
         a.Boundaries == b.Boundaries;
}

struct Pair {
  std::array<GeoCellId, 2> Cores;
  uint32_t Edge;
  TransportCoreEnd End;
};

std::optional<GeoCellId> Neighbor(GeoCellId core, const TransportBoundary &boundary) {
  const uint32_t count = uint32_t{1} << static_cast<unsigned>(core.Level);
  if (boundary.Axis == TransportBoundaryAxis::Longitude) {
    core.X = boundary.Line == core.X ? (core.X + count - 1) % count : (core.X + 1) % count;
  } else if (boundary.Line == core.Y) {
    if (core.Y == 0) { return {}; }
    --core.Y;
  } else {
    if (core.Y + 1 == count) { return {}; }
    ++core.Y;
  }
  return core;
}

bool Contains(const TransportCoreSelection &selection, uint32_t edge, const TransportCoreEnd &end) {
  return std::ranges::any_of(selection.Segments, [&](const auto &segment) {
    return segment.SourceEdge == edge && std::ranges::any_of(segment.Ends, [&](const auto &value) {
             return SameEnd(value, end);
           });
  });
}

std::optional<Pair>
Across(const GroundPortField &field, GeoCellId core, const TransportCoreSegment &segment) {
  for (const auto &end : segment.Ends) {
    const auto &boundary = end.Boundaries[0];
    if (end.Fraction <= 0 || end.Fraction >= 1 || !boundary) { continue; }
    const auto neighbor = Neighbor(core, *boundary);
    if (!neighbor) { continue; }
    const auto other = SelectGroundPortCore(field, *neighbor, kLimits);
    if (other && Contains(*other, segment.SourceEdge, end)) {
      return Pair{.Cores = {core, *neighbor}, .Edge = segment.SourceEdge, .End = end};
    }
  }
  return {};
}

std::optional<Pair> FindPair(const GroundPortField &field) {
  const auto &graph = field.Source().Source->Topology();
  for (const int level : {19, 20, 21}) {
    const auto count = static_cast<double>(uint32_t{1} << static_cast<unsigned>(level));
    for (const auto edge : field.Source().SourceEdges) {
      const auto &original = graph.Edges()[edge];
      const auto *a = graph.FindNode(original.FromNodeId);
      const auto *b = graph.FindNode(original.ToNodeId);
      if (a == nullptr || b == nullptr) { return {}; }
      const GeoCellId core{
          .Level = level,
          .X = static_cast<uint32_t>((std::midpoint(a->LongitudeDeg, b->LongitudeDeg) + 180) / 360 *
                                     count),
          .Y = static_cast<uint32_t>((std::midpoint(a->LatitudeDeg, b->LatitudeDeg) + 90) / 180 *
                                     count)};
      const auto selected = SelectGroundPortCore(field, core, kLimits);
      if (!selected) { return {}; }
      for (const auto &segment : selected->Segments) {
        auto pair = Across(field, core, segment);
        if (pair) { return pair; }
      }
    }
  }
  return {};
}

void CheckSelection(const GroundPortField &field, const TransportCoreSelection &selected) {
  const auto baseline = SelectTransportCore(selected.Core, field.Source().Source, kLimits);
  CHECK(baseline.has_value(), "the existing full native selector remains an independent reference");
  if (!baseline) { return; }
  size_t used = 0;
  for (const auto &segment : baseline->Segments) {
    if (std::ranges::find(field.Source().SourceEdges, segment.SourceEdge) ==
        field.Source().SourceEdges.end()) {
      continue;
    }
    ++used;
    const auto found =
        std::ranges::find(selected.Segments, segment.SourceEdge, &TransportCoreSegment::SourceEdge);
    CHECK(
        found != selected.Segments.end() && SameEnd(found->Ends[0], segment.Ends[0]) &&
            SameEnd(found->Ends[1], segment.Ends[1]),
        "all real fractions, geographic anchors and both boundary IDs equal the global reference");
  }
  CHECK(used == selected.Segments.size() && selected.Source == field.Source().Source &&
            selected.Work.ValidatedNodes == 0 && selected.Work.ValidatedEdges == 0 &&
            selected.Work.SourceEdgesVisited == 2 * field.Source().SourceEdges.size(),
        "selection owns the same qualified immutable lease and touches only its physical members");
  std::println("GROUND_CORE_WORK members={} local_edge_visits={} global_edge_visits={} "
               "global_validated_nodes={} global_validated_edges={} selected_segments={}",
               field.Source().SourceEdges.size(),
               selected.Work.SourceEdgesVisited,
               baseline->Work.SourceEdgesVisited,
               baseline->Work.ValidatedNodes,
               baseline->Work.ValidatedEdges,
               selected.Segments.size());
}

auto Words(const RoadPortFrameSample &sample) {
  const auto &gate = sample.Port.Gate;
  return std::bit_cast<std::array<uint64_t, 13>>(std::array{gate.EastM,
                                                            gate.NorthM,
                                                            gate.GradeM,
                                                            gate.OutE,
                                                            gate.OutN,
                                                            gate.HalfWidthM,
                                                            gate.ShoulderM,
                                                            gate.ThicknessM,
                                                            sample.Port.Plane.SlopeE,
                                                            sample.Port.Plane.SlopeN,
                                                            sample.LongitudinalGrade,
                                                            sample.CrossfallRad,
                                                            sample.RayResidualM});
}

std::optional<RoadPortFrameSample> CheckPair(const GroundPortField &field, const Pair &pair) {
  std::optional<RoadPortFrameSample> first;
  for (const bool reverse : {false, true}) {
    for (const size_t step : {size_t{0}, size_t{1}}) {
      const auto core = pair.Cores[reverse ? 1 - step : step];
      auto limits = kLimits;
      if (reverse) {
        limits.SourceNodesMost *= 2;
        limits.SourceEdgesMost *= 2;
        limits.SegmentsMost *= 2;
        limits.ContainerCapacityMost *= 2;
      }
      const auto start = std::chrono::steady_clock::now();
      BeginCppAllocationProbe();
      const auto selected = SelectGroundPortCore(field, core, limits);
      const auto allocations = EndCppAllocationProbe();
      const auto ms =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
              .count();
      CHECK(selected && allocations.Calls == 1,
            "geographic selection allocates only its admitted final source-segment output");
      if (!selected) { return {}; }
      CheckSelection(field, *selected);
      CHECK(Contains(*selected, pair.Edge, pair.End),
            "both independent core requests contain the same actual shared source cut");
      const std::array points{GroundPortFieldRequestPoint{
          .Edge = field.Source().Source->Topology().Edges()[pair.Edge].Id,
          .Anchor = pair.End.Geographic}};
      const auto sampled = field.Sample(points);
      CHECK(sampled && sampled->Ports.size() == 1,
            "the existing native field returns its real own-ENU height, normal and original "
            "direction");
      if (!sampled || sampled->Ports.size() != 1) { return {}; }
      const auto &port = sampled->Ports[0];
      if (!first) { first = port; }
      CHECK(Words(*first) == Words(port) && first->RayIterations == port.RayIterations,
            "every actual port word is identical for adjacent cores in either construction order");
      std::println(
          "GROUND_CORE ms={} allocations={} requested_bytes={} height_m={} ray_residual_m={}",
          ms,
          allocations.Calls,
          allocations.Bytes,
          port.Port.Gate.GradeM,
          port.RayResidualM);
    }
  }
  return first;
}

void CheckAdmission(const GroundPortField &field, GeoCellId core) {
  constexpr std::array expected{TransportCoreError::InvalidCore,
                                TransportCoreError::CapacityExceeded,
                                TransportCoreError::CapacityExceeded,
                                TransportCoreError::Canceled,
                                TransportCoreError::CapacityExceeded};
  for (const unsigned control : {0U, 1U, 2U, 3U, 4U}) {
    auto limits = kLimits;
    auto where = core;
    std::stop_source stop;
    if (control == 0) { where.Level = GeoCellId::MaximumLevel + 1; }
    if (control == 1) { limits.SourceNodesMost = 0; }
    if (control == 2) { limits.SegmentsMost = 0; }
    if (control == 3) { (void)stop.request_stop(); }
    if (control == 4) { limits.ContainerCapacityMost = 0; }
    BeginCppAllocationProbe();
    const auto result = SelectGroundPortCore(field, where, limits, stop.get_token());
    const auto allocations = EndCppAllocationProbe();
    CHECK(!result && result.error() == expected[control] && allocations.Calls == 0,
          "invalid cores, source/output caps and cancellation refuse before output allocation or "
          "DEM");
  }
  BeginCppAllocationProbe();
  const auto empty = SelectGroundPortCore(field, {.Level = 20, .X = 0, .Y = 0}, kLimits);
  const auto allocations = EndCppAllocationProbe();
  CHECK(empty && empty->Segments.empty() && allocations.Calls == 0,
        "an uncovered distant core returns no field members and allocates no empty payload");
}
}

int main() {
  auto fields = Fixture::LoadFields();
  CHECK(fields.size() == 4,
        "all actual affine/C1/scoped/republication inputs produce their full fields");
  if (fields.size() != 4) { return Report(); }
  std::vector<RoadPortFrameSample> ports;
  for (const auto &field : fields) {
    const auto pair = FindPair(field);
    CHECK(pair.has_value(),
          "an actual internal geographic boundary crosses the original finite field");
    if (!pair) { continue; }
    const auto port = CheckPair(field, *pair);
    if (port) { ports.push_back(*port); }
    CheckAdmission(field, pair->Cores[0]);
  }
  CHECK(ports.size() == 4 && Words(ports[2]) == Words(ports[3]),
        "real identical source republication also preserves the complete native geographic port "
        "value");
  return Report();
}
