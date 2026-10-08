#include "PreparedBuildingAssets.h"
#include "PreparedStructureCodec.h"
#include "AssetSourceRecipe.h"
#include "ByteArchive.h"
#include "Sha256.h"
#include "Geodesy.h"
#include <math/Units.h>
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
namespace {}

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
  auto assets = std::shared_ptr<PreparedBuildingAssets>(new PreparedBuildingAssets(
      std::move(*cache), AssetSourceRecipe("prepared-building-base-3", sources, kinds)));
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
      !bytes.Number(streetDigest) || !bytes.Number(shape.Seed)) {
    return {};
  }
  for (const double value : {spanM,
                             shape.AmplitudeM,
                             shape.WavelengthM,
                             shape.Gradient,
                             shape.BearingDeg,
                             shape.FocusLatDeg,
                             shape.FocusLonDeg}) {
    if (!std::isfinite(value) || !bytes.Number(value == 0.0 ? 0.0 : value)) { return {}; }
  }
  if (!bytes.Put({reinterpret_cast<const uint8_t *>(shape.Kind.data()), shape.Kind.size()})) {
    return {};
  }
  if (!bytes.Number(inputDigest.size()) ||
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
  if (!loaded) { return std::unexpected(StructureBakeErrorKind::ArtifactFailure); }
  if (*loaded) {
    const auto bytes = (**loaded).Bytes();
    auto base = DecodePreparedStructureIndex(bytes, kResidentBytesMost);
    if (!base) {
      const auto original = DecodePreparedStructureTile(bytes, kResidentBytesMost);
      if (original) {
        const auto stored = StoreBase(key, *original);
        if (!stored) { return std::unexpected(stored.error()); }
        base = DecodePreparedStructureIndex(*stored, kResidentBytesMost);
        if (!base) { return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct); }
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
                                 const std::atomic_bool &stopping) {
  auto base = PrepareStructureTile(raw, heights, &stopping);
  if (!base) { return std::unexpected(base.error()); }
  if (stopping.load(std::memory_order_relaxed)) {
    return std::unexpected(StructureBakeErrorKind::Cancelled);
  }
  const auto stored = StoreBase(key, *base);
  if (!stored) { return std::unexpected(stored.error()); }
  return Load(key);
}

std::expected<std::vector<uint8_t>, StructureBakeError>
PreparedBuildingAssets::StoreBase(const std::string &key, const PreparedStructureTile &base) {
  const auto surfaces = StoreSurfaces(key, base);
  if (!surfaces) { return std::unexpected(surfaces.error()); }
  auto encoded = EncodePreparedStructureIndex(base);
  if (!encoded || encoded->size() > kPackageBytesMost) {
    return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct);
  }
  if (!DecodePreparedStructureIndex(*encoded, kResidentBytesMost)) {
    return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct);
  }
  const AssetRecord record{.Key = key,
                           .Kind = "buildings",
                           .Bounds = Bounds(base),
                           .Package = {},
                           .ByteCount = encoded->size(),
                           .Parent = {}};
  {
    const std::scoped_lock lock(Lock_);
    const auto stored = Cache_->Publish(std::span(&record, 1), *encoded);
    if (!stored) { return std::unexpected(StructureBakeErrorKind::ArtifactFailure); }
    ++Writes_;
  }
  const auto storedBasis = StoreBasis(key, Bounds(base), PreparedBuildingBasis::Of(base));
  if (!storedBasis) { return std::unexpected(storedBasis.error()); }
  return std::move(*encoded);
}

PreparedBuildingAssets::Counters PreparedBuildingAssets::Costs() const noexcept {
  return {.Hits = Hits_.load(std::memory_order_relaxed),
          .Misses = Misses_.load(std::memory_order_relaxed),
          .Writes = Writes_.load(std::memory_order_relaxed),
          .ReadBytes = ReadBytes_.load(std::memory_order_relaxed),
          .BasisHits = BasisHits_.load(std::memory_order_relaxed),
          .BasisMisses = BasisMisses_.load(std::memory_order_relaxed),
          .BasisWrites = BasisWrites_.load(std::memory_order_relaxed),
          .BasisReadBytes = BasisReadBytes_.load(std::memory_order_relaxed),
          .GeometryHits = GeometryHits_.load(std::memory_order_relaxed),
          .GeometryMisses = GeometryMisses_.load(std::memory_order_relaxed),
          .GeometryWrites = GeometryWrites_.load(std::memory_order_relaxed),
          .GeometryReadBytes = GeometryReadBytes_.load(std::memory_order_relaxed),
          .SurfaceHits = SurfaceCosts_.Hits.load(std::memory_order_relaxed),
          .SurfaceMisses = SurfaceCosts_.Misses.load(std::memory_order_relaxed),
          .SurfaceWrites = SurfaceCosts_.Writes.load(std::memory_order_relaxed),
          .SurfaceReadBytes = SurfaceCosts_.ReadBytes.load(std::memory_order_relaxed)};
}
}
