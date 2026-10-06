#include "PreparedTerrainAssets.h"
#include "Geodesy.h"
#include "TileGeodesy.h"
#include <algorithm>
#include <cmath>
#include <mutex>
#include <numbers>
#include <utility>

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <span>

namespace outshine::Generators {
namespace {
Box Bounds(std::span<const Sheet> pages) {
  Box bounds;
  for (const Sheet &page : pages) {
    const double tileSpan = std::ldexp(1.0, page.Tile.Zoom);
    const double halo = page.Side > 1 ? 1.0 / (page.Side - 1.0) : 1.0;
    const auto low =
        Ground::TileFracToGeo({.X = page.Tile.X - halo, .Y = page.Tile.Y - halo}, page.Tile.Zoom);
    const auto high = Ground::TileFracToGeo(
        {.X = page.Tile.X + 1.0 + halo, .Y = page.Tile.Y + 1.0 + halo}, page.Tile.Zoom);
    double lower = 0;
    double upper = 0;
    if (!page.Nodes.empty()) {
      const auto range = std::ranges::minmax_element(page.Nodes);
      lower = *range.min;
      upper = *range.max;
    }
    Vec3 centre;
    GeoToEcef({.LongitudeDeg = (low.LongitudeDeg + high.LongitudeDeg) * 0.5,
               .LatitudeDeg = (low.LatitudeDeg + high.LatitudeDeg) * 0.5,
               .HeightM = (lower + upper) * 0.5},
              centre);
    const double radius =
        (6400000.0 + std::max(std::abs(lower), std::abs(upper))) *
            (2.0 * std::numbers::pi * (1.0 + 2.0 * halo) / tileSpan +
             std::abs(low.LatitudeDeg - high.LatitudeDeg) * std::numbers::pi / 180.0) *
            0.5 +
        (upper - lower) * 0.5;
    Box pageBounds;
    for (size_t axis = 0; axis < 3; ++axis) {
      pageBounds.Min[axis] = centre[axis] - radius;
      pageBounds.Max[axis] = centre[axis] + radius;
    }
    bounds.Cover(pageBounds);
  }
  return bounds;
}
}

std::expected<std::optional<PreparedTerrainDeformation>, std::string>
PreparedTerrainAssets::LoadDeformation(const std::string &key) {
  const std::scoped_lock lock(Lock_);
  auto loaded = Cache_->Load(key, kTerrainDeformationBytesMost);
  if (!loaded) { return std::unexpected("terrain deformation asset read failed"); }
  if (*loaded) {
    auto product = DecodeTerrainDeformation(key, (**loaded).Bytes());
    if (product) {
      ++DeformationHits_;
      DeformationReadBytes_ += (**loaded).Bytes().size();
      return product;
    }
    if (!Cache_->Remove(key)) {
      return std::unexpected("invalid terrain deformation asset removal failed");
    }
  }
  ++DeformationMisses_;
  return std::optional<PreparedTerrainDeformation>{};
}

std::expected<void, std::string>
PreparedTerrainAssets::StoreDeformation(const std::string &key,
                                        const PreparedTerrainDeformation &product) {
  auto bytes = EncodeTerrainDeformation(key, product);
  if (!bytes) {
    return std::unexpected("terrain deformation asset exceeds its format or byte budget");
  }
  const AssetRecord record{.Key = key,
                           .Kind = "terrain-deformed",
                           .Bounds = Bounds(product.Pages),
                           .Package = {},
                           .ByteCount = bytes->size(),
                           .Parent = {}};
  const std::scoped_lock lock(Lock_);
  if (!Cache_->Publish(std::span(&record, 1), *bytes)) {
    return std::unexpected("terrain deformation asset publication failed");
  }
  ++DeformationWrites_;
  return {};
}
}
