#include "TerrainFieldPreparation.h"
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
  auto assets = std::shared_ptr<PreparedTerrainAssets>(
      new PreparedTerrainAssets(std::move(*cache), std::move(recipe)));
  assets->Self_ = assets;
  return assets;
}

std::shared_ptr<PreparedTerrainAssets::Request>
PreparedTerrainAssets::RequestFor(const std::string &key) {
  const std::scoped_lock lock(Lock_);
  std::erase_if(Requests_, [](const auto &entry) { return entry.second.expired(); });
  auto &pending = Requests_[key];
  auto request = pending.lock();
  if (!request) {
    request = std::make_shared<Request>();
    pending = request;
  }
  return request;
}

std::shared_ptr<std::mutex> PreparedTerrainAssets::MutexForRequest(const std::string &key) {
  const auto request = RequestFor(key);
  return {request, &request->Lock};
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

class PreparedTerrainAssets::FieldRead final
    : public ::outshine::Ground::TerrainTiles::FieldRequest {
public:
  FieldRead(std::shared_ptr<PreparedTerrainAssets> owner,
            std::string key,
            Data::TileId at,
            std::shared_ptr<Request> request)
      : Owner_(std::move(owner)), Key_(std::move(key)), At_(at), Request_(std::move(request)) {
    Measure();
  }

  [[nodiscard]] ::outshine::Ground::TerrainTiles::FieldStep
  Resume(const ::outshine::Ground::TerrainTiles::FieldFactory &factory,
         const std::stop_token &stop) override;

  [[nodiscard]] size_t HeapBytes() const noexcept override { return Bytes_->load(); }

  [[nodiscard]] std::shared_ptr<const std::atomic_size_t> Memory() const noexcept override {
    return Bytes_;
  }

private:
  enum class Phase { Inspect, Generate, Publish, Restore };
  [[nodiscard]] ::outshine::Ground::TerrainTiles::FieldStep Read();
  [[nodiscard]] ::outshine::Ground::TerrainTiles::FieldStep
  Generate(const ::outshine::Ground::TerrainTiles::FieldFactory &factory,
           const std::stop_token &stop);
  [[nodiscard]] ::outshine::Ground::TerrainTiles::FieldStep Failed(AssetCacheError error);
  void Measure() noexcept;

  std::shared_ptr<PreparedTerrainAssets> Owner_;
  std::string Key_;
  Data::TileId At_;
  std::shared_ptr<Request> Request_;
  std::optional<PreparedAssetWrite> Package_;
  std::weak_ptr<const ::outshine::Ground::TerrainField> Resident_;
  Phase Next_ = Phase::Inspect;
  std::shared_ptr<std::atomic_size_t> Bytes_ = std::make_shared<std::atomic_size_t>(0);
};

void PreparedTerrainAssets::FieldRead::Measure() noexcept {
  Bytes_->store(sizeof(FieldRead) + sizeof(Request) + sizeof(std::atomic_size_t) + Key_.capacity() +
                (Package_ ? Package_->CapacityBytes() - sizeof(PreparedAssetWrite) : 0u));
}

::outshine::Ground::TerrainTiles::FieldStep
PreparedTerrainAssets::FieldRead::Failed(AssetCacheError error) {
  using namespace ::outshine::Ground;
  switch (error) {
    case AssetCacheError::Busy: return {.Grid = TerrainGrid::Deferred(), .CacheDeferred = true};
    case AssetCacheError::CapacityExceeded:
      return {.Grid = Refused(At_, Data::SourceFailureReason::CapacityRefused)};
    case AssetCacheError::InvalidInput:
      return {.Grid = Refused(At_, Data::SourceFailureReason::InvalidRequest)};
    case AssetCacheError::UnsupportedVersion:
      return {.Grid = Refused(At_, Data::SourceFailureReason::CorruptPayload)};
    case AssetCacheError::Storage:
      return {.Grid = Refused(At_, Data::SourceFailureReason::Unavailable)};
  }
  std::unreachable();
}

::outshine::Ground::TerrainTiles::FieldStep PreparedTerrainAssets::FieldRead::Read() {
  using namespace ::outshine::Ground;
  std::optional<TerrainField> cached;
  auto loaded = Owner_->Cache_->LoadValidated(
      Key_, "terrain-field", kPreparedTerrainBytesMost, [&](const CachedAsset &asset) {
        cached = DecodePreparedTerrain(At_, asset.Bytes(), kPreparedTerrainBytesMost);
        return cached.has_value();
      });
  if (!loaded) { return Failed(loaded.error()); }
  if (*loaded && cached) {
    if (Next_ == Phase::Inspect) {
      Owner_->Hits_.fetch_add(1, std::memory_order_relaxed);
      Owner_->ReadBytes_.fetch_add((**loaded).Bytes().size(), std::memory_order_relaxed);
    }
    auto result = Owner_->Remember(Key_, std::move(*cached));
    Resident_ = result.ShareField();
    return {.Grid = std::move(result)};
  }
  Owner_->Misses_.fetch_add(1, std::memory_order_relaxed);
  Next_ = Phase::Generate;
  return {.Grid = TerrainGrid::Deferred()};
}

::outshine::Ground::TerrainTiles::FieldStep PreparedTerrainAssets::FieldRead::Generate(
    const ::outshine::Ground::TerrainTiles::FieldFactory &factory, const std::stop_token &stop) {
  if (!factory) { return {.Grid = Refused(At_, Data::SourceFailureReason::ProviderRefused)}; }
  auto generated = factory();
  const auto *field = generated.TryField();
  if (stop.stop_requested()) { return {.Grid = ::outshine::Ground::TerrainGrid::Deferred()}; }
  if (field == nullptr) {
    Next_ = Phase::Inspect;
    return {.Grid = std::move(generated)};
  }
  auto encoded = EncodePreparedTerrain(At_, *field);
  if (!encoded) { return {.Grid = Refused(At_, Data::SourceFailureReason::CorruptPayload)}; }
  const AssetRecord record{.Key = Key_,
                           .Kind = "terrain-field",
                           .Bounds = Bounds(At_, *field),
                           .Package = {},
                           .ByteCount = encoded->size(),
                           .Level = static_cast<uint32_t>(At_.Zoom),
                           .Parent = {}};
  if (stop.stop_requested()) { return {.Grid = ::outshine::Ground::TerrainGrid::Deferred()}; }
  auto package = Owner_->Cache_->PreparePublication({record}, std::move(*encoded));
  if (!package) { return Failed(package.error()); }
  Package_ = std::move(*package);
  Owner_->FieldPreparations_.fetch_add(1, std::memory_order_relaxed);
  Next_ = Phase::Publish;
  Measure();
  return {.Grid = ::outshine::Ground::TerrainGrid::Deferred()};
}

::outshine::Ground::TerrainTiles::FieldStep PreparedTerrainAssets::FieldRead::Resume(
    const ::outshine::Ground::TerrainTiles::FieldFactory &factory, const std::stop_token &stop) {
  using namespace ::outshine::Ground;
  if (stop.stop_requested()) { return {.Grid = TerrainGrid::Deferred()}; }
  const std::unique_lock preparing(Request_->Lock, std::try_to_lock);
  if (!preparing.owns_lock()) { return {.Grid = TerrainGrid::Deferred(), .CacheDeferred = true}; }
  if (Key_.empty()) { return {.Grid = Refused(At_, Data::SourceFailureReason::InvalidRequest)}; }
  if (Resident_.expired()) {
    const std::scoped_lock lock(Owner_->Lock_);
    const auto held = Owner_->Resident_.find(Key_);
    if (held != Owner_->Resident_.end()) { Resident_ = held->second; }
  }
  if (auto resident = Resident_.lock()) {
    Owner_->ResidentHits_.fetch_add(1, std::memory_order_relaxed);
    return {.Grid = TerrainGrid::Holding(std::move(resident))};
  }
  if (Next_ == Phase::Inspect) {
    auto read = Read();
    if (Next_ != Phase::Generate) { return read; }
  }
  if (Next_ == Phase::Generate) {
    auto generated = Generate(factory, stop);
    if (Next_ != Phase::Publish) { return generated; }
  }
  if (stop.stop_requested()) { return {.Grid = TerrainGrid::Deferred()}; }
  if (Next_ == Phase::Publish) {
    if (!Package_) { return {.Grid = Refused(At_, Data::SourceFailureReason::CorruptPayload)}; }
    Owner_->FieldPublicationAttempts_.fetch_add(1, std::memory_order_relaxed);
    const auto published = Owner_->Cache_->Publish(*Package_);
    if (!published) { return Failed(published.error()); }
    Owner_->Writes_.fetch_add(1, std::memory_order_relaxed);
    Package_.reset();
    Next_ = Phase::Restore;
    Measure();
  }
  if (stop.stop_requested()) { return {.Grid = TerrainGrid::Deferred()}; }
  auto restored = Read();
  if (restored.Grid.TryField() == nullptr && !restored.CacheDeferred) {
    return {.Grid = Refused(At_, Data::SourceFailureReason::CorruptPayload)};
  }
  return restored;
}

std::shared_ptr<::outshine::Ground::TerrainTiles::FieldRequest>
PreparedTerrainAssets::RequestField(Data::TileId at,
                                    const ::outshine::Ground::TerrainTiles::Shaped &shape) {
  const auto key = at.Zoom >= 0 && at.Zoom <= Data::TileId::MaximumZoom &&
                           at.X < (uint32_t{1} << static_cast<unsigned>(at.Zoom)) &&
                           at.Y < (uint32_t{1} << static_cast<unsigned>(at.Zoom))
                       ? Key(Recipe_, at, shape)
                       : std::string{};
  if (key.empty()) {
    return std::make_shared<FieldRead>(Self_.lock(), key, at, std::make_shared<Request>());
  }
  const auto owner = RequestFor(key);
  const std::scoped_lock lock(Lock_);
  auto request = owner->Field.lock();
  if (!request) {
    request = std::make_shared<FieldRead>(Self_.lock(), key, at, owner);
    owner->Field = request;
  }
  return request;
}

PreparedTerrainAssets::Counters PreparedTerrainAssets::Costs() const noexcept {
  return {.Hits = Hits_.load(std::memory_order_relaxed),
          .Misses = Misses_.load(std::memory_order_relaxed),
          .Resident = ResidentHits_.load(std::memory_order_relaxed),
          .Writes = Writes_.load(std::memory_order_relaxed),
          .ReadBytes = ReadBytes_.load(std::memory_order_relaxed),
          .FieldPreparations = FieldPreparations_.load(std::memory_order_relaxed),
          .FieldPublicationAttempts = FieldPublicationAttempts_.load(std::memory_order_relaxed),
          .DeformationHits = DeformationHits_.load(std::memory_order_relaxed),
          .DeformationMisses = DeformationMisses_.load(std::memory_order_relaxed),
          .DeformationWrites = DeformationWrites_.load(std::memory_order_relaxed),
          .DeformationReadBytes = DeformationReadBytes_.load(std::memory_order_relaxed),
          .DeformationPreparations = DeformationPreparations_.load(std::memory_order_relaxed),
          .DeformationPublicationAttempts =
              DeformationPublicationAttempts_.load(std::memory_order_relaxed),
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
  std::shared_ptr<const GroundPatch> patch;
  auto loaded = Cache_->LoadValidated(
      key, "terrain-sampling-patch", kPreparedGroundPatchBytesMost, [&](const CachedAsset &asset) {
        PatchReadBytes_.fetch_add(asset.Bytes().size(), std::memory_order_relaxed);
        ++PatchDecodes_;
        patch = DecodePreparedGroundPatch(at, side, asset.Bytes());
        return patch != nullptr;
      });
  if (!loaded) { return std::unexpected(loaded.error()); }
  if (*loaded && patch) {
    ++PatchHits_;
    return patch;
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
