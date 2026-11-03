#include "ShapedGround.h"
#include "PreparedGroundRegions.h"
#include "AssetSourceRecipe.h"
#include "BinaryValueArchive.h"
#include "Sha256.h"
#include "SourceSet.h"
#include "OsmField.h"
#include <algorithm>
#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <stop_token>
#include <variant>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
namespace {
constexpr size_t kRequestBytesMost = size_t{4} * 1024u * 1024u;
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

std::expected<PreparedGroundRegions::Loaded, std::string>
DecodePackage(std::span<const uint8_t> bytes) {
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
  if (!water || !water->SurfacesReady() || in.In.Remaining() != 0) {
    return std::unexpected("invalid native region water inputs");
  }
  return PreparedGroundRegions::Loaded{.Region = std::move(*region),
                                       .Water = std::move(*water),
                                       .ReadBytes = bytes.size(),
                                       .Key = {}};
}

std::expected<std::vector<uint8_t>, std::string>
Encode(const GroundRegionAsset &region, const WaterAsset &water, size_t bytesMost) {
  if (!water.SurfacesReady()) {
    return std::unexpected("native region water surfaces are not prepared");
  }
  BinaryValueWriter out(bytesMost);
  if (!out(kPackageVersion, uint64_t{0}) || !EncodeGroundRegionAsset(region, out)) {
    return std::unexpected("native region products cannot be encoded");
  }
  const size_t waterLengthAt = out.Out.Bytes().size();
  if (!out.Out.NumberAt(sizeof(uint64_t),
                        static_cast<uint64_t>(waterLengthAt - 2 * sizeof(uint64_t))) ||
      !out(uint64_t{0}) || !water.EncodeNative(out) ||
      !out.Out.NumberAt(
          waterLengthAt,
          static_cast<uint64_t>(out.Out.Bytes().size() - waterLengthAt - sizeof(uint64_t)))) {
    return std::unexpected("native region package exceeds its budget");
  }
  return std::move(out.Out).TakeBytes();
}
}

PreparedGroundRegions::PreparedGroundRegions(std::shared_ptr<AssetCache> cache, std::string recipe)
    : Cache_(std::move(cache)), Recipe_(std::move(recipe)) {
  BinaryValueWriter prefix(kRequestBytesMost);
  if (!prefix.Text("native-ground-region-demand-2") || !prefix.Text(Recipe_)) { return; }
  const auto bytes = prefix.Out.Bytes();
  RequestPrefix_.emplace(bytes.data(), bytes.size());
  RequestPrefixBytes_ = bytes.size();
}

std::expected<std::shared_ptr<PreparedGroundRegions>, std::string> PreparedGroundRegions::Open(
    std::shared_ptr<AssetCache> cache, const Data::SourceSet &sources, std::string_view rules) {
  if (!cache) { return std::unexpected("ground regions require an asset store"); }
  auto recipe = Generators::AssetSourceRecipe(
      "prepared-ground-region-100",
      sources,
      std::array{Data::DataKind::Elevation, Data::DataKind::VectorMap});
  if (recipe.empty()) { return std::unexpected("ground source metadata cannot be encoded"); }
  recipe.append(rules);
  return std::shared_ptr<PreparedGroundRegions>(
      new PreparedGroundRegions(std::move(cache), std::move(recipe)));
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

PreparedGroundRegions::ReadResult PreparedGroundRegions::LoadProduct(const std::string &key) {
  auto product = Cache_->Load(key, PackageBytesMost);
  if (!product) { return std::unexpected(product.error()); }
  if (*product) {
    ReadBytes_ += (**product).Bytes().size();
    if ((**product).Record().Kind != "ground-region") { product->reset(); }
  }
  return product;
}

std::optional<PreparedGroundRegions::Loaded> PreparedGroundRegions::Decode(CachedAsset product) {
  if (product.Record().Kind != "ground-region") { return std::nullopt; }
  ++Decodes_;
  auto decoded = DecodePackage(product.Bytes());
  if (!decoded) { return std::nullopt; }
  decoded->Key = product.Record().Key;
  return std::move(*decoded);
}

std::string
PreparedGroundRegions::RequestKey(int zoom,
                                  MvtSchema schema,
                                  const ::outshine::Ground::ShapedGround &shape,
                                  std::span<const uint8_t> parameters,
                                  std::span<const ::outshine::Ground::TileSpot> tiles) const {
  if (!RequestPrefix_) { return {}; }
  BinaryValueWriter out(kRequestBytesMost - RequestPrefixBytes_);
  if (!ShapeValues(out, shape) || !out(zoom, schema, static_cast<uint64_t>(tiles.size())) ||
      !std::ranges::all_of(tiles,
                           [&](const auto &tile) { return out(tile.Zoom, tile.X, tile.Y); }) ||
      !out.Array(parameters)) {
    return {};
  }
  return RequestPrefix_->Hex(out.Out.Bytes().data(), out.Out.Bytes().size());
}

std::string PreparedGroundRegions::RequestKey(const OsmField &vectors,
                                              const ::outshine::Ground::ShapedGround &shape,
                                              std::span<const uint8_t> parameters) const {
  std::vector<::outshine::Ground::TileSpot> inputs;
  inputs.reserve(vectors.Tiles().size());
  for (const auto &tile : vectors.Tiles()) {
    inputs.push_back(
        {.Zoom = tile.Z, .X = static_cast<uint32_t>(tile.X), .Y = static_cast<uint32_t>(tile.Y)});
  }
  return RequestKey(vectors.Zoom(), vectors.Schema(), shape, parameters, inputs);
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

PreparedGroundRegions::ReadResult
PreparedGroundRegions::LoadRequestProduct(const std::string &requestKey) {
  constexpr unsigned kAttemptsMost = 2;
  for (unsigned attempt = 0; attempt < kAttemptsMost; ++attempt) {
    auto found = Cache_->FindRequest(requestKey);
    if (!found) { return std::unexpected(found.error()); }
    if (!*found) { return std::optional<CachedAsset>{}; }
    auto loaded = LoadProduct((**found).Key);
    const auto current = Cache_->FindRequest(requestKey);
    if (!current) { return std::unexpected(current.error()); }
    if (!*current) { return std::optional<CachedAsset>{}; }
    if (**current != **found) { continue; }
    return loaded;
  }
  return std::unexpected(AssetCacheError::Busy);
}

std::expected<void, PreparedGroundRegions::Failure>
PreparedGroundRegions::BindRequest(const std::string &key, const std::string &requestKey) {
  auto bound = Cache_->BindRequest(key, requestKey);
  if (!bound) { return std::unexpected(bound.error()); }
  if (!*bound) {
    return std::unexpected(std::string("could not bind the prepared ground request"));
  }
  return {};
}

std::expected<std::optional<PreparedGroundRegions::Loaded>, PreparedGroundRegions::Failure>
PreparedGroundRegions::LoadDecoded(const std::string &key) {
  auto product = LoadProduct(key);
  if (!product) { return std::unexpected(std::move(product.error())); }
  return *product ? Decode(std::move(**product)) : std::nullopt;
}

std::expected<PreparedGroundRegions::StoreWork, PreparedGroundRegions::Failure>
PreparedGroundRegions::PrepareStore(const std::string &key,
                                    const Box &bounds,
                                    const GroundRegionAsset &region,
                                    const WaterAsset &water,
                                    const std::string &requestKey) {
  auto loaded = LoadDecoded(key);
  if (!loaded) { return std::unexpected(std::move(loaded.error())); }
  StoreWork work(key, requestKey);
  if (*loaded) {
    work.Loaded_ = std::move(*loaded);
    work.Current_ = StoreWork::Phase::Bind;
    return work;
  }
  auto encoded = Encode(region, water, PackageBytesMost);
  if (!encoded) { return std::unexpected(std::move(encoded.error())); }
  std::vector<AssetRecord> records{
      {.Key = key,
       .Kind = "ground-region",
       .Bounds = bounds,
       .Package = {},
       .ByteCount = encoded->size(),
       .Parent = {},
       .RequestKey = requestKey.empty() ? std::nullopt : std::optional(requestKey)}};
  auto prepared = Cache_->PreparePublication(std::move(records), std::move(*encoded));
  if (!prepared) { return std::unexpected(prepared.error()); }
  work.Package_ = std::move(*prepared);
  ++Preparations_;
  return work;
}

std::expected<PreparedGroundRegions::Loaded, PreparedGroundRegions::Failure>
PreparedGroundRegions::ResumeStore(StoreWork &work, const std::stop_token &stop) {
  using Phase = StoreWork::Phase;

  if (stop.stop_requested()) {
    return std::unexpected(std::string("ground region publication canceled"));
  }
  if (work.Current_ == Phase::Publish) {
    if (!work.Package_) {
      return std::unexpected(std::string("prepared ground publication is missing"));
    }
    ++work.Attempts_;
    const auto committed = Cache_->Publish(*work.Package_);
    if (!committed) { return std::unexpected(committed.error()); }
    work.Current_ = Phase::Restore;
  }
  if (work.Current_ == Phase::Restore) {
    auto loaded = LoadDecoded(work.Key_);
    if (!loaded) { return std::unexpected(std::move(loaded.error())); }
    if (!*loaded) {
      return std::unexpected(std::string("published ground region failed native validation"));
    }
    work.Loaded_ = std::move(*loaded);
  } else if (work.Current_ == Phase::Bind && !work.Request_.empty()) {
    const auto bound = BindRequest(work.Key_, work.Request_);
    if (!bound) { return std::unexpected(bound.error()); }
  }
  if (!work.Loaded_ || work.Current_ == Phase::Complete) {
    return std::unexpected(std::string("prepared ground region is unavailable"));
  }
  return std::move(*work.Loaded_);
}

std::expected<PreparedGroundRegions::Loaded, PreparedGroundRegions::Failure>
PreparedGroundRegions::Store(StoreWork &work, const std::stop_token &stop) {
  auto result = ResumeStore(work, stop);
  if (stop.stop_requested()) {
    result = std::unexpected(std::string("ground region publication canceled"));
  }
  const auto *error = result ? nullptr : std::get_if<AssetCacheError>(&result.error());
  if (error == nullptr || *error != AssetCacheError::Busy) {
    work.Package_.reset();
    work.Loaded_.reset();
    work.Current_ = StoreWork::Phase::Complete;
  }
  return result;
}

PreparedGroundRegions::Counters PreparedGroundRegions::Costs() const noexcept {
  return {.ReadBytes = ReadBytes_.load(std::memory_order_relaxed),
          .Decodes = Decodes_.load(std::memory_order_relaxed),
          .Preparations = Preparations_.load(std::memory_order_relaxed)};
}

}
