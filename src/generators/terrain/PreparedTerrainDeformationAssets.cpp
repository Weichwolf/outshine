#include "PreparedTerrainAssets.h"
#include "TerrainDeformationParts.h"
#include "Geodesy.h"
#include "TileGeodesy.h"
#include <algorithm>
#include <cmath>
#include <mutex>
#include <iterator>
#include <numeric>
#include <numbers>
#include <utility>

#include <cstddef>
#include <cstdint>
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

std::expected<std::optional<PreparedTerrainDeformation>, std::string>
ReadParts(const AssetCache &cache,
          const std::string &key,
          const TerrainDeformationParts &parts,
          size_t heapBytesMost,
          size_t &readBytes) {
  if (parts.HeapBytes > heapBytesMost) {
    return std::unexpected("terrain deformation region exceeds its residency budget");
  }
  PreparedTerrainDeformation product;
  product.Effects = parts.Effects;
  product.Pages.reserve(
      std::accumulate(parts.PageCounts.begin(), parts.PageCounts.end(), size_t{0}));
  size_t remaining = parts.HeapBytes;
  for (size_t index = 0; index < parts.PageCounts.size(); ++index) {
    const std::string partKey = TerrainDeformationPartKey(key, index);
    auto loaded = cache.Load(partKey, kTerrainDeformationBytesMost);
    if (!loaded) { return std::unexpected("terrain deformation part read failed"); }
    if (!*loaded) { return std::optional<PreparedTerrainDeformation>{}; }
    auto part = DecodeTerrainDeformation(partKey, (**loaded).Bytes());
    if (!part || part->Pages.size() != parts.PageCounts[index]) {
      return std::optional<PreparedTerrainDeformation>{};
    }
    const size_t held = TerrainDeformationHeap(part->Pages);
    if (held > remaining) { return std::optional<PreparedTerrainDeformation>{}; }
    remaining -= held;
    readBytes += (**loaded).Bytes().size();
    product.Pages.insert(product.Pages.end(),
                         std::make_move_iterator(part->Pages.begin()),
                         std::make_move_iterator(part->Pages.end()));
  }
  if (remaining != 0) { return std::optional<PreparedTerrainDeformation>{}; }
  return std::optional<PreparedTerrainDeformation>{std::move(product)};
}
}

std::expected<std::optional<PreparedTerrainDeformation>, std::string>
PreparedTerrainAssets::LoadDeformation(const std::string &key, size_t heapBytesMost) {
  const std::scoped_lock lock(Lock_);
  auto loaded = Cache_->Load(key, kTerrainDeformationBytesMost);
  if (!loaded) { return std::unexpected("terrain deformation asset read failed"); }
  if (*loaded) {
    size_t readBytes = (**loaded).Bytes().size();
    auto product = DecodeTerrainDeformation(key, (**loaded).Bytes());
    if (product && TerrainDeformationHeap(product->Pages) > heapBytesMost) {
      return std::unexpected("terrain deformation region exceeds its residency budget");
    }
    if (!product) {
      const auto parts = DecodeTerrainDeformationParts(key, (**loaded).Bytes());
      if (parts) {
        auto joined = ReadParts(*Cache_, key, *parts, heapBytesMost, readBytes);
        if (!joined) { return std::unexpected(std::move(joined.error())); }
        product = std::move(*joined);
      }
    }
    if (product) {
      ++DeformationHits_;
      DeformationReadBytes_ += readBytes;
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
  const auto counts = TerrainDeformationPageCounts(product.Pages);
  if (!counts) { return std::unexpected("terrain deformation page exceeds its package budget"); }
  const std::scoped_lock lock(Lock_);
  const auto publish = [&](const std::string &identity,
                           std::span<const Sheet> pages,
                           std::span<const uint8_t> bytes,
                           bool part,
                           size_t level) {
    const AssetRecord record{.Key = identity,
                             .Kind = part ? "terrain-deformed-pages" : "terrain-deformed",
                             .Bounds = Bounds(pages),
                             .Package = {},
                             .ByteCount = bytes.size(),
                             .Level = static_cast<uint32_t>(level),
                             .Parent = part ? key : std::string{}};
    return Cache_->Publish(std::span(&record, 1), bytes);
  };
  if (counts->size() == 1) {
    auto bytes = EncodeTerrainDeformation(key, product);
    if (!bytes || !publish(key, product.Pages, *bytes, false, 0)) {
      return std::unexpected("terrain deformation asset publication failed");
    }
  } else {
    size_t first = 0;
    for (size_t index = 0; index < counts->size(); ++index) {
      const auto pages = std::span(product.Pages).subspan(first, (*counts)[index]);
      const std::string partKey = TerrainDeformationPartKey(key, index);
      auto bytes = EncodeTerrainDeformation(partKey, pages, {});
      if (!bytes || !publish(partKey, pages, *bytes, true, index)) {
        return std::unexpected("terrain deformation part publication failed");
      }
      first += (*counts)[index];
    }
    const TerrainDeformationParts parts{.HeapBytes = TerrainDeformationHeap(product.Pages),
                                        .Effects = product.Effects,
                                        .PageCounts = *counts};
    auto bytes = EncodeTerrainDeformationParts(key, parts);
    if (!bytes || !publish(key, product.Pages, *bytes, false, 0)) {
      return std::unexpected("terrain deformation region publication failed");
    }
  }
  ++DeformationWrites_;
  return {};
}
}
