#include "TerrainSourceCoverage.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "HeightField.h"
#include "math/Units.h"
#include "OsmField.h"
#include "OsmLayer.h"
#include "TileGeodesy.h"

namespace outshine {
namespace {
constexpr uint8_t kPolygonFeature = 3;

[[nodiscard]] auto TileKey(Data::TileId tile) {
  return std::tuple(tile.Zoom, tile.X, tile.Y);
}

[[nodiscard]] bool ValidTile(Data::TileId tile) {
  if (tile.Zoom < 0 || tile.Zoom > Ground::HeightField::MaximumTileZoom) { return false; }
  const uint32_t side = uint32_t{1} << static_cast<uint32_t>(tile.Zoom);
  return tile.X < side && tile.Y < side;
}

class TilePlan {
public:
  explicit TilePlan(size_t maximum) : Maximum_(maximum) { Tiles_.reserve(maximum); }

  [[nodiscard]] bool Add(Data::TileId tile) {
    const auto at = std::ranges::lower_bound(Tiles_, TileKey(tile), {}, TileKey);
    if (at != Tiles_.end() && *at == tile) { return true; }
    if (Tiles_.size() == Maximum_) { return false; }
    Tiles_.insert(at, tile);
    return true;
  }

  [[nodiscard]] bool Neighbours(Data::TileId tile) {
    for (long dy = -1; dy <= 1; ++dy) {
      for (long dx = -1; dx <= 1; ++dx) {
        long x = static_cast<long>(tile.X) + dx;
        const long y = static_cast<long>(tile.Y) + dy;
        if (!Ground::WrapTile(tile.Zoom, &x, &y)) { continue; }
        if (!Add({.Zoom = tile.Zoom,
                  .X = static_cast<uint32_t>(x),
                  .Y = static_cast<uint32_t>(y)})) {
          return false;
        }
      }
    }
    return true;
  }

  [[nodiscard]] std::vector<Data::TileId> Take() && { return std::move(Tiles_); }

private:
  size_t Maximum_;
  std::vector<Data::TileId> Tiles_;
};

using PlanResult = std::expected<void, std::string>;
constexpr double kPoleLatitudeDeg = 90.0;
constexpr auto kBudgetRefusal = "terrain source coverage exceeds its tile budget";

[[nodiscard]] PlanResult BudgetExceeded() {
  return std::unexpected(kBudgetRefusal);
}

[[nodiscard]] bool ValidBounds(const ::outshine::Generators::Osm::OsmField::Feature &feature) {
  return std::isfinite(feature.MinLon) && std::isfinite(feature.MaxLon) &&
         std::isfinite(feature.MinLat) && std::isfinite(feature.MaxLat) &&
         feature.MinLat >= -kPoleLatitudeDeg && feature.MaxLat <= kPoleLatitudeDeg &&
         feature.MinLon <= feature.MaxLon && feature.MinLat <= feature.MaxLat &&
         feature.MaxLon - feature.MinLon <= kDegPerTurn;
}

[[nodiscard]] uint32_t BoundsRow(double lat, int zoom) {
  const auto at = Ground::ToTileFracClamped({.LongitudeDeg = 0.0, .LatitudeDeg = lat}, zoom);
  const double last = std::ldexp(1.0, zoom) - 1.0;
  return static_cast<uint32_t>(std::clamp(std::floor(at.Y), 0.0, last));
}

[[nodiscard]] PlanResult AddBuilding(TilePlan &plan,
                                     const ::outshine::Generators::Osm::OsmField::Feature &feature,
                                     TerrainSourceCoverage coverage) {
  if (!ValidBounds(feature)) { return std::unexpected("building height bounds are invalid"); }
  const uint32_t side = uint32_t{1} << static_cast<uint32_t>(coverage.FinestZoom);
  const double minLon = std::remainder(feature.MinLon, kDegPerTurn);
  const double maxLon = minLon + (feature.MaxLon - feature.MinLon);
  const auto column = [side](double lon) {
    return static_cast<uint64_t>(std::floor((lon + kDegPerHalfTurn) / kDegPerTurn * side));
  };
  const uint64_t first = column(minLon);
  const uint64_t width = std::min(column(maxLon) - first + 1u, static_cast<uint64_t>(side));
  const uint32_t lowY = BoundsRow(feature.MaxLat, coverage.FinestZoom);
  const uint32_t highY = BoundsRow(feature.MinLat, coverage.FinestZoom);
  const uint64_t height = static_cast<uint64_t>(highY) - lowY + 1u;
  if (width * height > coverage.MaximumTiles) { return BudgetExceeded(); }
  for (uint32_t y = lowY; y <= highY; ++y) {
    for (uint64_t col = 0; col < width; ++col) {
      if (!plan.Add({.Zoom = coverage.FinestZoom,
                     .X = static_cast<uint32_t>((first + col) % side),
                     .Y = y})) {
        return BudgetExceeded();
      }
    }
  }
  return {};
}

[[nodiscard]] PlanResult
AddSources(TilePlan &plan, std::span<const Data::TileId> sources, TerrainSourceCoverage coverage) {
  for (const auto tile : sources) {
    if (!ValidTile(tile) || tile.Zoom > coverage.FinestZoom) {
      return std::unexpected("terrain source tile is outside the source grid");
    }
    if (!plan.Neighbours(tile)) { return BudgetExceeded(); }
    if (coverage.GroundZoom >= 0 && coverage.GroundZoom < tile.Zoom) {
      const auto drop = static_cast<uint32_t>(tile.Zoom - coverage.GroundZoom);
      if (!plan.Neighbours(
              {.Zoom = coverage.GroundZoom, .X = tile.X >> drop, .Y = tile.Y >> drop})) {
        return BudgetExceeded();
      }
    }
  }
  return {};
}

[[nodiscard]] PlanResult AddVectors(TilePlan &plan,
                                    const ::outshine::Generators::Osm::OsmField &vectors,
                                    TerrainSourceCoverage coverage) {
  for (const auto &tile : vectors.Tiles()) {
    Data::TileId source{
        .Zoom = tile.Z, .X = static_cast<uint32_t>(tile.X), .Y = static_cast<uint32_t>(tile.Y)};
    if (!ValidTile(source)) { return std::unexpected("vector tile is outside its source grid"); }
    const int zoom = std::min(source.Zoom, coverage.FinestZoom);
    const auto drop = static_cast<uint32_t>(source.Zoom - zoom);
    source = {.Zoom = zoom, .X = source.X >> drop, .Y = source.Y >> drop};
    if (!plan.Neighbours(source)) { return BudgetExceeded(); }
  }
  if (!coverage.BuildingFootprints) { return {}; }
  const int buildings = vectors.Layer(::outshine::Generators::Osm::OsmLayer::Buildings);
  for (const auto &feature : vectors.Features()) {
    if (feature.Type != kPolygonFeature || std::cmp_not_equal(feature.Layer, buildings)) {
      continue;
    }
    const auto added = AddBuilding(plan, feature, coverage);
    if (!added) { return added; }
  }
  return {};
}
}

std::expected<std::vector<Data::TileId>, std::string>
PlanTerrainSourceTiles(std::span<const Data::TileId> sources, TerrainSourceCoverage coverage) {
  if (coverage.FinestZoom < 0 || coverage.FinestZoom > Ground::HeightField::MaximumTileZoom ||
      coverage.GroundZoom < -1 || coverage.GroundZoom > coverage.FinestZoom ||
      coverage.MaximumTiles == 0 || coverage.MaximumTiles > TerrainSourceCoverage::MaximumFields) {
    return std::unexpected("terrain source coverage has invalid zooms or tile budget");
  }
  TilePlan plan(coverage.MaximumTiles);
  const auto sourced = AddSources(plan, sources, coverage);
  if (!sourced) { return std::unexpected(sourced.error()); }
  if (coverage.Vectors != nullptr) {
    const auto vectors = AddVectors(plan, *coverage.Vectors, coverage);
    if (!vectors) { return std::unexpected(vectors.error()); }
  }
  for (const auto tile : coverage.AdditionalTiles) {
    if (!ValidTile(tile) || tile.Zoom != coverage.FinestZoom) {
      return std::unexpected("additional terrain tile is outside the candidate source grid");
    }
    if (!plan.Add(tile)) { return std::unexpected(kBudgetRefusal); }
  }
  return std::move(plan).Take();
}
}
