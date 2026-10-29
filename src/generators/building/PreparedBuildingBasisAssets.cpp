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

std::expected<PreparedAssetWrite, StructureBakeError>
PreparedBuildingAssets::PrepareBasis(const std::string &key,
                                     const Box &bounds,
                                     const PreparedBuildingBasis &basis,
                                     const std::string &requestKey) {
  ++BasisPreparations_;
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
PreparedBuildingAssets::LoadCachedBasis(const std::string &key,
                                        std::optional<AssetRecord> &record) {
  record.reset();
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
  record = (**loaded).Record();
  ++BasisHits_;
  BasisReadBytes_ += (**loaded).Bytes().size();
  auto product = std::make_shared<const PreparedBuildingBasis>(std::move(*basis));
  if (ResidentBases_.size() >= kResidentBasisEntriesMost) {
    std::erase_if(ResidentBases_, [](const auto &entry) { return entry.second.Product.expired(); });
  }
  if (ResidentBases_.size() < kResidentBasisEntriesMost || ResidentBases_.contains(record->Key)) {
    ResidentBases_.insert_or_assign(record->Key,
                                    ResidentBasis{.Record = *record, .Product = product});
  }
  return product;
}

PreparedBuildingAssets::Basis PreparedBuildingAssets::LoadResidentBasis(const AssetRecord &record) {
  const std::scoped_lock lock(Lock_);
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

std::expected<bool, StructureBakeError>
PreparedBuildingAssets::BindBasisRequestIfUnbound(const std::string &key,
                                                  const std::string &requestKey) {
  const std::scoped_lock lock(Lock_);
  const auto bound = Cache_->BindRequestIfUnbound(BasisKey(key), requestKey);
  if (!bound) { return std::unexpected(CacheFailure(bound.error())); }
  return *bound;
}

std::expected<std::optional<PreparedBuildingAssets::RequestedBasis>, StructureBakeError>
PreparedBuildingAssets::LoadBasisRequest(const std::string &requestKey) {
  BasisRead read;
  const std::atomic_bool stopping{false};
  for (;;) {
    auto result = ReadBasisRequest(requestKey, read, stopping);
    if (!result || *result || !read.Pending()) { return result; }
  }
}
}
