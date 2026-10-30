#include "GroundPortBodySliceInternal.h"
#include "GroundPortBodyIdentity.h"
#include "GroundPortCore.h"
#include "ByteArchive.h"
#include "Sha256.h"
#include "TransportNetworkSnapshot.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <stop_token>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
using namespace GroundBodySlices;

auto Key(const World::TransportEdgeId &edge) {
  return std::tuple{edge.PathId, edge.SegmentOrdinal, edge.Direction};
}

bool Same(const TransportInputReference &a, const TransportInputReference &b) {
  return a.DatasetId == b.DatasetId && a.Version == b.Version && a.Revision == b.Revision &&
         a.ItemId == b.ItemId && a.Content == b.Content;
}

bool Same(const std::optional<GroundPortFieldScopeInfo> &a,
          const std::optional<GroundPortFieldScopeInfo> &b) {
  return a.has_value() == b.has_value() &&
         (!a || (Same(a->Input, b->Input) && a->ContentBinding == b->ContentBinding &&
                 a->Exits == b->Exits));
}

bool Same(const TransportCoreSegment &a, const TransportCoreSegment &b) {
  if (a.SourceEdge != b.SourceEdge) { return false; }
  for (size_t end = 0; end < 2; ++end) {
    if (a.Ends[end].Fraction != b.Ends[end].Fraction ||
        a.Ends[end].Geographic != b.Ends[end].Geographic ||
        a.Ends[end].Boundaries != b.Ends[end].Boundaries) {
      return false;
    }
  }
  return true;
}

Checked<std::vector<TransportCoreSegment>> Intervals(const GroundPortField &field,
                                                     const TransportCoreSelection &selection,
                                                     GroundPortBodyBudget budget,
                                                     const std::stop_token &stop) {
  const auto &graph = field.Source().Source->Topology();
  auto canonical = SelectGroundPortCore(field,
                                        selection.Core,
                                        {.SourceNodesMost = graph.Nodes().size(),
                                         .SourceEdgesMost = graph.Edges().size(),
                                         .SegmentsMost = budget.Bodies,
                                         .ContainerCapacityMost = budget.ContainerBytes},
                                        stop);
  if (!canonical) {
    const auto error = canonical.error();
    if (error == TransportCoreError::Canceled) {
      return std::unexpected(Error{RoadGroundError::Canceled});
    }
    if (error == TransportCoreError::CapacityExceeded) {
      return std::unexpected(Error{RoadGroundError::CapacityExceeded});
    }
    return std::unexpected(Error{HeightSolidError::InvalidInput});
  }
  auto requested = selection.Segments;
  const auto order = [](const auto &segment) {
    return std::tuple{segment.SourceEdge, segment.Ends[0].Fraction, segment.Ends[1].Fraction};
  };
  std::ranges::sort(requested, {}, order);
  auto &segments = canonical->Segments;
  std::ranges::sort(segments, {}, order);
  if (!std::ranges::equal(
          requested, segments, [](const auto &a, const auto &b) { return Same(a, b); })) {
    return std::unexpected(Error{HeightSolidError::InvalidInput});
  }
  size_t kept = 0;
  for (const auto &segment : segments) {
    if (kept > 0 && segments[kept - 1].SourceEdge == segment.SourceEdge &&
        segments[kept - 1].Ends[1].Fraction == segment.Ends[0].Fraction) {
      segments[kept - 1].Ends[1] = segment.Ends[1];
    } else {
      segments[kept++] = segment;
    }
  }
  segments.resize(kept);
  return std::move(segments);
}

Checked<size_t> Bound(const GroundPortBodies &source,
                      std::span<const TransportCoreSegment> intervals,
                      GroundPortBodyBudget budget) {
  size_t remaining = budget.ContainerBytes;
  const auto fits = [&](size_t count, size_t width) {
    if (count > remaining / width) { return false; }
    remaining -= count * width;
    return true;
  };
  if (!fits(intervals.size(), 4 * sizeof(TransportCoreSegment) + 2 * sizeof(GroundPortBody))) {
    return std::unexpected(Error{RoadGroundError::CapacityExceeded});
  }
  const auto text = [&](const TransportInputReference &input) {
    return fits(input.DatasetId.size() + input.Version.size() + input.Revision.size() +
                    input.ItemId.size() + 4,
                2);
  };
  if (!text(source.Inputs[0]) || !text(source.Inputs[1]) ||
      (source.Scope && (!text(source.Scope->Input) ||
                        !fits(source.Scope->Exits.size(), 2 * sizeof(GroundPortFieldExit))))) {
    return std::unexpected(Error{RoadGroundError::CapacityExceeded});
  }
  constexpr size_t solidFace = 6 * sizeof(std::array<double, 3>) + 8 * sizeof(PlanarFace) +
                               6 * (sizeof(uint32_t) + sizeof(HeightSolidWallKind));
  constexpr size_t contactFace = sizeof(EarthworkStamp) + 12 * sizeof(double);
  constexpr size_t inputScratch = 3 * (sizeof(PlanarPoint) + 4 * sizeof(double)) +
                                  6 * sizeof(uint32_t) + sizeof(PlanarFace) +
                                  6 * sizeof(std::array<uint32_t, 2>);
  constexpr size_t clippedScratch = 7 * (sizeof(PlanarPoint) + 4 * sizeof(double) +
                                         sizeof(std::array<uint32_t, 2>) + sizeof(uint32_t)) +
                                    3 * sizeof(uint32_t) +
                                    4 * (sizeof(PlanarFace) + sizeof(uint32_t));
  constexpr size_t solidScratch = 5 * (sizeof(PlanarPoint) + sizeof(double)) +
                                  3 * (sizeof(PlanarFace) + sizeof(std::array<double, 3>));
  constexpr size_t scratchFace = 2 * (inputScratch + clippedScratch + solidScratch);
  const auto edges = source.Source->Topology().Edges();
  for (const auto &interval : intervals) {
    const auto id = edges[interval.SourceEdge].Id;
    const auto body = std::ranges::lower_bound(
        source.Bodies, Key(id), {}, [](const auto &value) { return Key(value.Source); });
    if (body == source.Bodies.end() || body->Source != id || body->SourceBegin != 0 ||
        body->SourceEnd != 1) {
      return std::unexpected(Error{HeightSolidError::InvalidInput});
    }
    if (!fits(body->Geometry.SurfaceFaces, 6 * (solidFace + contactFace) + scratchFace)) {
      return std::unexpected(Error{RoadGroundError::CapacityExceeded});
    }
  }
  return budget.ContainerBytes - remaining;
}

std::array<uint8_t, 32>
Identity(const std::array<uint8_t, 32> &base, GeoCellId core, const GroundPortBodies &sliced) {
  constexpr std::string_view kVersion = "outshine/canonical-ground-core-bodies/1";
  Sha256Prefix hash(kVersion.data(), kVersion.size());
  hash.Append(base.data(), base.size());
  const auto word = [&](auto value) {
    const auto encoded = LittleEndian(value);
    hash.Append(&encoded, sizeof(encoded));
  };
  word(static_cast<uint32_t>(core.Level));
  word(core.X);
  word(core.Y);
  word(static_cast<uint64_t>(sliced.Bodies.size()));
  for (const auto &body : sliced.Bodies) {
    word(body.Source.PathId);
    word(body.Source.SegmentOrdinal);
    word(static_cast<uint8_t>(body.Source.Direction));
    word(body.SourceBegin);
    word(body.SourceEnd);
  }
  return hash.Digest(nullptr, 0);
}
}

std::expected<GroundPortBodies, GroundPortBodyError>
SliceGroundPortBodies(const GroundPortField &field,
                      const GroundPortBodies &source,
                      const TransportCoreSelection &selection,
                      GroundPortBodyBudget budget,
                      const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error{RoadGroundError::Canceled}); }
  if (source.Source != field.Source().Source || source.Source != selection.Source || source.Core ||
      source.Anchor != field.Domain().Anchor || !selection.Core.Valid() ||
      source.ProcedureAndTerrainBinding != field.ProcedureAndTerrainBinding() ||
      !Same(source.Inputs[0], field.Fit().RecipeInput) ||
      !Same(source.Inputs[1], field.Terrain().Input()) || !Same(source.Scope, field.Scope()) ||
      source.ContentBinding != GroundPortBodyContentBinding(field, source) ||
      !std::ranges::is_sorted(
          source.Bodies, {}, [](const auto &body) { return Key(body.Source); })) {
    return std::unexpected(Error{HeightSolidError::InvalidInput});
  }
  if (selection.Segments.size() > budget.Bodies ||
      selection.Segments.size() > budget.ContainerBytes / (4 * sizeof(TransportCoreSegment))) {
    return std::unexpected(Error{RoadGroundError::CapacityExceeded});
  }
  for (const auto &segment : selection.Segments) {
    const double begin = segment.Ends[0].Fraction;
    const double end = segment.Ends[1].Fraction;
    if (!std::isfinite(begin) || !std::isfinite(end) || begin < 0 || end > 1 || begin >= end) {
      return std::unexpected(Error{HeightSolidError::InvalidInput});
    }
  }
  auto intervals = Intervals(field, selection, budget, stop);
  if (!intervals) { return std::unexpected(intervals.error()); }
  const auto bound = Bound(source, *intervals, budget);
  if (!bound) { return std::unexpected(bound.error()); }
  GroundPortBodies output{.Anchor = source.Anchor,
                          .Source = source.Source,
                          .Inputs = source.Inputs,
                          .ProcedureAndTerrainBinding = source.ProcedureAndTerrainBinding,
                          .Recipe = source.Recipe,
                          .Scope = source.Scope,
                          .Core = selection.Core,
                          .Bodies = {},
                          .Work = {}};
  output.Work.ContainerBoundBytes = *bound;
  output.Bodies.reserve(intervals->size());
  for (const auto &interval : *intervals) {
    if (stop.stop_requested()) { return std::unexpected(Error{RoadGroundError::Canceled}); }
    const auto id = source.Source->Topology().Edges()[interval.SourceEdge].Id;
    const auto base = std::ranges::lower_bound(
        source.Bodies, Key(id), {}, [](const auto &value) { return Key(value.Source); });
    auto body = GroundBodySlices::Build(field, *base, interval, budget, output.Work, stop);
    if (!body) { return std::unexpected(body.error()); }
    if (body->Geometry.SurfaceFaces > budget.SurfaceFaces) {
      return std::unexpected(Error{RoadGroundError::CapacityExceeded});
    }
    budget.SurfaceFaces -= body->Geometry.SurfaceFaces;
    output.Bodies.push_back(std::move(*body));
  }
  output.Work.ContainerCapacityBytes = GroundPortBodyCapacityBytes(output);
  if (output.Work.ContainerCapacityBytes > budget.ContainerBytes) {
    return std::unexpected(Error{RoadGroundError::CapacityExceeded});
  }
  output.ContentBinding = Identity(source.ContentBinding, selection.Core, output);
  return output;
}
}
