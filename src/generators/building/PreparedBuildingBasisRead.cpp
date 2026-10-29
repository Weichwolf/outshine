#include "PreparedBuildingAssets.h"
#include <atomic>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace outshine::Generators {
struct PreparedBuildingAssets::BasisRead::State {
  enum class Phase { Cached, Base, Publish, Ready, Missing };
  std::string Key;
  std::optional<BaseWrite> Migration;
  std::optional<PreparedAssetWrite> Package;
  Basis Product;
  Phase Current = Phase::Cached;
};

PreparedBuildingAssets::BasisRead::BasisRead() noexcept = default;
PreparedBuildingAssets::BasisRead::BasisRead(BasisRead &&) noexcept = default;
PreparedBuildingAssets::BasisRead &
PreparedBuildingAssets::BasisRead::operator=(BasisRead &&) noexcept = default;
PreparedBuildingAssets::BasisRead::~BasisRead() = default;

bool PreparedBuildingAssets::BasisRead::Pending() const noexcept {
  return State_ && State_->Current != State::Phase::Ready &&
         State_->Current != State::Phase::Missing;
}

std::expected<PreparedBuildingAssets::Basis, StructureBakeError> PreparedBuildingAssets::ReadBasis(
    const std::string &key, BasisRead &read, const std::atomic_bool &stopping) {
  using Phase = BasisRead::State::Phase;
  if (stopping.load(std::memory_order_relaxed)) {
    read = BasisRead{};
    return std::unexpected(StructureBakeErrorKind::Cancelled);
  }
  if (!read.State_) {
    read.State_ = std::make_unique<BasisRead::State>();
    read.State_->Key = key;
  }
  auto &state = *read.State_;
  if (state.Key != key) { return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct); }
  switch (state.Current) {
    case Phase::Cached: {
      auto cached = LoadCachedBasis(key);
      if (!cached) { return std::unexpected(cached.error()); }
      if (*cached) {
        state.Product = std::move(*cached);
        state.Current = Phase::Ready;
        return state.Product;
      }
      ++BasisMisses_;
      state.Current = Phase::Base;
      return Basis{};
    }
    case Phase::Base: return ReadBasisInput(read, stopping);
    case Phase::Publish: {
      if (!state.Package) {
        return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct);
      }
      {
        const std::scoped_lock lock(Lock_);
        const auto stored = Cache_->Publish(*state.Package);
        if (!stored) { return std::unexpected(CacheFailure(stored.error())); }
        ResidentBases_.erase(BasisKey(key));
        ++BasisWrites_;
      }
      state.Package.reset();
      state.Current = Phase::Ready;
      if (stopping.load(std::memory_order_relaxed)) {
        read = BasisRead{};
        return std::unexpected(StructureBakeErrorKind::Cancelled);
      }
      return state.Product;
    }
    case Phase::Ready: return state.Product;
    case Phase::Missing: return Basis{};
  }
  std::unreachable();
}

std::expected<PreparedBuildingAssets::Basis, StructureBakeError>
PreparedBuildingAssets::ReadBasisInput(BasisRead &read, const std::atomic_bool &stopping) {
  using Phase = BasisRead::State::Phase;
  auto &state = *read.State_;
  const auto &key = state.Key;
  const bool migrating = state.Migration.has_value();
  auto base = ReadBase(key, state.Migration, stopping);
  if (!base) { return std::unexpected(base.error()); }
  if (!*base) {
    if (!state.Migration) { state.Current = Phase::Missing; }
    return Basis{};
  }
  if (migrating) {
    state.Current = Phase::Cached;
    return Basis{};
  }
  if (stopping.load(std::memory_order_relaxed)) {
    read = BasisRead{};
    return std::unexpected(StructureBakeErrorKind::Cancelled);
  }
  state.Product = std::make_shared<const PreparedBuildingBasis>(PreparedBuildingBasis::Of(**base));
  auto package = PrepareBasis(key, Bounds(**base), *state.Product, {});
  if (!package) { return std::unexpected(package.error()); }
  state.Package = std::move(*package);
  state.Current = Phase::Publish;
  return Basis{};
}

std::expected<PreparedBuildingAssets::Basis, StructureBakeError>
PreparedBuildingAssets::LoadBasis(const std::string &key, const std::string &requestKey) {
  BasisRead read;
  const std::atomic_bool stopping{false};
  for (;;) {
    auto result = ReadBasis(key, read, stopping);
    if (!result) { return result; }
    if (*result) {
      if (!requestKey.empty()) {
        const auto bound = BindBasisRequest(key, requestKey);
        if (!bound) { return std::unexpected(bound.error()); }
      }
      return result;
    }
    if (!read.Pending()) { return result; }
  }
}
}
