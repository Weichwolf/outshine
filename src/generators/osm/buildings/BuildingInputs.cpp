#include "BuildingInputs.h"
#include "BuildingCanopy.h"
#include "BuildingProperties.h"
#include "Log.h"
#include "OsmLayer.h"
#include "StructureSourceKey.h"
#include "spatial/GeosPolygon.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
namespace {

constexpr uint32_t kMostRingPoints = 512;
constexpr uint8_t kPolygonFeature = 3;

int PitchedOf(std::string_view said) {
  if (said.empty()) { return -1; }
  return said == "flat" ? 0 : 1;
}

void AppendInnerRings(std::span<const GeographicRing> rings,
                      std::span<const double> points,
                      Generators::RawTile &raw) {
  for (const auto &hole : rings) {
    if (hole.Exterior) { break; }
    const auto holeFirst = static_cast<uint32_t>(raw.LatLon.size() / 2);
    const auto contour =
        points.subspan(static_cast<size_t>(hole.First) * 2, static_cast<size_t>(hole.Count) * 2);
    raw.LatLon.insert(raw.LatLon.end(), contour.begin(), contour.end());
    raw.Holes.push_back({.First = holeFirst, .Count = hole.Count, .Exterior = false});
  }
}

struct StructureHeights {
  double TopM, MinimumM;
  std::optional<Ground::BuildingHeightOrigin> Origin;
};

StructureHeights HeightsOf(const BuildingProperties &building,
                           const OsmField &vectors,
                           const OsmField::Feature &feature) {
  return {.TopM = building.Height ? building.Height->TopM : vectors.Num(feature, "height", 0.0),
          .MinimumM =
              building.Height ? building.Height->MinimumM : vectors.Num(feature, "min_height", 0.0),
          .Origin = building.Height ? std::optional(building.Height->TopOrigin) : std::nullopt};
}

struct PolygonContext {
  GEOSContextHandle_t Handle = GEOS_init_r();

  ~PolygonContext() { GEOS_finish_r(Handle); }
};

Spatial::GeosGeometry Footprint(GEOSContextHandle_t context,
                                const OsmField &vectors,
                                std::span<const GeographicRing> rings) {
  std::vector<PolygonRing> polygon;
  for (const auto &ring : rings) {
    if (!polygon.empty() && ring.Exterior) { break; }
    if (ring.Count < 3 || ring.Count > kMostRingPoints) { return {nullptr, {context}}; }
    auto &part = polygon.emplace_back();
    part.reserve(ring.Count);
    const auto points = vectors.Points().subspan(size_t{ring.First} * 2u, size_t{ring.Count} * 2u);
    for (size_t at = 0; at < points.size(); at += 2) {
      part.push_back({points[at + 1], points[at]});
    }
  }
  auto shape = Spatial::MakeGeosPolygon(context, polygon);
  if (shape && GEOSisValid_r(context, shape.get()) != 1) { shape.reset(); }
  return shape;
}

Ground::GeoBounds RingBounds(const OsmField &vectors, const GeographicRing &ring) {
  const auto points = vectors.Points().subspan(size_t{ring.First} * 2u, size_t{ring.Count} * 2u);
  Ground::GeoBounds bounds{.MinLonDeg = points[1],
                           .MinLatDeg = points[0],
                           .MaxLonDeg = points[1],
                           .MaxLatDeg = points[0]};
  for (size_t at = 2; at < points.size(); at += 2) {
    bounds.MinLatDeg = std::min(bounds.MinLatDeg, points[at]);
    bounds.MaxLatDeg = std::max(bounds.MaxLatDeg, points[at]);
    bounds.MinLonDeg = std::min(bounds.MinLonDeg, points[at + 1]);
    bounds.MaxLonDeg = std::max(bounds.MaxLonDeg, points[at + 1]);
  }
  return bounds;
}

bool Overlaps(
    const Ground::GeoBounds &bounds, double south, double west, double north, double east) {
  return south <= bounds.MaxLatDeg && north >= bounds.MinLatDeg && west <= bounds.MaxLonDeg &&
         east >= bounds.MinLonDeg;
}

struct SupportParts {
  GEOSContextHandle_t Context;
  const GEOSGeometry *Outline;
  Ground::GeoBounds Bounds;
  double SoleM;
  std::vector<Spatial::GeosGeometry> Lowest;
  std::vector<Spatial::GeosGeometry> Grounded;

  void Include(Spatial::GeosGeometry shape, double minimumM) {
    if (!shape || GEOSIntersects_r(Context, Outline, shape.get()) != 1) { return; }
    if (minimumM <= 0.0) {
      Grounded.push_back(std::move(shape));
    } else if (GEOSCovers_r(Context, Outline, shape.get()) == 1) {
      if (minimumM < SoleM) { Lowest.clear(); }
      SoleM = minimumM;
      Lowest.push_back(std::move(shape));
    }
  }

  [[nodiscard]] std::optional<double> SupportHeight() const {
    if (Lowest.empty()) { return std::nullopt; }
    for (const auto &base : Grounded) {
      for (const auto &part : Lowest) {
        if (GEOSIntersects_r(Context, part.get(), base.get()) != 0) { return std::nullopt; }
      }
    }
    return SoleM;
  }
};

void IncludeFeature(SupportParts &support,
                    const OsmField &vectors,
                    const OsmField::Feature &feature) {
  if (feature.Type != kPolygonFeature ||
      !Overlaps(support.Bounds, feature.MinLat, feature.MinLon, feature.MaxLat, feature.MaxLon)) {
    return;
  }
  const double minimumM = vectors.Num(feature, "min_height", 0.0);
  const double topM = vectors.Num(feature, "height", 0.0);
  if (!std::isfinite(minimumM) || !std::isfinite(topM) || minimumM > support.SoleM || topM <= 0.0 ||
      topM <= minimumM || HiddenBuildingOutline(vectors, feature)) {
    return;
  }
  const auto parts = vectors.Rings().subspan(feature.FirstRing, feature.RingCount);
  for (size_t part = 0; part < parts.size(); ++part) {
    const auto &ring = parts[part];
    if (!ring.Exterior || ring.Count < 3 || ring.Count > kMostRingPoints) { continue; }
    const auto other = RingBounds(vectors, ring);
    if (Overlaps(
            support.Bounds, other.MinLatDeg, other.MinLonDeg, other.MaxLatDeg, other.MaxLonDeg)) {
      support.Include(Footprint(support.Context, vectors, parts.subspan(part)), minimumM);
    }
  }
}

std::optional<double> OutlineSupportHeight(const OsmField &vectors,
                                           const OsmField::Feature &outline,
                                           std::span<const GeographicRing> rings,
                                           double outlineTopM) {
  const PolygonContext context;
  if (context.Handle == nullptr) { return std::nullopt; }
  const auto shell = Footprint(context.Handle, vectors, rings);
  if (!shell) { return std::nullopt; }
  SupportParts support{.Context = context.Handle,
                       .Outline = shell.get(),
                       .Bounds = RingBounds(vectors, rings.front()),
                       .SoleM = outlineTopM,
                       .Lowest = {},
                       .Grounded = {}};
  for (const auto &feature : vectors.OfTile(static_cast<int>(outline.Tile))) {
    if (feature.Layer == outline.Layer) { IncludeFeature(support, vectors, feature); }
  }
  return support.SupportHeight();
}

void AppendBuildingRings(const OsmField &vectors,
                         const OsmField::Feature &feature,
                         const BuildingProperties &building,
                         const Ground::GeoBounds &tileBounds,
                         RawTile &raw) {
  const auto heights = HeightsOf(building, vectors, feature);
  if (building.Hidden &&
      (heights.MinimumM != 0.0 || heights.TopM <= 0.0 || !std::isfinite(heights.TopM) ||
       !building.Height || building.Height->ConflictingLevels ||
       vectors.Num(feature, "height", 0.0) <= 0.0)) {
    return;
  }
  const int pitched = building.Hidden ? 0 : PitchedOf(vectors.Str(feature, "roof:shape"));
  const auto points = vectors.Points();
  const auto rings = vectors.Rings().subspan(feature.FirstRing, feature.RingCount);
  for (uint32_t index = 0; index < rings.size(); ++index) {
    const auto &ring = rings[index];
    if (!ring.Exterior || ring.Count < 3 || ring.Count > kMostRingPoints) { continue; }
    const auto ringPoints = points.subspan(size_t{ring.First} * 2u, size_t{ring.Count} * 2u);
    const auto cell = Generators::StructureCellOf(tileBounds, ringPoints);
    if (raw.RequestedCell && (!cell || cell->Index != *raw.RequestedCell)) { continue; }
    const auto topM =
        building.Hidden ? OutlineSupportHeight(vectors, feature, rings.subspan(index), heights.TopM)
                        : std::optional(heights.TopM);
    if (!topM) { continue; }
    const auto local = static_cast<uint32_t>(raw.LatLon.size() / 2);
    raw.LatLon.insert(raw.LatLon.end(), ringPoints.begin(), ringPoints.end());
    const auto firstHole = static_cast<uint32_t>(raw.Holes.size());
    AppendInnerRings(rings.subspan(index + 1), points, raw);
    raw.Structures.push_back({.LocalFirst = local,
                              .PointCount = ring.Count,
                              .SourceFirst = ring.First,
                              .FirstHole = firstHole,
                              .HoleCount = static_cast<uint32_t>(raw.Holes.size()) - firstHole,
                              .SourceFirstHole = feature.FirstRing + index + 1,
                              .Cell = cell.value_or(Generators::StructureCell{}),
                              .HeightM = *topM,
                              .MinimumHeightM = heights.MinimumM,
                              .Pitched = pitched,
                              .WallColour = building.WallColour,
                              .Facade = building.Facade,
                              .HeightOrigin = heights.Origin});
  }
}

}

bool HasCanopySlab(const OsmField &vectors, uint32_t tile) {
  const int layer = vectors.Layer(OsmLayer::Buildings);
  return std::ranges::any_of(vectors.OfTile(static_cast<int>(tile)), [&](const auto &feature) {
    return feature.Type == kPolygonFeature && std::cmp_equal(feature.Layer, layer) &&
           IsCanopySlab({.MinimumM = vectors.Num(feature, "min_height", 0),
                         .TopM = vectors.Num(feature, "height", 0)});
  });
}

void PrepareBuildingInputs(const OsmField &vectors,
                           const BuildingField &prints,
                           const StreetField &streets,
                           const TileAdmission::Next &next,
                           LongitudeLatitude eye,
                           std::optional<LevelOfDetail> detail,
                           std::optional<uint32_t> cell,
                           RawTile &raw) {
  raw.LatLon.clear();
  raw.Structures.clear();
  raw.Holes.clear();
  raw.Ways.clear();
  raw.SourceInputs = {};
  raw.AnchorEcef = prints.Anchor();
  raw.Eye = eye;
  raw.EyeEcef = prints.EyeEcef();
  raw.RequestedDetail = detail;
  raw.RequestedCell = cell;
  raw.Projection = prints.Projection();
  raw.TileSpanM = prints.TileSpanM();
  raw.Extent = vectors.Extent();
  const int layer = vectors.Layer(OsmLayer::Buildings);
  const std::span<const OsmField::Feature> feats = vectors.Features();
  const std::span<const double> points = vectors.Points();
  const OsmField::Tile &tile = vectors.Tiles()[next.Tile];
  const Ground::GeoBounds tileBounds = Ground::TileBounds(
      {.Zoom = tile.Z, .X = static_cast<uint32_t>(tile.X), .Y = static_cast<uint32_t>(tile.Y)});
  for (const StreetField::Way &way : streets.OfTile(static_cast<int>(next.Tile))) {
    const auto first = static_cast<size_t>(way.FirstPoint);
    const auto count = static_cast<size_t>(way.PointCount);
    if (count < 2 || first + count > points.size() / 2) { continue; }
    const auto local = static_cast<uint32_t>(raw.LatLon.size() / 2);
    raw.LatLon.insert(raw.LatLon.end(),
                      points.begin() + static_cast<long>(first) * 2,
                      points.begin() + static_cast<long>(first + count) * 2);
    raw.Ways.push_back(
        {.LocalFirst = local, .PointCount = way.PointCount, .HalfWidthM = way.HalfWidthM});
  }
  for (size_t at = next.From; at < next.To; ++at) {
    const OsmField::Feature &f = feats[at];
    if (f.Type != kPolygonFeature || std::cmp_not_equal(f.Layer, layer)) { continue; }
    const auto building = ReadBuildingProperties(vectors, f);
    if (building.WallColourRejected) {
      Log::Error(LogTag::World,
                 "building_colour_rejected",
                 {{"featureIndex", static_cast<int>(at)},
                  {"colour", std::string(vectors.Str(f, "building:colour"))}});
    }
    AppendBuildingRings(vectors, f, building, tileBounds, raw);
  }
}

}
