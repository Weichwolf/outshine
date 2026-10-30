#include "PreparedBuildingAssets.h"
#include "PreparedStructureCodec.h"
#include "AssetSourceRecipe.h"
#include "ByteArchive.h"
#include "Sha256.h"
#include "Geodesy.h"
#include <Units.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <cmath>
#include <filesystem>
#include <span>
#include <system_error>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
bool EncodeShape(ByteWriter &bytes, const ::outshine::Ground::ShapedGround &shape, double spanM) {
  if (!bytes.Number(shape.Seed)) { return false; }
  for (const double value : {spanM,
                             shape.AmplitudeM,
                             shape.WavelengthM,
                             shape.Gradient,
                             shape.BearingDeg,
                             shape.FocusLatDeg,
                             shape.FocusLonDeg}) {
    if (!std::isfinite(value) || !bytes.Number(value == 0.0 ? 0.0 : value)) { return false; }
  }
  return bytes.Put({reinterpret_cast<const uint8_t *>(shape.Kind.data()), shape.Kind.size()});
}
}

PreparedBuildingAssets::PreparedBuildingAssets(std::unique_ptr<AssetCache> cache,
                                               std::string recipe)
    : Cache_(std::move(cache)), Recipe_(std::move(recipe)) {}

std::expected<std::shared_ptr<PreparedBuildingAssets>, std::string>
PreparedBuildingAssets::Open(const std::string &directory, const Data::SourceSet &sources) {
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  if (error) { return std::unexpected("could not create the native building cache"); }
  auto cache = AssetCache::Open((std::filesystem::path(directory) / "assets.sqlite").string());
  if (!cache) { return std::unexpected("could not open the native building cache"); }
  constexpr std::array kinds{Data::DataKind::Elevation, Data::DataKind::VectorMap};
  auto recipe = AssetSourceRecipe("prepared-building-base-12", sources, kinds);
  if (recipe.empty()) { return std::unexpected("building source metadata cannot be encoded"); }
  auto assets = std::shared_ptr<PreparedBuildingAssets>(
      new PreparedBuildingAssets(std::move(*cache), std::move(recipe)));
  assets->Self_ = assets;
  return assets;
}

std::string PreparedBuildingAssets::Key(Data::TileId tile,
                                        uint64_t streetDigest,
                                        const ::outshine::Ground::ShapedGround &shape,
                                        double spanM,
                                        std::string_view inputDigest) const {
  ByteWriter bytes(4096);
  if (!bytes.Put({reinterpret_cast<const uint8_t *>(Recipe_.data()), Recipe_.size()}) ||
      !bytes.Number(tile.Zoom) || !bytes.Number(tile.X) || !bytes.Number(tile.Y) ||
      !bytes.Number(streetDigest) || !EncodeShape(bytes, shape, spanM)) {
    return {};
  }
  if (!bytes.Number(inputDigest.size()) ||
      !bytes.Put({reinterpret_cast<const uint8_t *>(inputDigest.data()), inputDigest.size()})) {
    return {};
  }
  return Sha256Hex(bytes.Bytes().data(), bytes.Bytes().size());
}

std::string PreparedBuildingAssets::RequestKey(Data::TileId tile,
                                               const ::outshine::Ground::ShapedGround &shape,
                                               double spanM,
                                               std::string_view inputDigest) const {
  if (tile.Zoom < 0 || tile.Zoom > Data::TileId::MaximumZoom ||
      tile.X >= (uint32_t{1} << static_cast<unsigned>(tile.Zoom)) ||
      tile.Y >= (uint32_t{1} << static_cast<unsigned>(tile.Zoom)) || spanM <= 0 ||
      inputDigest.empty()) {
    return {};
  }
  constexpr std::string_view family = "prepared-building-demand-2";
  ByteWriter bytes(4096);
  if (!bytes.Put({reinterpret_cast<const uint8_t *>(family.data()), family.size()}) ||
      !bytes.Put({reinterpret_cast<const uint8_t *>(Recipe_.data()), Recipe_.size()}) ||
      !bytes.Number(tile.Zoom) || !bytes.Number(tile.X) || !bytes.Number(tile.Y) ||
      !EncodeShape(bytes, shape, spanM) || !bytes.Number(inputDigest.size()) ||
      !bytes.Put({reinterpret_cast<const uint8_t *>(inputDigest.data()), inputDigest.size()})) {
    return {};
  }
  return Sha256Hex(bytes.Bytes().data(), bytes.Bytes().size());
}

std::expected<PreparedBuildingAssets::Base, StructureBakeError>
PreparedBuildingAssets::Load(const std::string &key) {
  auto loaded = [&] {
    const std::scoped_lock lock(Lock_);
    return Cache_->Load(key, kPackageBytesMost);
  }();
  if (!loaded) { return std::unexpected(CacheFailure(loaded.error())); }
  if (*loaded) {
    const auto bytes = (**loaded).Bytes();
    auto base = DecodePreparedStructureIndex(bytes, kResidentBytesMost);
    if (!base) {
      auto original = DecodePreparedStructureTile(bytes, kResidentBytesMost);
      if (original) {
        auto prepared = PrepareBaseWrite(key, std::move(*original), {});
        const std::atomic_bool stopping{false};
        return CompleteBase(prepared, stopping);
      }
    }
    if (base) {
      ++Hits_;
      ReadBytes_ += bytes.size();
      auto shared = std::make_shared<PreparedStructureTile>(std::move(*base));
      BindSurfaces(key, shared);
      return shared;
    }
  }
  ++Misses_;
  return Base{};
}

std::expected<PreparedBuildingAssets::Base, StructureBakeError>
PreparedBuildingAssets::Generate(const std::string &key,
                                 const RawTile &raw,
                                 const ::outshine::Ground::HeightField &heights,
                                 const std::atomic_bool &stopping,
                                 const std::string &requestKey) {
  auto prepared = PrepareBase(key, raw, heights, stopping, requestKey);
  if (!prepared) { return std::unexpected(prepared.error()); }
  return CompleteBase(*prepared, stopping);
}

PreparedBuildingAssets::Counters PreparedBuildingAssets::Costs() const noexcept {
  return {
      .Hits = Hits_.load(std::memory_order_relaxed),
      .Misses = Misses_.load(std::memory_order_relaxed),
      .Writes = Writes_.load(std::memory_order_relaxed),
      .ReadBytes = ReadBytes_.load(std::memory_order_relaxed),
      .BasisHits = BasisHits_.load(std::memory_order_relaxed),
      .BasisMisses = BasisMisses_.load(std::memory_order_relaxed),
      .BasisWrites = BasisWrites_.load(std::memory_order_relaxed),
      .BasisReadBytes = BasisReadBytes_.load(std::memory_order_relaxed),
      .BasisResidentHits = BasisResidentHits_.load(std::memory_order_relaxed),
      .GeometryHits = GeometryHits_.load(std::memory_order_relaxed),
      .GeometryMisses = GeometryMisses_.load(std::memory_order_relaxed),
      .GeometryWrites = GeometryWrites_.load(std::memory_order_relaxed),
      .GeometryReadBytes = GeometryReadBytes_.load(std::memory_order_relaxed),
      .SurfaceHits = SurfaceCosts_.Hits.load(std::memory_order_relaxed),
      .SurfaceMisses = SurfaceCosts_.Misses.load(std::memory_order_relaxed),
      .SurfaceWrites = SurfaceCosts_.Writes.load(std::memory_order_relaxed),
      .SurfaceReadBytes = SurfaceCosts_.ReadBytes.load(std::memory_order_relaxed),
      .GeometryPreparations = GeometryPreparations_.load(std::memory_order_relaxed),
      .GeometryPublicationAttempts = GeometryPublicationAttempts_.load(std::memory_order_relaxed),
      .GeometryPublicationDeferred = GeometryPublicationDeferred_.load(std::memory_order_relaxed),
      .GeometryPublicationPeakBytes = GeometryPublicationPeakBytes_.load(std::memory_order_relaxed),
      .BasePreparations = BasePreparations_.load(std::memory_order_relaxed),
      .BasePackagePreparations = BasePackagePreparations_.load(std::memory_order_relaxed)};
}
}
