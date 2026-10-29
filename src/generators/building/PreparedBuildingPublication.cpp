#include "PreparedBuildingAssets.h"
#include "PreparedStructureCodec.h"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
std::expected<bool, AssetCacheError>
RestoreRequestBinding(AssetCache &cache, const std::string &key, const std::string &request) {
  auto bound = cache.BindRequestIfUnbound(key, request);
  if (!bound || *bound) { return bound; }
  auto current = cache.FindRequest(request);
  if (!current) { return std::unexpected(current.error()); }
  return current->has_value();
}
}

struct PreparedBuildingAssets::BaseWrite::State {
  enum class Phase : uint8_t { Surfaces, Index, Basis, Restore };
  std::string Key;
  std::string RequestKey;
  std::string RepairRequestKey;
  PreparedStructureTile Product;
  std::array<std::vector<const BuildingSurface *>, kStructureCellsPerTile> Cells;
  std::array<Box, kStructureCellsPerTile> Bounds;
  std::optional<PreparedAssetWrite> Package;
  uint32_t Cell = 0;
  Phase Current = Phase::Surfaces;
};

PreparedBuildingAssets::BaseWrite::BaseWrite(std::unique_ptr<State> state)
    : State_(std::move(state)) {}

PreparedBuildingAssets::BaseWrite::BaseWrite(BaseWrite &&) noexcept = default;
PreparedBuildingAssets::BaseWrite &
PreparedBuildingAssets::BaseWrite::operator=(BaseWrite &&) noexcept = default;
PreparedBuildingAssets::BaseWrite::~BaseWrite() = default;

bool PreparedBuildingAssets::BaseWrite::ForKey(std::string_view key) const noexcept {
  return State_ && State_->Key == key;
}

size_t PreparedBuildingAssets::BaseWrite::PublicationBytes() const noexcept {
  return State_ && State_->Package ? State_->Package->CapacityBytes() : 0;
}

PreparedBuildingAssets::BaseWrite PreparedBuildingAssets::PrepareBaseWrite(
    std::string key, PreparedStructureTile base, std::string requestKey) {
  auto state = std::make_unique<BaseWrite::State>();
  state->Key = std::move(key);
  state->RequestKey = std::move(requestKey);
  state->Product = std::move(base);
  for (size_t index = 0; index < state->Product.Structures.size(); ++index) {
    const auto cell = state->Product.Structures[index].Layout.Cell.Index - 1u;
    state->Cells[cell].push_back(&state->Product.Surfaces[index]);
    state->Bounds[cell].Cover(StructureBounds(state->Product, index));
  }
  return BaseWrite(std::move(state));
}

std::expected<PreparedBuildingAssets::BaseWrite, StructureBakeError>
PreparedBuildingAssets::PrepareBase(const std::string &key,
                                    const RawTile &raw,
                                    const ::outshine::Ground::HeightField &heights,
                                    const std::atomic_bool &stopping,
                                    const std::string &requestKey) {
  if (key.empty()) { return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct); }
  ++BasePreparations_;
  auto base = PrepareStructureTile(raw, heights, &stopping);
  if (!base) { return std::unexpected(base.error()); }
  if (stopping.load(std::memory_order_relaxed)) {
    return std::unexpected(StructureBakeErrorKind::Cancelled);
  }
  return PrepareBaseWrite(key, std::move(*base), requestKey);
}

std::expected<PreparedAssetWrite, StructureBakeError>
PreparedBuildingAssets::PrepareBasePackage(BaseWrite::State &state) {
  using Phase = BaseWrite::State::Phase;
  ++BasePackagePreparations_;
  if (state.Current == Phase::Surfaces) {
    return PrepareSurfaceCell(
        state.Key, state.Cell + 1u, state.Bounds[state.Cell], state.Cells[state.Cell]);
  }
  if (state.Current == Phase::Basis) {
    return PrepareBasis(state.Key,
                        Bounds(state.Product),
                        PreparedBuildingBasis::Of(state.Product),
                        state.RequestKey);
  }
  auto bytes = EncodePreparedStructureIndex(state.Product);
  if (!bytes || bytes->size() > kPackageBytesMost ||
      !DecodePreparedStructureIndex(*bytes, kResidentBytesMost)) {
    return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct);
  }
  std::vector<AssetRecord> records{{.Key = state.Key,
                                    .Kind = "buildings",
                                    .Bounds = Bounds(state.Product),
                                    .Package = {},
                                    .ByteCount = bytes->size(),
                                    .Parent = {}}};
  auto prepared = Cache_->PreparePublication(std::move(records), std::move(*bytes));
  if (!prepared) { return std::unexpected(CacheFailure(prepared.error())); }
  return std::move(*prepared);
}

std::expected<PreparedBuildingAssets::Base, StructureBakeError>
PreparedBuildingAssets::ResumeBase(BaseWrite &prepared, const std::atomic_bool &stopping) {
  using Phase = BaseWrite::State::Phase;
  if (!prepared.State_) { return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct); }
  if (stopping.load(std::memory_order_relaxed)) {
    prepared.State_.reset();
    return std::unexpected(StructureBakeErrorKind::Cancelled);
  }
  auto &state = *prepared.State_;
  if (state.Current == Phase::Restore) { return RestoreBase(prepared, stopping); }
  if (state.Current == Phase::Surfaces) {
    while (state.Cell < state.Cells.size() && state.Cells[state.Cell].empty()) { ++state.Cell; }
    if (state.Cell == state.Cells.size()) { state.Current = Phase::Index; }
  }
  if (!state.Package) {
    auto package = PrepareBasePackage(state);
    if (!package) { return std::unexpected(package.error()); }
    state.Package = std::move(*package);
  }
  if (stopping.load(std::memory_order_relaxed)) {
    prepared.State_.reset();
    return std::unexpected(StructureBakeErrorKind::Cancelled);
  }
  {
    const std::scoped_lock lock(Lock_);
    ++prepared.Attempts_;
    const auto stored = Cache_->Publish(*state.Package);
    if (!stored) { return std::unexpected(CacheFailure(stored.error())); }
    if (state.Current == Phase::Surfaces) {
      ++SurfaceCosts_.Writes;
    } else if (state.Current == Phase::Index) {
      ++Writes_;
    } else {
      ResidentBases_.erase(BasisKey(state.Key));
      ++BasisWrites_;
    }
  }
  state.Package.reset();
  if (state.Current == Phase::Surfaces) {
    ++state.Cell;
  } else {
    state.Current = state.Current == Phase::Index ? Phase::Basis : Phase::Restore;
  }
  if (stopping.load(std::memory_order_relaxed)) {
    prepared.State_.reset();
    return std::unexpected(StructureBakeErrorKind::Cancelled);
  }
  return Base{};
}

std::expected<PreparedBuildingAssets::Base, StructureBakeError>
PreparedBuildingAssets::RestoreBase(BaseWrite &prepared, const std::atomic_bool &stopping) {
  if (!prepared.State_) { return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct); }
  if (stopping.load(std::memory_order_relaxed)) {
    prepared.State_.reset();
    return std::unexpected(StructureBakeErrorKind::Cancelled);
  }
  if (!prepared.State_->RepairRequestKey.empty()) {
    const std::scoped_lock lock(Lock_);
    const auto bound = RestoreRequestBinding(
        *Cache_, BasisKey(prepared.State_->Key), prepared.State_->RepairRequestKey);
    if (!bound) { return std::unexpected(CacheFailure(bound.error())); }
    if (!*bound) {
      prepared.State_->Current = BaseWrite::State::Phase::Basis;
      return Base{};
    }
  }
  if (stopping.load(std::memory_order_relaxed)) {
    prepared.State_.reset();
    return std::unexpected(StructureBakeErrorKind::Cancelled);
  }
  std::optional<BaseWrite> replacement;
  auto loaded = ReadBase(prepared.State_->Key, replacement, stopping);
  if (replacement) {
    if (replacement->State_) {
      auto &request = prepared.State_->RepairRequestKey.empty() ? prepared.State_->RequestKey
                                                                : prepared.State_->RepairRequestKey;
      replacement->State_->RepairRequestKey = std::move(request);
    }
    prepared.Attempts_ += replacement->Attempts_;
    prepared.State_ = std::move(replacement->State_);
  }
  if (stopping.load(std::memory_order_relaxed)) {
    prepared.State_.reset();
    return std::unexpected(StructureBakeErrorKind::Cancelled);
  }
  if (!loaded) { return std::unexpected(loaded.error()); }
  if (!*loaded) {
    if (replacement && prepared.State_) { return Base{}; }
    return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct);
  }
  prepared.State_.reset();
  return loaded;
}

std::expected<PreparedBuildingAssets::Base, StructureBakeError>
PreparedBuildingAssets::CompleteBase(BaseWrite &prepared, const std::atomic_bool &stopping) {
  for (;;) {
    auto loaded = ResumeBase(prepared, stopping);
    if (!loaded || *loaded) { return loaded; }
  }
}
}
