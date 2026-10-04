#include "WaterField.h"

#include "WaterBoundaryIntervals.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>
#include <vector>

namespace outshine::Generators::Osm {
namespace {
struct SourceScope {
  Data::TileSourceIdentity::Origin Origin;
  Data::DataKind SourceKind;
  std::string_view SourceId, Revision;
  uint16_t Layer;
  auto operator<=>(const SourceScope &) const = default;
};

struct BodyKey {
  SourceScope Source;
  std::string_view Kind;
  uint64_t Id;
  auto operator<=>(const BodyKey &) const = default;
};

struct Boundary {
  BodyKey Body;
  WaterBoundaryCut Cut;
  double Seam;
  bool High;
  size_t Surface;
  std::optional<std::vector<WaterBoundaryInterval>> Intervals;
};

bool StandingWater(std::string_view kind) {
  return kind == "lake" || kind == "pond" || kind == "dock" || kind == "reservoir";
}

SourceScope ScopeOf(const OsmField &field, const OsmField::Feature &feature) {
  const auto &source = field.Tiles()[feature.Tile].Source;
  return {.Origin = source.From,
          .SourceKind = source.Kind,
          .SourceId = source.SourceId,
          .Revision = source.Revision,
          .Layer = feature.Layer};
}

void AppendBoundaries(std::vector<Boundary> &into,
                      const OsmField &field,
                      const OsmField::Feature &feature,
                      size_t surfaceAt) {
  const auto kind = field.Str(feature, "kind");
  if (!feature.ProviderFeatureId || !StandingWater(kind)) { return; }
  const auto &tile = field.Tiles()[feature.Tile];
  const BodyKey body{
      .Source = ScopeOf(field, feature), .Kind = kind, .Id = *feature.ProviderFeatureId};
  const double span = std::ldexp(1.0, -tile.Z);
  const double x = static_cast<double>(tile.X) * span;
  const double y = static_cast<double>(tile.Y) * span;
  for (const auto axis : {WaterBoundaryAxis::X, WaterBoundaryAxis::Y}) {
    const double coordinate = axis == WaterBoundaryAxis::X ? x : y;
    const double from = axis == WaterBoundaryAxis::X ? y : x;
    for (const bool high : {false, true}) {
      const double cut = coordinate + (high ? span : 0.0);
      const double seam = axis == WaterBoundaryAxis::X && cut == 1.0 ? 0.0 : cut;
      into.push_back({.Body = body,
                      .Cut = {.Axis = axis, .Coordinate = cut, .From = from, .To = from + span},
                      .Seam = seam,
                      .High = high,
                      .Surface = surfaceAt,
                      .Intervals = std::nullopt});
    }
  }
}

size_t Root(std::span<size_t> parents, size_t at) {
  while (parents[at] != at) {
    parents[at] = parents[parents[at]];
    at = parents[at];
  }
  return at;
}

std::span<const WaterBoundaryInterval>
Intervals(Boundary &boundary, const OsmField &field, const WaterField &water) {
  if (!boundary.Intervals) {
    boundary.Intervals = WaterBoundaryIntervals(
        field, water.RingsOf(water.Surfaces()[boundary.Surface]), boundary.Cut);
  }
  return *boundary.Intervals;
}

void ConnectBoundaries(std::vector<Boundary> &boundaries,
                       const OsmField &field,
                       const WaterField &water,
                       std::span<size_t> parents) {
  const auto group = [](const Boundary &one) { return std::tie(one.Body, one.Cut.Axis, one.Seam); };
  std::ranges::sort(boundaries, [&group](const Boundary &a, const Boundary &b) {
    return std::tuple(group(a), a.Cut.From, a.Cut.To, a.High, a.Surface) <
           std::tuple(group(b), b.Cut.From, b.Cut.To, b.High, b.Surface);
  });
  std::vector<size_t> active;
  for (size_t at = 0; at < boundaries.size(); ++at) {
    Boundary &one = boundaries[at];
    if (at == 0 || group(one) != group(boundaries[at - 1])) { active.clear(); }
    std::erase_if(active, [&](size_t index) { return boundaries[index].Cut.To <= one.Cut.From; });
    for (const size_t otherAt : active) {
      Boundary &other = boundaries[otherAt];
      const size_t a = Root(parents, one.Surface);
      const size_t b = Root(parents, other.Surface);
      if (one.High == other.High || a == b) { continue; }
      if (WaterBoundariesOverlap(Intervals(one, field, water), Intervals(other, field, water))) {
        parents[std::max(a, b)] = std::min(a, b);
      }
    }
    active.push_back(at);
  }
}

using BoundaryPoint = std::array<int64_t, 2>;

struct SharedEdge {
  SourceScope Source;
  BoundaryPoint First, Last;
  bool Forward;
  size_t Surface;
};

BoundaryPoint BoundaryPointOf(std::span<const double> points, size_t at) {
  const auto projected = WaterBoundaryPosition(points, at);
  return {std::llround(projected[0] / kWaterBoundaryTolerance),
          std::llround(projected[1] / kWaterBoundaryTolerance)};
}

void AppendSharedEdges(std::vector<SharedEdge> &into,
                       const OsmField &field,
                       const WaterField &water,
                       const OsmField::Feature &feature,
                       size_t surfaceAt) {
  const auto kind = field.Str(feature, "kind");
  if (!feature.ProviderFeatureId || (!StandingWater(kind) && kind != "ocean")) { return; }
  const auto &ring = water.RingsOf(water.Surfaces()[surfaceAt]).front();
  const auto scope = ScopeOf(field, feature);
  BoundaryPoint previous = BoundaryPointOf(field.Points(), ring.FirstPoint + ring.PointCount - 1);
  for (size_t step = 0; step < ring.PointCount; ++step) {
    const auto current =
        BoundaryPointOf(field.Points(), static_cast<size_t>(ring.FirstPoint) + step);
    if (previous != current) {
      into.push_back({.Source = scope,
                      .First = std::min(previous, current),
                      .Last = std::max(previous, current),
                      .Forward = previous < current,
                      .Surface = surfaceAt});
    }
    previous = current;
  }
}

void ConnectSharedEdges(std::vector<SharedEdge> &edges, std::span<size_t> parents) {
  const auto key = [](const SharedEdge &one) { return std::tie(one.Source, one.First, one.Last); };
  std::ranges::sort(edges, [&key](const SharedEdge &a, const SharedEdge &b) {
    return std::tuple(key(a), a.Forward, a.Surface) < std::tuple(key(b), b.Forward, b.Surface);
  });
  size_t group = 0;
  while (group < edges.size()) {
    size_t end = group + 1;
    while (end < edges.size() && key(edges[end]) == key(edges[group])) { ++end; }
    if (edges[group].Forward != edges[end - 1].Forward) {
      for (size_t at = group + 1; at < end; ++at) {
        const size_t one = Root(parents, edges[group].Surface);
        const size_t other = Root(parents, edges[at].Surface);
        parents[std::max(one, other)] = std::min(one, other);
      }
    }
    group = end;
  }
}
}

void WaterField::ResolveSurfaceLevels(const OsmField &field) {
  if (SurfaceInputs_.empty()) { return; }
  std::vector<Boundary> boundaries;
  boundaries.reserve(SurfaceInputs_.size() * 4);
  for (size_t at = 0; at < SurfaceInputs_.size(); ++at) {
    AppendBoundaries(boundaries, field, field.Features()[SurfaceInputs_[at].Feature], at);
  }
  std::vector<size_t> parents(Surfaces_.size());
  std::ranges::iota(parents, size_t{0});
  ConnectBoundaries(boundaries, field, *this, parents);
  std::vector<SharedEdge> edges;
  for (size_t at = 0; at < SurfaceInputs_.size(); ++at) {
    AppendSharedEdges(edges, field, *this, field.Features()[SurfaceInputs_[at].Feature], at);
  }
  ConnectSharedEdges(edges, parents);
  std::vector<size_t> counts(Surfaces_.size());
  for (size_t at = 0; at < parents.size(); ++at) { ++counts[Root(parents, at)]; }
  std::vector<std::vector<double>> samples(Surfaces_.size());
  std::vector<bool> ocean(Surfaces_.size());
  for (size_t at = 0; at < SurfaceInputs_.size(); ++at) {
    const size_t root = Root(parents, at);
    ocean[root] =
        ocean[root] || field.Str(field.Features()[SurfaceInputs_[at].Feature], "kind") == "ocean";
    if (counts[root] <= 1) { continue; }
    const auto &heights = SurfaceInputs_[at].Heights;
    samples[root].insert(samples[root].end(), heights.begin(), heights.end());
  }
  for (size_t at = 0; at < samples.size(); ++at) {
    if (ocean[at]) {
      Surfaces_[at].LevelM = 0.0f;
    } else if (const auto level = SurfaceLevel(samples[at])) {
      Surfaces_[at].LevelM = *level;
    }
  }
  for (size_t at = 0; at < Surfaces_.size(); ++at) {
    Surfaces_[at].LevelM = Surfaces_[Root(parents, at)].LevelM;
  }
  RecountSurfaceOutliers();
  SurfaceInputs_.clear();
  SurfaceInputs_.shrink_to_fit();
}
}
