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
#include <vector>

namespace outshine::Generators::Osm {
namespace {
constexpr uint64_t kPackageVersion = 0x35524e474fULL;

bool ShapeValues(BinaryValueWriter &out, const ::outshine::Ground::ShapedGround &shape) {
  return out.Text(shape.Kind) && out(shape.AmplitudeM,
                                     shape.WavelengthM,
                                     shape.Gradient,
                                     shape.BearingDeg,
                                     shape.FocusLatDeg,
                                     shape.FocusLonDeg,
                                     shape.Seed);
}

std::expected<PreparedGroundRegions::Loaded, std::string> Decode(std::span<const uint8_t> bytes) {
  BinaryValueReader in(bytes);
  uint64_t version = 0;
  uint64_t groundSize = 0;
  uint64_t waterSize = 0;
  if (!in(version, groundSize) || version != kPackageVersion || groundSize > in.In.Remaining()) {
    return std::unexpected("invalid native region package header");
  }
  const auto groundBytes = in.In.Take(static_cast<size_t>(groundSize));
  if (!groundBytes) { return std::unexpected("invalid native region ground range"); }
  auto region = DecodeGroundRegionAsset(*groundBytes, PreparedGroundRegions::PackageBytesMost);
  if (!region || !in(waterSize) || waterSize > in.In.Remaining()) {
    return std::unexpected("invalid native region products");
  }
  const auto waterBytes = in.In.Take(static_cast<size_t>(waterSize));
  if (!waterBytes) { return std::unexpected("invalid native region water range"); }
  auto water = WaterAsset::DecodeNative(*waterBytes);
  if (!water || in.In.Remaining() != 0) {
    return std::unexpected("invalid native region water inputs");
  }
  return PreparedGroundRegions::Loaded{.Region = std::move(*region),
                                       .Water = std::move(*water),
                                       .ReadBytes = bytes.size(),
                                       .Key = {}};
}

std::expected<std::vector<uint8_t>, std::string>
Encode(const GroundRegionAsset &region, const WaterAsset &water, size_t bytesMost) {
  auto ground = EncodeGroundRegionAsset(region, bytesMost);
  auto inputs = water.EncodeNative(bytesMost);
  if (!ground || !inputs) { return std::unexpected("native region products cannot be encoded"); }
  BinaryValueWriter out(bytesMost);
  if (!out(kPackageVersion, static_cast<uint64_t>(ground->size())) || !out.Out.Put(*ground) ||
      !out(static_cast<uint64_t>(inputs->size())) || !out.Out.Put(*inputs)) {
    return std::unexpected("native region package exceeds its budget");
  }
  return std::move(out.Out).TakeBytes();
}
}

PreparedGroundRegions::PreparedGroundRegions(std::unique_ptr<AssetCache> cache, std::string recipe)
    : Cache_(std::move(cache)), Recipe_(std::move(recipe)) {}

std::expected<std::shared_ptr<PreparedGroundRegions>, std::string> PreparedGroundRegions::Open(
    const std::string &directory, const Data::SourceSet &sources, std::string_view rules) {
  auto cache = AssetCache::Open((std::filesystem::path(directory) / "assets.sqlite").string());
  if (!cache) { return std::unexpected("could not open the prepared ground region cache"); }
  auto recipe = Generators::AssetSourceRecipe(
      "prepared-ground-region-38",
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
  if (!out.Text(Recipe_) || !ShapeValues(out, shape) ||
      !out(vectors.Zoom(), vectors.Schema(), static_cast<uint64_t>(vectors.Tiles().size())) ||
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
  return Read(key);
}

std::string
PreparedGroundRegions::RequestKey(int zoom,
                                  MvtSchema schema,
                                  const ::outshine::Ground::ShapedGround &shape,
                                  std::span<const uint8_t> parameters,
                                  std::span<const ::outshine::Ground::TileSpot> tiles) const {
  BinaryValueWriter out(size_t{4} * 1024u * 1024u);
  if (!out.Text("native-ground-region-demand-2") || !out.Text(Recipe_) ||
      !ShapeValues(out, shape) || !out(zoom, schema, static_cast<uint64_t>(tiles.size())) ||
      !std::ranges::all_of(tiles,
                           [&](const auto &tile) { return out(tile.Zoom, tile.X, tile.Y); }) ||
      !out.Array(parameters)) {
    return {};
  }
  return Sha256Hex(out.Out.Bytes().data(), out.Out.Bytes().size());
}

std::expected<std::string, std::string>
PreparedGroundRegions::PlanRequest(LongitudeLatitude focus,
                                   int zoom,
                                   MvtSchema schema,
                                   int ring,
                                   const ::outshine::Ground::ShapedGround &shape,
                                   std::span<const uint8_t> parameters) const {
  constexpr size_t tilesMost = 8192;
  const auto window = OsmField::SourceWindow(focus, zoom, ring, tilesMost);
  if (!window) { return std::unexpected(std::string(window.error())); }
  std::vector<::outshine::Ground::TileSpot> tiles;
  for (int64_t y = window->MinY; y <= window->MaxY; ++y) {
    for (int64_t x = window->MinX; x <= window->MaxX; ++x) {
      tiles.push_back({.Zoom = zoom, .X = static_cast<uint32_t>(x), .Y = static_cast<uint32_t>(y)});
    }
  }
  auto key = RequestKey(zoom, schema, shape, parameters, tiles);
  if (key.empty()) { return std::unexpected("ground request key exceeds its encoding limit"); }
  return key;
}

std::expected<std::optional<PreparedGroundRegions::Loaded>, std::string>
PreparedGroundRegions::LoadRequest(const std::string &requestKey) {
  const std::scoped_lock lock(Lock_);
  auto found = Cache_->FindRequest(requestKey);
  if (!found) { return std::unexpected("could not find the prepared ground request"); }
  if (!*found) { return std::optional<Loaded>{}; }
  return Read((**found).Key);
}

std::expected<void, std::string> PreparedGroundRegions::BindRequest(const std::string &key,
                                                                    const std::string &requestKey) {
  const std::scoped_lock lock(Lock_);
  auto bound = Cache_->BindRequest(key, requestKey);
  if (!bound || !*bound) { return std::unexpected("could not bind the prepared ground request"); }
  return {};
}

std::expected<std::optional<PreparedGroundRegions::Loaded>, std::string>
PreparedGroundRegions::Read(const std::string &key) {
  auto cached = Cache_->Load(key, PackageBytesMost);
  if (!cached) { return std::unexpected("could not read the prepared ground region"); }
  if (!*cached) { return std::optional<Loaded>{}; }
  auto region = Decode((**cached).Bytes());
  if ((**cached).Record().Kind == "ground-region" && region) {
    region->Key = key;
    return std::move(*region);
  }
  if (!Cache_->Remove(key)) {
    return std::unexpected("could not remove an invalid prepared ground region");
  }
  return std::optional<Loaded>{};
}

std::expected<PreparedGroundRegions::Loaded, std::string>
PreparedGroundRegions::Store(const std::string &key,
                             const Box &bounds,
                             const GroundRegionAsset &region,
                             const WaterAsset &water,
                             const std::string &requestKey) {
  const std::scoped_lock lock(Lock_);
  auto cached = ResolveAsset(
      *Cache_,
      key,
      PackageBytesMost,
      [&](size_t bytesMost) -> std::expected<GeneratedAssetPackage, std::string> {
        auto encoded = Encode(region, water, bytesMost);
        if (!encoded) { return std::unexpected("ground region is invalid or exceeds its budget"); }
        return GeneratedAssetPackage{
            .Records = {{.Key = key,
                         .Kind = "ground-region",
                         .Bounds = bounds,
                         .Package = {},
                         .ByteCount = encoded->size(),
                         .Parent = {},
                         .RequestKey =
                             requestKey.empty() ? std::nullopt : std::optional(requestKey)}},
            .Bytes = std::move(*encoded)};
      });
  if (!cached) { return std::unexpected(std::move(cached.error())); }
  auto decoded = Decode(cached->Bytes());
  if (!decoded) { return std::unexpected("published ground region failed native validation"); }
  if (!requestKey.empty()) {
    auto bound = Cache_->BindRequest(key, requestKey);
    if (!bound || !*bound) { return std::unexpected("could not bind the stored ground region"); }
  }
  decoded->Key = key;
  return std::move(*decoded);
}
}
