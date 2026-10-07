#include "PreparedGroundRegions.h"
#include "AssetSourceRecipe.h"
#include "BinaryValueArchive.h"
#include "Sha256.h"
#include "SourceSet.h"
#include "OsmField.h"
#include "TilePool.h"
#include "content/AssetGeneration.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Generators::Osm {
PreparedGroundRegions::PreparedGroundRegions(std::unique_ptr<AssetCache> cache, std::string recipe)
    : Cache_(std::move(cache)), Recipe_(std::move(recipe)) {}

std::expected<std::shared_ptr<PreparedGroundRegions>, std::string> PreparedGroundRegions::Open(
    const std::string &directory, const Data::SourceSet &sources, std::string_view rules) {
  auto cache = AssetCache::Open((std::filesystem::path(directory) / "assets.sqlite").string());
  if (!cache) { return std::unexpected("could not open the prepared ground region cache"); }
  auto recipe = Generators::AssetSourceRecipe(
      "prepared-ground-region-4",
      sources,
      std::array{Data::DataKind::Elevation, Data::DataKind::VectorMap});
  recipe.append(rules);
  return std::shared_ptr<PreparedGroundRegions>(
      new PreparedGroundRegions(std::move(*cache), std::move(recipe)));
}

std::string PreparedGroundRegions::Key(const OsmField &vectors,
                                       const ::outshine::Ground::ShapedGround &shape,
                                       std::span<const uint8_t> parameters) const {
  BinaryValueWriter out(size_t{4} * 1024u * 1024u);
  if (!out.Text(Recipe_) || !out.Text(shape.Kind) ||
      !out(shape.AmplitudeM,
           shape.WavelengthM,
           shape.Gradient,
           shape.BearingDeg,
           shape.FocusLatDeg,
           shape.FocusLonDeg,
           shape.Seed,
           vectors.Zoom(),
           vectors.Schema(),
           static_cast<uint64_t>(vectors.Tiles().size())) ||
      !std::ranges::all_of(
          vectors.Tiles(),
          [&](const auto &tile) {
            return !tile.InputDigest.empty() && out(tile.Z, tile.X, tile.Y) &&
                   out.Text(tile.InputDigest) &&
                   out(tile.Source.From,
                       tile.Source.Kind,
                       tile.Source.Tile.Zoom,
                       tile.Source.Tile.X,
                       tile.Source.Tile.Y,
                       tile.Source.NativeCell.has_value()) &&
                   (!tile.Source.NativeCell ||
                    out(tile.Source.NativeCell->SouthDeg, tile.Source.NativeCell->WestDeg)) &&
                   out.Text(tile.Source.SourceId) && out.Text(tile.Source.Revision);
          }) ||
      !out.Array(parameters)) {
    return {};
  }
  return Sha256Hex(out.Out.Bytes().data(), out.Out.Bytes().size());
}

std::expected<std::optional<PreparedGroundRegions::Loaded>, std::string>
PreparedGroundRegions::Load(const std::string &key) {
  const std::scoped_lock lock(Lock_);
  auto cached = Cache_->Load(key, PackageBytesMost);
  if (!cached) { return std::unexpected("could not read the prepared ground region"); }
  if (!*cached) { return std::optional<Loaded>{}; }
  auto region = DecodeGroundRegionAsset((**cached).Bytes(), PackageBytesMost);
  if ((**cached).Record().Kind == "ground-region" && region) {
    return Loaded{.Region = std::move(*region), .ReadBytes = (**cached).Bytes().size()};
  }
  if (!Cache_->Remove(key)) {
    return std::unexpected("could not remove an invalid prepared ground region");
  }
  return std::optional<Loaded>{};
}

std::expected<PreparedGroundRegions::Loaded, std::string> PreparedGroundRegions::Store(
    const std::string &key, const Box &bounds, const GroundRegionAsset &region) {
  const std::scoped_lock lock(Lock_);
  auto cached =
      ResolveAsset(*Cache_,
                   key,
                   PackageBytesMost,
                   [&](size_t bytesMost) -> std::expected<GeneratedAssetPackage, std::string> {
                     auto encoded = EncodeGroundRegionAsset(region, bytesMost);
                     if (!encoded) {
                       return std::unexpected("ground region is invalid or exceeds its budget");
                     }
                     return GeneratedAssetPackage{.Records = {{.Key = key,
                                                               .Kind = "ground-region",
                                                               .Bounds = bounds,
                                                               .Package = {},
                                                               .ByteCount = encoded->size(),
                                                               .Parent = {}}},
                                                  .Bytes = std::move(*encoded)};
                   });
  if (!cached) { return std::unexpected(std::move(cached.error())); }
  auto decoded = DecodeGroundRegionAsset(cached->Bytes(), PackageBytesMost);
  if (!decoded) { return std::unexpected("published ground region failed native validation"); }
  return Loaded{.Region = std::move(*decoded), .ReadBytes = cached->Bytes().size()};
}
}
