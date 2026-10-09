#include "BuildingInputs.h"
#include "BuildingProperties.h"
#include "Log.h"
#include "OsmLayer.h"
#include "StructureSourceKey.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

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
    if (building.Hidden) { continue; }
    if (building.WallColourRejected) {
      Log::Error(LogTag::World,
                 "building_colour_rejected",
                 {{"featureIndex", static_cast<int>(at)},
                  {"colour", std::string(vectors.Str(f, "building:colour"))}});
    }
    const auto heights = HeightsOf(building, vectors, f);
    const int pitched = PitchedOf(vectors.Str(f, "roof:shape"));
    for (uint32_t r = 0; r < f.RingCount; ++r) {
      const OsmField::Ring &ring = vectors.Rings()[f.FirstRing + r];
      if (!ring.Exterior || ring.Count < 3 || ring.Count > kMostRingPoints) { continue; }
      const size_t ringFirst = static_cast<size_t>(ring.First) * 2u;
      const size_t ringLength = static_cast<size_t>(ring.Count) * 2u;
      const std::span<const double> ringPoints(points.data() + ringFirst, ringLength);
      const auto assignedCell = Generators::StructureCellOf(tileBounds, ringPoints);
      if (raw.RequestedCell && (!assignedCell || assignedCell->Index != *raw.RequestedCell)) {
        continue;
      }
      const auto local = static_cast<uint32_t>(raw.LatLon.size() / 2);
      raw.LatLon.insert(raw.LatLon.end(),
                        points.begin() + static_cast<long>(ring.First) * 2,
                        points.begin() + static_cast<long>(ring.First + ring.Count) * 2);
      const auto firstHole = static_cast<uint32_t>(raw.Holes.size());
      AppendInnerRings(
          vectors.Rings().subspan(f.FirstRing + r + 1, f.RingCount - r - 1), points, raw);
      raw.Structures.push_back({.LocalFirst = local,
                                .PointCount = ring.Count,
                                .SourceFirst = ring.First,
                                .FirstHole = firstHole,
                                .HoleCount = static_cast<uint32_t>(raw.Holes.size()) - firstHole,
                                .SourceFirstHole = f.FirstRing + r + 1,
                                .Cell = assignedCell.value_or(Generators::StructureCell{}),
                                .HeightM = heights.TopM,
                                .MinimumHeightM = heights.MinimumM,
                                .Pitched = pitched,
                                .WallColour = building.WallColour,
                                .Facade = building.Facade,
                                .HeightOrigin = heights.Origin});
    }
  }
}

}
