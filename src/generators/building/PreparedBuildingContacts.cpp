#include "PreparedBuildingAssets.h"
#include "StructureArtifact.h"
#include "GeographicRing.h"
#include "Sha256.h"
#include <algorithm>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace outshine::Generators {
namespace {
std::string ContactKey(const std::string &base) {
  return Sha256Hex("building-terrain-contacts-1/" + base);
}

bool ValidContacts(const BakedTile &tile,
                   std::span<const double> coordinates,
                   std::span<const GeographicRing> rings,
                   size_t sources) {
  const size_t points = coordinates.size() / 2u;
  const size_t holes = rings.size();
  if (tile.Prints.size() != sources || tile.Built.UsedBytes() != 0 || !tile.Walls.Index.empty() ||
      !tile.Walls.Clusters.empty() || !tile.Roofs.Index.empty() || !tile.Roofs.Clusters.empty() ||
      tile.RequestedCell || tile.RequestedDetail || tile.SelectedView || tile.SurfaceError ||
      tile.SurfaceFailure || !std::ranges::all_of(tile.FootprintDetails, [](LevelOfDetail detail) {
        return detail == LevelOfDetail::Fine;
      })) {
    return false;
  }
  return std::ranges::all_of(tile.Prints, [&](const auto &print) {
    return print.FirstPoint <= points && print.PointCount <= points - print.FirstPoint &&
           print.FirstHole <= holes && print.HoleCount <= holes - print.FirstHole &&
           (!print.TerrainCeiling || print.TerrainCeiling->OriginPoint < points);
  });
}
}

std::expected<std::optional<BakedTile>, StructureBakeError>
PreparedBuildingAssets::LoadContactArtifact(const std::string &baseKey,
                                            std::span<const double> coordinates,
                                            std::span<const GeographicRing> rings,
                                            size_t sources) {
  const auto key = ContactKey(baseKey);
  const auto loaded = Cache_->Load(key, kStructureArtifactBytesMost);
  if (!loaded) {
    if (loaded.error() == AssetCacheError::CapacityExceeded) {
      ++ContactCosts_.Misses;
      return std::optional<BakedTile>{};
    }
    return std::unexpected(CacheFailure(loaded.error()));
  }
  if (*loaded && (**loaded).Record().Parent == baseKey &&
      (**loaded).Record().Kind == "building-terrain-contacts" && (**loaded).Record().Level == 0) {
    auto tile = DecodeStructureArtifact((**loaded).Bytes(), key, 0);
    if (tile && ValidContacts(*tile, coordinates, rings, sources)) {
      ++ContactCosts_.Hits;
      ContactCosts_.ReadBytes += (**loaded).Bytes().size();
      return std::move(tile);
    }
  }
  ++ContactCosts_.Misses;
  return std::optional<BakedTile>{};
}

std::expected<std::optional<BakedTile>, StructureBakeError>
PreparedBuildingAssets::LoadTerrainContacts(const std::string &baseKey,
                                            const PreparedStructureTile &base) {
  auto loaded = LoadContactArtifact(baseKey, base.PointsLatLon, base.Holes, base.Structures.size());
  if (loaded && *loaded) { RestoreCoordinates(**loaded, base); }
  return loaded;
}

std::expected<std::optional<BakedTile>, StructureBakeError>
PreparedBuildingAssets::LoadTerrainContacts(const std::string &baseKey, Basis basis) {
  if (!basis) { return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct); }
  auto loaded =
      LoadContactArtifact(baseKey, basis->PointsLatLon, basis->Holes, basis->Sources.size());
  if (loaded && *loaded) { RestoreCoordinates(**loaded, std::move(basis)); }
  return loaded;
}

std::expected<PreparedBuildingAssets::ProductWrite, StructureBakeError>
PreparedBuildingAssets::PrepareTerrainContacts(const std::string &baseKey,
                                               const PreparedStructureTile &base,
                                               const BakedTile &tile) {
  if (!ValidContacts(tile, base.PointsLatLon, base.Holes, base.Structures.size())) {
    return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct);
  }
  ++ContactCosts_.Preparations;
  return PrepareProduct({.Key = ContactKey(baseKey),
                         .Kind = "building-terrain-contacts",
                         .Bounds = Bounds(base),
                         .Package = {},
                         .Parent = baseKey},
                        tile,
                        ProductKind::TerrainContacts);
}
}
