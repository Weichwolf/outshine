#include "SurfaceTileAsset.h"
#include "AssetCache.h"
#include "AssetPublication.h"
#include "Sha256.h"
#include "TangentFrame.h"
#include "TileGeodesy.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace outshine {
namespace {

constexpr std::string_view kKind = "render-surface-tile-v1";

Data::TileId Parent(Data::TileId tile) {
  return {.Zoom = tile.Zoom - 1, .X = tile.X >> 1, .Y = tile.Y >> 1};
}

std::string Address(Data::TileId tile) {
  return std::format("{}/{}/{}", tile.Zoom, tile.X, tile.Y);
}

std::string Request(std::string_view revision, Data::TileId tile) {
  return Sha256Hex(std::format("{}/{}/{}", kKind, revision, Address(tile)));
}

Box EarthBounds(const SurfaceTile &tile) {
  const auto frame = TangentFrame::At(tile.Origin);
  const Box local{.Min = tile.MinimumM, .Max = tile.MaximumM};
  Box earth;
  for (unsigned corner = 0; corner < 8; ++corner) {
    const auto p = local.Corner(corner);
    earth.Cover(frame.ToEcefPosition({.EastM = p[0], .NorthM = -p[2], .UpM = p[1]}));
  }
  return earth;
}

std::expected<std::optional<SurfaceTile>, std::string> ReadRecord(AssetCache &cache,
                                                                  std::string_view revision,
                                                                  Data::TileId address,
                                                                  size_t &requestedBytes) {
  const auto record = cache.FindRequest(Request(revision, address));
  if (!record) { return std::unexpected("surface tile index lookup failed"); }
  if (!*record || (**record).Kind != kKind ||
      (**record).ByteCount > SurfaceTile::MaximumPackedBytes) {
    return std::nullopt;
  }
  requestedBytes += static_cast<size_t>((**record).ByteCount);
  std::optional<SurfaceTile> tile;
  const auto loaded =
      cache.LoadValidated((**record).Key,
                          kKind,
                          SurfaceTile::MaximumPackedBytes,
                          [&](const CachedAsset &asset) -> std::expected<bool, AssetCacheError> {
                            auto decoded = DecodePackedSurfaceTile(asset.Bytes());
                            if (!decoded || decoded->Coverage != address) { return false; }
                            tile = std::move(*decoded);
                            return true;
                          });
  if (!loaded) {
    if (loaded.error() == AssetCacheError::CapacityExceeded) { return std::nullopt; }
    return std::unexpected("surface tile package lookup failed");
  }
  return tile;
}

}

bool IsSurfaceTileSetRevision(std::string_view revision) {
  return AssetProductKey::Parse(revision).has_value();
}

std::expected<void, std::string>
PublishSurfaceTile(AssetCache &cache, std::string_view revision, std::span<const uint8_t> bytes) {
  if (!IsSurfaceTileSetRevision(revision)) {
    return std::unexpected("surface tile set needs a SHA-256 revision");
  }
  const auto tile = DecodePackedSurfaceTile(bytes);
  if (!tile) { return std::unexpected(tile.error()); }
  const auto request = Request(revision, tile->Coverage);
  const auto parent = tile->Coverage.Zoom > 0 ? Request(revision, Parent(tile->Coverage)) : "";
  const auto package = Sha256Hex(bytes.data(), bytes.size());
  const AssetRecord record{.Key = Sha256Hex(request + package),
                           .Kind = std::string(kKind),
                           .Bounds = EarthBounds(*tile),
                           .Package = package,
                           .ByteCount = bytes.size(),
                           .Level = static_cast<uint32_t>(tile->Coverage.Zoom),
                           .Parent = parent,
                           .RequestKey = request,
                           .Tile = AssetTile{.Revision = std::string(revision),
                                             .Coverage = Sha256Hex(Address(tile->Coverage)),
                                             .Coarser = {},
                                             .ErrorM = tile->GeometricErrorM}};
  if (!cache.Publish(std::span(&record, 1), bytes)) {
    return std::unexpected("surface tile publication failed");
  }
  return {};
}

std::expected<SurfaceTileLookup, std::string>
LoadSurfaceTileAt(AssetCache &cache, std::string_view revision, LongitudeLatitude location) {
  if (!IsSurfaceTileSetRevision(revision)) {
    return std::unexpected("surface tile set needs a SHA-256 revision");
  }
  const auto leaf = Ground::TileIndex::Of({.LongitudeDeg = location.LongitudeDeg,
                                           .LatitudeDeg = location.LatitudeDeg},
                                          Data::TileId::MaximumZoom)
                        .Tile();
  SurfaceTileLookup result;
  if (!leaf) { return result; }
  for (auto address = *leaf; address.Zoom >= 0; address = Parent(address)) {
    auto tile = ReadRecord(cache, revision, address, result.RequestedBytes);
    if (!tile) { return std::unexpected(tile.error()); }
    if (*tile) {
      result.Product = std::move(*tile);
      break;
    }
  }
  return result;
}

}
