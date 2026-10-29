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
  std::string RequestKey;
  std::optional<AssetRecord> Binding;
  std::optional<AssetRecord> ProductRecord;
  bool BindingPending = false;
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
  return State_ && (State_->BindingPending || (State_->Current != State::Phase::Ready &&
                                               State_->Current != State::Phase::Missing));
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
      auto cached = LoadCachedBasis(key, state.ProductRecord);
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

std::expected<std::optional<PreparedBuildingAssets::RequestedBasis>, StructureBakeError>
PreparedBuildingAssets::ReadBasisRequest(const std::string &requestKey,
                                         BasisRead &read,
                                         const std::atomic_bool &stopping) {
  if (stopping.load(std::memory_order_relaxed)) {
    read = BasisRead{};
    return std::unexpected(StructureBakeErrorKind::Cancelled);
  }
  if (read.State_ && read.State_->RequestKey != requestKey) {
    return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct);
  }
  auto found = [&] {
    const std::scoped_lock lock(Lock_);
    return Cache_->FindRequest(requestKey);
  }();
  if (!found) { return std::unexpected(CacheFailure(found.error())); }
  if (*found) {
    auto record = std::move(**found);
    if (record.Kind != "building-basis" || record.Parent.empty() ||
        record.Key != BasisKey(record.Parent)) {
      return RetireBasisBinding(record, read);
    }
    record.RequestKey.reset();
    if (auto resident = LoadResidentBasis(record)) {
      ++BasisResidentHits_;
      read = BasisRead{};
      return RequestedBasis{
          .BaseKey = record.Parent, .Product = std::move(resident), .Bounds = record.Bounds};
    }
    if (read.State_ && (read.State_->Key != record.Parent ||
                        (read.State_->Binding && *read.State_->Binding != record))) {
      read = BasisRead{};
    }
    if (!read.State_) {
      read.State_ = std::make_unique<BasisRead::State>();
      read.State_->Key = record.Parent;
      read.State_->RequestKey = requestKey;
      read.State_->BindingPending = true;
    }
    read.State_->Binding = std::move(record);
  } else if (!read.State_ || read.State_->Key.empty()) {
    read = BasisRead{};
    return std::optional<RequestedBasis>{};
  }
  auto basis = ReadBasis(read.State_->Key, read, stopping);
  if (!basis) { return std::unexpected(basis.error()); }
  if (!*basis) {
    if (read.State_->Current == BasisRead::State::Phase::Missing) { read = BasisRead{}; }
    return std::optional<RequestedBasis>{};
  }
  return FinishBasisRequest(read, stopping);
}

std::expected<std::optional<PreparedBuildingAssets::RequestedBasis>, StructureBakeError>
PreparedBuildingAssets::RetireBasisBinding(const AssetRecord &binding, BasisRead &read) {
  if (!binding.RequestKey) {
    return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct);
  }
  if (!read.State_ || !read.State_->Key.empty() || read.State_->Binding != binding) {
    read = BasisRead{};
    read.State_ = std::make_unique<BasisRead::State>();
    read.State_->RequestKey = *binding.RequestKey;
    read.State_->Binding = binding;
    read.State_->BindingPending = true;
  }
  const auto removed = [&] {
    const auto lock = std::scoped_lock(Lock_);
    return Cache_->UnbindRequestIfMatches(binding);
  }();
  if (!removed) { return std::unexpected(CacheFailure(removed.error())); }
  if (*removed) {
    ++BasisMisses_;
    read = BasisRead{};
  }
  return std::optional<RequestedBasis>{};
}

std::expected<std::optional<PreparedBuildingAssets::RequestedBasis>, StructureBakeError>
PreparedBuildingAssets::FinishBasisRequest(BasisRead &read, const std::atomic_bool &stopping) {
  if (stopping.load(std::memory_order_relaxed)) {
    read = BasisRead{};
    return std::unexpected(StructureBakeErrorKind::Cancelled);
  }
  auto &state = *read.State_;
  const auto bound = BindBasisRequestIfUnbound(state.Key, state.RequestKey);
  if (!bound) { return std::unexpected(bound.error()); }
  const auto found = [&] {
    const std::scoped_lock lock(Lock_);
    return Cache_->FindRequest(state.RequestKey);
  }();
  if (!found) { return std::unexpected(CacheFailure(found.error())); }
  if (!*found) {
    if (!*bound) {
      read = BasisRead{};
    } else {
      state.Current = BasisRead::State::Phase::Cached;
      state.Product.reset();
      state.ProductRecord.reset();
    }
    return std::optional<RequestedBasis>{};
  }
  if (!*bound || (**found).Key != BasisKey(state.Key)) {
    state.Binding.reset();
    state.Key.clear();
    state.Product.reset();
    state.ProductRecord.reset();
    state.Current = BasisRead::State::Phase::Cached;
    return std::optional<RequestedBasis>{};
  }
  auto record = **found;
  record.RequestKey.reset();
  auto product = state.Product;
  if (!state.ProductRecord || *state.ProductRecord != record) {
    auto cached = LoadCachedBasis(state.Key, state.ProductRecord);
    if (!cached) { return std::unexpected(cached.error()); }
    product = std::move(*cached);
  }
  if (!product) {
    state.Current = BasisRead::State::Phase::Cached;
    state.Product.reset();
    return std::optional<RequestedBasis>{};
  }
  if (!state.ProductRecord) {
    return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct);
  }
  state.Product = product;
  state.BindingPending = false;
  return RequestedBasis{
      .BaseKey = state.Key, .Product = std::move(product), .Bounds = state.ProductRecord->Bounds};
}
}
