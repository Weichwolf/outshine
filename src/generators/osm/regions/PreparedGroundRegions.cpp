#include "PreparedGroundRegions.h"
#include "AssetSourceRecipe.h"
#include "BinaryValueArchive.h"
#include "Sha256.h"
#include "SourceSet.h"
#include "content/AssetGeneration.h"
#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
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
      "prepared-ground-region-3",
      sources,
      std::array{Data::DataKind::Elevation, Data::DataKind::VectorMap});
  recipe.append(rules);
  return std::shared_ptr<PreparedGroundRegions>(
      new PreparedGroundRegions(std::move(*cache), std::move(recipe)));
}

std::string PreparedGroundRegions::Key(std::string_view binding) const {
  BinaryValueWriter out(size_t{4} * 1024u * 1024u);
  if (!out.Text(Recipe_) || !out.Text(binding)) { return {}; }
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
