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
  auto prepared = PrepareBasis(key, bounds, basis, requestKey);
  if (!prepared) { return std::unexpected(prepared.error()); }
  const std::scoped_lock lock(Lock_);
  const auto published = Cache_->Publish(*prepared);
  if (!published) { return std::unexpected(CacheFailure(published.error())); }
  ResidentBases_.erase(BasisKey(key));
  ++BasisWrites_;
  return {};
}

std::expected<PreparedAssetWrite, StructureBakeError>
PreparedBuildingAssets::PrepareBasis(const std::string &key,
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
  auto prepared = Cache_->PreparePublication({std::move(record)}, std::move(*bytes));
  if (!prepared) { return std::unexpected(CacheFailure(prepared.error())); }
  return std::move(*prepared);
}

std::expected<bool, StructureBakeError>
PreparedBuildingAssets::RetireUnavailableBasis(const std::string &key) {
  const std::scoped_lock lock(Lock_);
  const auto basisKey = BasisKey(key);
  const auto retained = Cache_->LoadValidated(
      basisKey,
      "building-basis",
      kPackageBytesMost,
      [&](const CachedAsset &asset) -> std::expected<bool, AssetCacheError> {
        if (asset.Record().Parent != key ||
            !DecodePreparedBuildingBasis(asset.Bytes(), kResidentBytesMost)) {
          return false;
        }
        const auto base = Cache_->Load(key, kPackageBytesMost);
        if (!base) { return std::unexpected(base.error()); }
        if (!*base) { return false; }
        return DecodePreparedStructureIndex((**base).Bytes(), kResidentBytesMost).has_value() ||
               DecodePreparedStructureTile((**base).Bytes(), kResidentBytesMost).has_value();
      });
  if (!retained) {
    if (retained.error() != AssetCacheError::InvalidInput) {
      return std::unexpected(CacheFailure(retained.error()));
    }
  } else if (*retained) {
    return false;
  }
  ResidentBases_.erase(basisKey);
  return true;
}

std::expected<void, StructureBakeError>
PreparedBuildingAssets::BindBasisRequest(const std::string &key, const std::string &requestKey) {
  if (key.empty() || requestKey.empty()) {
    return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct);
  }
  const std::scoped_lock lock(Lock_);
  const auto bound = Cache_->BindRequest(BasisKey(key), requestKey);
  if (!bound) { return std::unexpected(CacheFailure(bound.error())); }
  if (!*bound) { return std::unexpected(StructureBakeErrorKind::ArtifactFailure); }
  return {};
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

std::expected<std::optional<PreparedBuildingAssets::RequestedBasis>, StructureBakeError>
PreparedBuildingAssets::LoadBasisRequest(const std::string &requestKey) {
  auto found = [&] {
    const std::scoped_lock lock(Lock_);
    return Cache_->FindRequest(requestKey);
  }();
  if (!found) { return std::unexpected(CacheFailure(found.error())); }
  if (!*found) { return std::optional<RequestedBasis>{}; }
  auto record = std::move(**found);
  record.RequestKey.reset();
  if (record.Kind != "building-basis" || record.Key != BasisKey(record.Parent)) {
    ++BasisMisses_;
    return std::optional<RequestedBasis>{};
  }
  {
    const std::scoped_lock lock(Lock_);
    const auto resident = ResidentBases_.find(record.Key);
    if (resident != ResidentBases_.end()) {
      if (resident->second.Record == record) {
        if (auto product = resident->second.Product.lock()) {
          Cache_->UsePackage(record.Package);
          ++BasisResidentHits_;
          return RequestedBasis{
              .BaseKey = record.Parent, .Product = std::move(product), .Bounds = record.Bounds};
        }
      }
      ResidentBases_.erase(resident);
    }
  }
  auto basis = LoadBasis(record.Parent, requestKey);
  if (!basis) { return std::unexpected(basis.error()); }
  if (!*basis) { return std::optional<RequestedBasis>{}; }
  return RequestedBasis{
      .BaseKey = record.Parent, .Product = std::move(*basis), .Bounds = record.Bounds};
}
}
