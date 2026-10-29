#include "PreparedTerrainAssets.h"
#include "PreparedTerrainCodec.h"
#include "PreparedGroundPatchCodec.h"
#include "AssetSourceRecipe.h"
#include <array>
#include "ByteArchive.h"
#include "SourceSet.h"
#include "Sha256.h"
#include "Geodesy.h"
#include "TileGeodesy.h"
#include <Units.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <string>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <numbers>
#include <optional>
#include <span>
#include <stop_token>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace outshine::Generators {
namespace {

std::string Key(const std::string &recipe,
                Data::TileId at,
                const ::outshine::Ground::TerrainTiles::Shaped &shape) {
  ByteWriter encoded(4096);
  if (!encoded.Put({reinterpret_cast<const uint8_t *>(recipe.data()), recipe.size()}) ||
      !encoded.Number(at.Zoom) || !encoded.Number(at.X) || !encoded.Number(at.Y) ||
      !encoded.Number(shape.Seed)) {
    return {};
  }
  for (const auto value : {shape.AmplitudeM,
                           shape.WavelengthM,
                           shape.Gradient,
                           shape.BearingDeg,
                           shape.FocusLatDeg,
                           shape.FocusLonDeg}) {
    if (!std::isfinite(value) || !encoded.Number(value == 0.0 ? 0.0 : value)) { return {}; }
  }
  if (!encoded.Put({reinterpret_cast<const uint8_t *>(shape.Kind.data()), shape.Kind.size()})) {
    return {};
  }
  return Sha256Hex(encoded.Bytes().data(), encoded.Bytes().size());
}

Box Bounds(Data::TileId at, double lowestM, double highestM) {
  const double span = std::ldexp(1.0, at.Zoom);
  const double west = kDegPerTurn * at.X / span - kDegPerHalfTurn;
  const double east = kDegPerTurn * (at.X + 1.0) / span - kDegPerHalfTurn;
  const auto latitude = [span](double row) {
    return std::atan(std::sinh(std::numbers::pi * (1.0 - 2.0 * row / span))) * kDegPerHalfTurn /
           std::numbers::pi;
  };
  const double north = latitude(at.Y);
  const double south = latitude(at.Y + 1.0);
  Vec3 centre;
  GeoToEcef({.LongitudeDeg = (west + east) * 0.5,
             .LatitudeDeg = (south + north) * 0.5,
             .HeightM = (lowestM + highestM) * 0.5},
            centre);
  constexpr double earthDerivativeBoundM = 6400000.0;
  const double radius = (earthDerivativeBoundM + std::max(std::abs(lowestM), std::abs(highestM))) *
                            (east - west + north - south) * std::numbers::pi / kDegPerTurn +
                        (highestM - lowestM) * 0.5;
  Box box;
  for (size_t axis = 0; axis < 3; ++axis) {
    box.Min[axis] = centre[axis] - radius;
    box.Max[axis] = centre[axis] + radius;
  }
  return box;
}

Box Bounds(Data::TileId at, const ::outshine::Ground::TerrainField &field) {
  const auto heights = std::span(field.Data(), static_cast<size_t>(field.Rows()) * field.Cols());
  const auto range = std::ranges::minmax_element(heights);
  return Bounds(at, *range.min, *range.max);
}

::outshine::Ground::TerrainGrid Refused(Data::TileId at, Data::SourceFailureReason reason) {
  return ::outshine::Ground::TerrainGrid::Refused(
      Data::SourceFailure{.Requested = Data::Address::At(at),
                          .Served = std::nullopt,
                          .SourceId = "prepared-terrain",
                          .SourceRevision = {},
                          .SourceKey = {},
                          .Reason = reason});
}

}

PreparedTerrainAssets::PreparedTerrainAssets(std::unique_ptr<AssetCache> cache, std::string recipe)
    : Cache_(std::move(cache)), Recipe_(std::move(recipe)) {}

std::expected<std::shared_ptr<PreparedTerrainAssets>, std::string>
PreparedTerrainAssets::Open(const std::string &directory, const Data::SourceSet &sources) {
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  if (error) {
    return std::unexpected("could not create the native asset cache: " + error.message());
  }
  auto cache = AssetCache::Open((std::filesystem::path(directory) / "assets.sqlite").string());
  if (!cache) { return std::unexpected("could not open the native asset cache"); }
  auto recipe =
      AssetSourceRecipe("prepared-terrain-field-1", sources, std::array{Data::DataKind::Elevation});
  if (recipe.empty()) { return std::unexpected("terrain source metadata cannot be encoded"); }
  return std::shared_ptr<PreparedTerrainAssets>(
      new PreparedTerrainAssets(std::move(*cache), std::move(recipe)));
}

std::shared_ptr<std::mutex> PreparedTerrainAssets::MutexForRequest(const std::string &key) {
  const std::scoped_lock lock(Lock_);
  std::erase_if(Requests_, [](const auto &entry) { return entry.second.expired(); });
  auto &pending = Requests_[key];
  auto request = pending.lock();
  if (!request) {
    request = std::make_shared<std::mutex>();
    pending = request;
  }
  return request;
}

::outshine::Ground::TerrainGrid
PreparedTerrainAssets::Remember(const std::string &key, ::outshine::Ground::TerrainField field) {
  auto shared = std::make_shared<const ::outshine::Ground::TerrainField>(std::move(field));
  const std::scoped_lock lock(Lock_);
  constexpr size_t maximum = 4096;
  if (Resident_.size() >= maximum) {
    std::erase_if(Resident_, [](const auto &entry) { return entry.second.expired(); });
  }
  if (Resident_.size() < maximum) { Resident_[key] = shared; }
  return ::outshine::Ground::TerrainGrid::Holding(std::move(shared));
}

::outshine::Ground::TerrainGrid
PreparedTerrainAssets::Resolve(Data::TileId at,
                               const ::outshine::Ground::TerrainTiles::Shaped &shape,
                               const ::outshine::Ground::TerrainTiles::FieldFactory &factory) {
  using namespace ::outshine::Ground;
  if (at.Zoom < 0 || at.Zoom > Data::TileId::MaximumZoom ||
      at.X >= (uint32_t{1} << static_cast<unsigned>(at.Zoom)) ||
      at.Y >= (uint32_t{1} << static_cast<unsigned>(at.Zoom))) {
    return Refused(at, Data::SourceFailureReason::InvalidRequest);
  }
  const auto key = Key(Recipe_, at, shape);
  if (key.empty()) { return Refused(at, Data::SourceFailureReason::InvalidRequest); }
  const auto request = MutexForRequest(key);
  const std::scoped_lock preparing(*request);
  {
    const std::scoped_lock lock(Lock_);
    const auto held = Resident_.find(key);
    if (held != Resident_.end()) {
      if (auto field = held->second.lock()) {
        ResidentHits_.fetch_add(1, std::memory_order_relaxed);
        return TerrainGrid::Holding(std::move(field));
      }
    }
  }
  auto loaded = Cache_->Load(key, kPreparedTerrainBytesMost);
  if (!loaded) { return Refused(at, Data::SourceFailureReason::ProviderRefused); }
  if (*loaded) {
    auto field = DecodePreparedTerrain(at, (**loaded).Bytes(), kPreparedTerrainBytesMost);
    if (field) {
      Hits_.fetch_add(1, std::memory_order_relaxed);
      ReadBytes_.fetch_add((**loaded).Bytes().size(), std::memory_order_relaxed);
      return Remember(key, std::move(*field));
    }
    if (!Cache_->Remove(key)) { return Refused(at, Data::SourceFailureReason::ProviderRefused); }
  }
  Misses_.fetch_add(1, std::memory_order_relaxed);
  if (!factory) { return Refused(at, Data::SourceFailureReason::ProviderRefused); }
  auto generated = factory();
  const auto *field = generated.TryField();
  if (field == nullptr) { return generated; }
  auto encoded = EncodePreparedTerrain(at, *field);
  if (!encoded) { return Refused(at, Data::SourceFailureReason::CorruptPayload); }
  const AssetRecord record{.Key = key,
                           .Kind = "terrain-field",
                           .Bounds = Bounds(at, *field),
                           .Package = {},
                           .ByteCount = encoded->size(),
                           .Level = static_cast<uint32_t>(at.Zoom),
                           .Parent = {}};
  if (!Cache_->Publish({&record, 1}, *encoded)) {
    return Refused(at, Data::SourceFailureReason::ProviderRefused);
  }
  Writes_.fetch_add(1, std::memory_order_relaxed);
  loaded = Cache_->Load(key, kPreparedTerrainBytesMost);
  if (!loaded || !*loaded) { return Refused(at, Data::SourceFailureReason::ProviderRefused); }
  auto ready = DecodePreparedTerrain(at, (**loaded).Bytes(), kPreparedTerrainBytesMost);
  return ready ? Remember(key, std::move(*ready))
               : Refused(at, Data::SourceFailureReason::CorruptPayload);
}

PreparedTerrainAssets::Counters PreparedTerrainAssets::Costs() const noexcept {
  return {.Hits = Hits_.load(std::memory_order_relaxed),
          .Misses = Misses_.load(std::memory_order_relaxed),
          .Resident = ResidentHits_.load(std::memory_order_relaxed),
          .Writes = Writes_.load(std::memory_order_relaxed),
          .ReadBytes = ReadBytes_.load(std::memory_order_relaxed),
          .DeformationHits = DeformationHits_.load(std::memory_order_relaxed),
          .DeformationMisses = DeformationMisses_.load(std::memory_order_relaxed),
          .DeformationWrites = DeformationWrites_.load(std::memory_order_relaxed),
          .DeformationReadBytes = DeformationReadBytes_.load(std::memory_order_relaxed),
          .PatchHits = PatchHits_.load(std::memory_order_relaxed),
          .PatchMisses = PatchMisses_.load(std::memory_order_relaxed),
          .PatchWrites = PatchWrites_.load(std::memory_order_relaxed),
          .PatchReadBytes = PatchReadBytes_.load(std::memory_order_relaxed),
          .PatchPreparations = PatchPreparations_.load(std::memory_order_relaxed),
          .PatchPublicationAttempts = PatchPublicationAttempts_.load(std::memory_order_relaxed),
          .PatchDecodes = PatchDecodes_.load(std::memory_order_relaxed)};
}

std::string PreparedTerrainAssets::PatchKey(Data::TileId at,
                                            const ::outshine::Ground::TerrainTiles::Shaped &shape,
                                            int side,
                                            int blockZoom) const {
  if (at.Zoom < 0 || at.Zoom > Data::TileId::MaximumZoom ||
      at.X >= (uint32_t{1} << static_cast<unsigned>(at.Zoom)) ||
      at.Y >= (uint32_t{1} << static_cast<unsigned>(at.Zoom)) || side < 2 ||
      side > kPreparedGroundPatchSideMost || blockZoom < 0 ||
      blockZoom > Data::TileId::MaximumZoom) {
    return {};
  }
  const auto field = Key(Recipe_, at, shape);
  if (field.empty()) { return {}; }
  const auto parameters = "terrain-sampling-patch-1:" + field + ":" + std::to_string(side) + ":" +
                          std::to_string(blockZoom);
  return Sha256Hex(parameters.data(), parameters.size());
}

std::string PreparedTerrainAssets::DescribeFailure(const Failure &failure) {
  if (const auto *message = std::get_if<std::string>(&failure)) { return *message; }
  switch (std::get<AssetCacheError>(failure)) {
    case AssetCacheError::InvalidInput: return "invalid terrain asset cache input";
    case AssetCacheError::Storage: return "terrain asset cache storage failed";
    case AssetCacheError::Busy: return "terrain asset cache publication is busy";
    case AssetCacheError::UnsupportedVersion: return "unsupported terrain asset cache version";
    case AssetCacheError::CapacityExceeded: return "terrain asset package exceeds its byte budget";
  }
  return "unknown terrain asset cache failure";
}

std::expected<std::shared_ptr<const GroundPatch>, PreparedTerrainAssets::Failure>
PreparedTerrainAssets::LoadPatch(const std::string &key, Data::TileId at, int side) {
  const auto request = MutexForRequest(key);
  const std::scoped_lock preparing(*request);
  auto loaded = Cache_->Load(key, kPreparedGroundPatchBytesMost);
  if (!loaded) { return std::unexpected(loaded.error()); }
  if (*loaded) {
    PatchReadBytes_.fetch_add((**loaded).Bytes().size(), std::memory_order_relaxed);
    if ((**loaded).Record().Kind != "terrain-sampling-patch") {
      return std::unexpected(
          std::string("terrain sampling patch belongs to another native product kind"));
    }
    ++PatchDecodes_;
    auto patch = DecodePreparedGroundPatch(at, side, (**loaded).Bytes());
    if (patch) {
      ++PatchHits_;
      return patch;
    }
    const auto removed = Cache_->Remove(key);
    if (!removed) { return std::unexpected(removed.error()); }
  }
  ++PatchMisses_;
  return std::shared_ptr<const GroundPatch>{};
}

std::expected<PreparedTerrainAssets::PatchWrite, PreparedTerrainAssets::Failure>
PreparedTerrainAssets::PreparePatch(const std::string &key,
                                    Data::TileId at,
                                    const GroundPatch &patch) {
  auto encoded = EncodePreparedGroundPatch(at, patch);
  if (!encoded) { return std::unexpected(std::string("terrain sampling patch encoding failed")); }
  const auto range = std::ranges::minmax_element(patch.HeightsAslM());
  std::vector<AssetRecord> records{{.Key = key,
                                    .Kind = "terrain-sampling-patch",
                                    .Bounds = Bounds(at, *range.min, *range.max),
                                    .Package = {},
                                    .ByteCount = encoded->size(),
                                    .Level = static_cast<uint32_t>(at.Zoom),
                                    .Parent = {}}};
  ++PatchPreparations_;
  auto package = Cache_->PreparePublication(std::move(records), std::move(*encoded));
  if (!package) { return std::unexpected(package.error()); }
  return PatchWrite(key, at, patch.Side(), std::move(*package));
}

std::expected<std::shared_ptr<const GroundPatch>, PreparedTerrainAssets::Failure>
PreparedTerrainAssets::StorePatch(PatchWrite &write, const std::stop_token &stop) {
  if (stop.stop_requested()) {
    return std::unexpected(std::string("terrain sampling patch canceled"));
  }
  if (write.Ready_) { return write.Ready_; }
  if (!write.Published_) {
    if (!write.Package_) { return std::unexpected(std::string("terrain patch lost its package")); }
    const auto request = MutexForRequest(write.Key_);
    const std::scoped_lock preparing(*request);
    ++PatchPublicationAttempts_;
    const auto published = Cache_->Publish(*write.Package_);
    if (!published) { return std::unexpected(published.error()); }
    ++PatchWrites_;
    write.Published_ = true;
  }
  if (stop.stop_requested()) {
    return std::unexpected(std::string("terrain sampling patch canceled"));
  }
  auto ready = LoadPatch(write.Key_, write.At_, write.Side_);
  if (!ready) { return std::unexpected(std::move(ready.error())); }
  if (!*ready) { return std::unexpected(std::string("stored terrain sampling patch is absent")); }
  if (stop.stop_requested()) {
    return std::unexpected(std::string("terrain sampling patch canceled"));
  }
  write.Ready_ = std::move(*ready);
  write.Package_.reset();
  return write.Ready_;
}

}
