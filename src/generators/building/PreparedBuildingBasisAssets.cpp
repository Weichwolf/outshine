#include "PreparedBuildingAssets.h"
#include "PreparedStructureCodec.h"
#include "Sha256.h"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace outshine::Generators {
StructureBakeError PreparedBuildingAssets::CacheFailure(AssetCacheError error) noexcept {
  return error == AssetCacheError::Busy ? StructureBakeErrorKind::ArtifactBusy
                                        : StructureBakeErrorKind::ArtifactFailure;
}

std::string PreparedBuildingAssets::BasisKey(const std::string &key) {
  return Sha256Hex("prepared-building-basis-2/" + key);
}

std::expected<void, StructureBakeError>
PreparedBuildingAssets::StoreBasis(const std::string &key,
                                   const Box &bounds,
                                   const PreparedBuildingBasis &basis,
                                   const std::string &requestKey) {
  auto bytes = EncodePreparedBuildingBasis(basis);
  if (!bytes || bytes->size() > kPackageBytesMost) {
    return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct);
  }
  AssetRecord record{.Key = BasisKey(key),
                     .Kind = "building-basis",
                     .Bounds = bounds,
                     .Package = {},
                     .ByteCount = bytes->size(),
                     .Parent = key};
  if (!requestKey.empty()) { record.RequestKey = requestKey; }
  const std::scoped_lock lock(Lock_);
  const auto published = Cache_->Publish(std::span(&record, 1), *bytes);
  if (!published) { return std::unexpected(CacheFailure(published.error())); }
  ResidentBases_.erase(record.Key);
  ++BasisWrites_;
  return {};
}

std::expected<void, StructureBakeError>
PreparedBuildingAssets::InvalidateBasis(const std::string &key) {
  const std::scoped_lock lock(Lock_);
  const auto basisKey = BasisKey(key);
  const auto removed = Cache_->Remove(basisKey);
  if (!removed) { return std::unexpected(CacheFailure(removed.error())); }
  ResidentBases_.erase(basisKey);
  return {};
}

std::expected<bool, StructureBakeError>
PreparedBuildingAssets::BindBasisRequestIfUnbound(const std::string &key,
                                                  const std::string &requestKey) {
  if (key.empty() || requestKey.empty()) {
    return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct);
  }
  const auto lock = std::scoped_lock(Lock_);
  const auto bound = Cache_->BindRequestIfUnbound(BasisKey(key), requestKey);
  if (!bound) { return std::unexpected(CacheFailure(bound.error())); }
  return *bound;
}

std::expected<PreparedBuildingAssets::Basis, StructureBakeError>
PreparedBuildingAssets::LoadCachedBasis(const std::string &key, const std::string &requestKey) {
  const std::scoped_lock lock(Lock_);
  std::optional<PreparedBuildingBasis> basis;
  auto loaded = Cache_->LoadValidated(
      BasisKey(key), "building-basis", kPackageBytesMost, [&](const CachedAsset &asset) {
        basis = DecodePreparedBuildingBasis(asset.Bytes(), kResidentBytesMost);
        return asset.Record().Parent == key && basis.has_value();
      });
  if (!loaded) {
    if (loaded.error() == AssetCacheError::InvalidInput) { return Basis{}; }
    return std::unexpected(CacheFailure(loaded.error()));
  }
  if (!*loaded || !basis) { return Basis{}; }
  const auto &record = (**loaded).Record();
  if (!requestKey.empty()) {
    const auto bound = Cache_->BindRequest(BasisKey(key), requestKey);
    if (!bound) { return std::unexpected(CacheFailure(bound.error())); }
    if (!*bound) { return std::unexpected(StructureBakeErrorKind::ArtifactFailure); }
  }
  ++BasisHits_;
  BasisReadBytes_ += (**loaded).Bytes().size();
  auto product = std::make_shared<const PreparedBuildingBasis>(std::move(*basis));
  if (ResidentBases_.size() >= kResidentBasisEntriesMost) {
    std::erase_if(ResidentBases_, [](const auto &entry) { return entry.second.Product.expired(); });
  }
  if (ResidentBases_.size() < kResidentBasisEntriesMost || ResidentBases_.contains(record.Key)) {
    ResidentBases_.insert_or_assign(record.Key,
                                    ResidentBasis{.Record = record, .Product = product});
  }
  return product;
}

std::expected<PreparedBuildingAssets::Basis, StructureBakeError>
PreparedBuildingAssets::LoadBasis(const std::string &key, const std::string &requestKey) {
  auto cached = LoadCachedBasis(key, requestKey);
  if (!cached) { return std::unexpected(cached.error()); }
  if (*cached) { return cached; }
  ++BasisMisses_;
  const auto base = Load(key);
  if (!base) { return std::unexpected(base.error()); }
  if (!*base) { return Basis{}; }
  auto basis = std::make_shared<const PreparedBuildingBasis>(PreparedBuildingBasis::Of(**base));
  const auto stored = StoreBasis(key, Bounds(**base), *basis, requestKey);
  if (!stored) { return std::unexpected(stored.error()); }
  return basis;
}

PreparedBuildingAssets::Basis PreparedBuildingAssets::LoadResidentBasis(const AssetRecord &record) {
  const auto lock = std::scoped_lock(Lock_);
  const auto resident = ResidentBases_.find(record.Key);
  if (resident == ResidentBases_.end()) { return {}; }
  if (resident->second.Record == record) {
    if (auto product = resident->second.Product.lock()) {
      Cache_->UsePackage(record.Package);
      return product;
    }
  }
  ResidentBases_.erase(resident);
  return {};
}

std::expected<std::optional<PreparedBuildingAssets::RequestedBasis>, StructureBakeError>
PreparedBuildingAssets::LoadBasisRequest(const std::string &requestKey) {
  constexpr unsigned kAttemptsMost = 2;
  for (unsigned attempt = 0; attempt < kAttemptsMost; ++attempt) {
    auto found = [&] {
      const std::scoped_lock lock(Lock_);
      return Cache_->FindRequest(requestKey);
    }();
    if (!found) { return std::unexpected(CacheFailure(found.error())); }
    if (!*found) { return std::optional<RequestedBasis>{}; }
    auto record = std::move(**found);
    if (record.Kind != "building-basis" || record.Parent.empty() ||
        record.Key != BasisKey(record.Parent)) {
      const auto removed = [&] {
        const auto lock = std::scoped_lock(Lock_);
        return Cache_->UnbindRequestIfMatches(record);
      }();
      if (!removed) { return std::unexpected(CacheFailure(removed.error())); }
      ++BasisMisses_;
      return std::optional<RequestedBasis>{};
    }
    record.RequestKey.reset();
    if (auto product = LoadResidentBasis(record)) {
      ++BasisResidentHits_;
      return RequestedBasis{
          .BaseKey = record.Parent, .Product = std::move(product), .Bounds = record.Bounds};
    }
    auto basis = LoadBasis(record.Parent);
    if (!basis) { return std::unexpected(basis.error()); }
    if (!*basis) { return std::optional<RequestedBasis>{}; }
    const auto bound = BindBasisRequestIfUnbound(record.Parent, requestKey);
    if (!bound) { return std::unexpected(bound.error()); }
    auto current = [&] {
      const auto lock = std::scoped_lock(Lock_);
      return Cache_->FindRequest(requestKey);
    }();
    if (!current) { return std::unexpected(CacheFailure(current.error())); }
    if (!*current) { return std::optional<RequestedBasis>{}; }
    (**current).RequestKey.reset();
    if (**current != record) { continue; }
    return RequestedBasis{
        .BaseKey = record.Parent, .Product = std::move(*basis), .Bounds = record.Bounds};
  }
  return std::unexpected(StructureBakeErrorKind::ArtifactBusy);
}
}
