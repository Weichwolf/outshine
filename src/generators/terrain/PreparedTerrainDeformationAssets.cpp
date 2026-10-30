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
#include <stop_token>
#include <vector>

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

std::expected<std::optional<PreparedTerrainDeformation>, AssetCacheError>
ReadParts(const AssetCache &cache,
          const std::string &key,
          const TerrainDeformationParts &parts,
          size_t heapBytesMost,
          size_t &readBytes) {
  if (parts.HeapBytes > heapBytesMost) {
    return std::unexpected(AssetCacheError::CapacityExceeded);
  }
  PreparedTerrainDeformation product;
  product.Effects = parts.Effects;
  product.Pages.reserve(
      std::accumulate(parts.PageCounts.begin(), parts.PageCounts.end(), size_t{0}));
  size_t remaining = parts.HeapBytes;
  for (size_t index = 0; index < parts.PageCounts.size(); ++index) {
    const std::string partKey = TerrainDeformationPartKey(key, index);
    auto loaded = cache.Load(partKey, kTerrainDeformationBytesMost);
    if (!loaded) { return std::unexpected(loaded.error()); }
    if (!*loaded || (**loaded).Record().Kind != "terrain-deformed-pages") {
      return std::optional<PreparedTerrainDeformation>{};
    }
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

std::expected<std::optional<PreparedTerrainDeformation>, PreparedTerrainAssets::Failure>
PreparedTerrainAssets::LoadDeformation(const std::string &key, size_t heapBytesMost) {
  const auto request = MutexForRequest(key);
  const std::scoped_lock preparing(*request);
  std::optional<PreparedTerrainDeformation> product;
  size_t readBytes = 0;
  auto loaded = Cache_->LoadValidated(
      key,
      "terrain-deformed",
      kTerrainDeformationBytesMost,
      [&](const CachedAsset &asset) -> std::expected<bool, AssetCacheError> {
        readBytes = asset.Bytes().size();
        product = DecodeTerrainDeformation(key, asset.Bytes());
        if (product && TerrainDeformationHeap(product->Pages) > heapBytesMost) {
          return std::unexpected(AssetCacheError::CapacityExceeded);
        }
        if (!product) {
          const auto parts = DecodeTerrainDeformationParts(key, asset.Bytes());
          if (parts) {
            auto joined = ReadParts(*Cache_, key, *parts, heapBytesMost, readBytes);
            if (!joined) { return std::unexpected(joined.error()); }
            product = std::move(*joined);
          }
        }
        return product.has_value();
      });
  if (!loaded) { return std::unexpected(loaded.error()); }
  if (*loaded && product) {
    ++DeformationHits_;
    DeformationReadBytes_ += readBytes;
    return product;
  }
  ++DeformationMisses_;
  return std::optional<PreparedTerrainDeformation>{};
}

size_t PreparedTerrainAssets::DeformationWrite::HeapBytes() const noexcept {
  size_t bytes = Key_.capacity() + 1 + Product_.Pages.capacity() * sizeof(Sheet) +
                 Parts_.capacity() * sizeof(uint32_t);
  for (const auto &page : Product_.Pages) { bytes += page.Nodes.capacity() * sizeof(float); }
  return bytes + (Package_ ? Package_->CapacityBytes() - sizeof(PreparedAssetWrite) : 0);
}

std::expected<PreparedTerrainAssets::DeformationWrite, PreparedTerrainAssets::Failure>
PreparedTerrainAssets::PrepareDeformation(std::string key, PreparedTerrainDeformation product) {
  auto counts = TerrainDeformationPageCounts(product.Pages);
  if (!counts) {
    return std::unexpected(std::string("terrain deformation page exceeds its package budget"));
  }
  return DeformationWrite(std::move(key), std::move(product), std::move(*counts));
}

std::expected<PreparedAssetWrite, PreparedTerrainAssets::Failure>
PreparedTerrainAssets::PrepareDeformationPackage(const DeformationWrite &write) {
  const bool part = write.Parts_.size() > 1 && write.Next_ < write.Parts_.size();
  const auto pages =
      part ? std::span(write.Product_.Pages).subspan(write.First_, write.Parts_[write.Next_])
           : std::span(write.Product_.Pages);
  const std::string identity =
      part ? TerrainDeformationPartKey(write.Key_, write.Next_) : write.Key_;
  auto bytes = [&] {
    if (part) { return EncodeTerrainDeformation(identity, pages, {}); }
    if (write.Parts_.size() == 1) { return EncodeTerrainDeformation(identity, write.Product_); }
    return EncodeTerrainDeformationParts(identity,
                                         {.HeapBytes = TerrainDeformationHeap(pages),
                                          .Effects = write.Product_.Effects,
                                          .PageCounts = write.Parts_});
  }();
  if (!bytes) { return std::unexpected(std::string("terrain deformation encoding failed")); }
  AssetRecord record{.Key = identity,
                     .Kind = part ? "terrain-deformed-pages" : "terrain-deformed",
                     .Bounds = Bounds(pages),
                     .Package = {},
                     .ByteCount = bytes->size(),
                     .Level = static_cast<uint32_t>(write.Next_),
                     .Parent = part ? write.Key_ : std::string{}};
  if (!part) { record.Level = 0; }
  std::vector<AssetRecord> records;
  records.push_back(std::move(record));
  auto package = Cache_->PreparePublication(std::move(records), std::move(*bytes));
  if (!package) { return std::unexpected(package.error()); }
  ++DeformationPreparations_;
  return std::move(*package);
}

std::expected<bool, PreparedTerrainAssets::Failure>
PreparedTerrainAssets::StoreDeformation(DeformationWrite &write, const std::stop_token &stop) {
  if (stop.stop_requested()) {
    return std::unexpected(std::string("terrain deformation cancelled"));
  }
  if (write.Complete_) { return true; }
  if (!write.Package_) {
    auto prepared = PrepareDeformationPackage(write);
    if (!prepared) { return std::unexpected(std::move(prepared.error())); }
    write.Package_ = std::move(*prepared);
  }
  if (stop.stop_requested()) {
    return std::unexpected(std::string("terrain deformation cancelled"));
  }
  const auto request = MutexForRequest(write.Key_);
  const std::scoped_lock preparing(*request);
  ++DeformationPublicationAttempts_;
  const auto published = Cache_->Publish(*write.Package_);
  if (!published) { return std::unexpected(published.error()); }
  write.Package_.reset();
  if (write.Parts_.size() > 1 && write.Next_ < write.Parts_.size()) {
    write.First_ += write.Parts_[write.Next_++];
    return false;
  }
  write.Complete_ = true;
  ++DeformationWrites_;
  return true;
}
}
