#include "PreparedBuildingAssets.h"
#include "PreparedStructureCodec.h"
#include "Sha256.h"
#include <atomic>
#include <cstddef>
#include <expected>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <utility>

namespace outshine::Generators {
std::string PreparedBuildingAssets::BasisKey(const std::string &key) {
  return Sha256Hex("prepared-building-basis-1/" + key);
}

std::expected<void, StructureBakeError> PreparedBuildingAssets::StoreBasis(
    const std::string &key, const Box &bounds, const PreparedBuildingBasis &basis) {
  auto bytes = EncodePreparedBuildingBasis(basis);
  if (!bytes || bytes->size() > kPackageBytesMost) {
    return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct);
  }
  const AssetRecord record{.Key = BasisKey(key),
                           .Kind = "building-basis",
                           .Bounds = bounds,
                           .Package = {},
                           .ByteCount = bytes->size(),
                           .Parent = key};
  const std::scoped_lock lock(Lock_);
  if (!Cache_->Publish(std::span(&record, 1), *bytes)) {
    return std::unexpected(StructureBakeErrorKind::ArtifactFailure);
  }
  ++BasisWrites_;
  return {};
}

std::expected<void, StructureBakeError>
PreparedBuildingAssets::InvalidateBasis(const std::string &key) {
  const std::scoped_lock lock(Lock_);
  if (!Cache_->Remove(BasisKey(key))) {
    return std::unexpected(StructureBakeErrorKind::ArtifactFailure);
  }
  return {};
}

std::expected<PreparedBuildingAssets::Basis, StructureBakeError>
PreparedBuildingAssets::LoadBasis(const std::string &key) {
  {
    const std::scoped_lock lock(Lock_);
    auto loaded = Cache_->Load(BasisKey(key), kPackageBytesMost);
    if (!loaded) { return std::unexpected(StructureBakeErrorKind::ArtifactFailure); }
    if (*loaded) {
      const auto &record = (**loaded).Record();
      auto basis = DecodePreparedBuildingBasis((**loaded).Bytes(), kResidentBytesMost);
      if (record.Kind == "building-basis" && record.Parent == key && basis) {
        ++BasisHits_;
        BasisReadBytes_ += (**loaded).Bytes().size();
        return std::make_shared<const PreparedBuildingBasis>(std::move(*basis));
      }
      if (!Cache_->Remove(BasisKey(key))) {
        return std::unexpected(StructureBakeErrorKind::ArtifactFailure);
      }
    }
  }
  ++BasisMisses_;
  const auto base = Load(key);
  if (!base) { return std::unexpected(base.error()); }
  if (!*base) { return Basis{}; }
  auto basis = std::make_shared<const PreparedBuildingBasis>(PreparedBuildingBasis::Of(**base));
  const auto stored = StoreBasis(key, Bounds(**base), *basis);
  if (!stored) { return std::unexpected(stored.error()); }
  return basis;
}
}
