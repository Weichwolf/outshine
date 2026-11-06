#include "PreparedBuildingAssets.h"
#include "BuildingSourceMetadata.h"
#include "Sha256.h"
#include <expected>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Generators {
namespace {
std::string MetadataKey(std::string_view basisKey, std::string_view packageKey) {
  return Sha256Hex("building-source-metadata-1/" + std::string(basisKey) + "/" +
                   std::string(packageKey));
}
}

std::expected<std::optional<PreparedBuildingAssets::RequestedSource>, StructureBakeError>
PreparedBuildingAssets::LoadSourceRequest(const std::string &requestKey) {
  auto found = [&] {
    const std::scoped_lock lock(Lock_);
    return Cache_->FindRequest(requestKey);
  }();
  if (!found) { return std::unexpected(CacheFailure(found.error())); }
  if (!*found) {
    ++SourceMetadataMisses_;
    return std::optional<RequestedSource>{};
  }
  const auto &record = **found;
  if (record.Kind == "building-basis" && !record.Parent.empty() &&
      record.Key == BasisKey(record.Parent)) {
    std::optional<BuildingSourceMetadata> metadata;
    auto loaded = [&] {
      const std::scoped_lock lock(Lock_);
      return Cache_->LoadValidated(MetadataKey(record.Key, record.Package),
                                   "building-source-metadata",
                                   BuildingSourceMetadata::EncodedBytes,
                                   [&](const CachedAsset &asset) {
                                     metadata = DecodeBuildingSourceMetadata(asset.Bytes());
                                     return asset.Record().Parent == record.Key &&
                                            asset.Record().Bounds == record.Bounds &&
                                            metadata.has_value();
                                   });
    }();
    if (!loaded && loaded.error() != AssetCacheError::InvalidInput &&
        loaded.error() != AssetCacheError::CapacityExceeded) {
      return std::unexpected(CacheFailure(loaded.error()));
    }
    if (loaded && *loaded && metadata) {
      {
        const std::scoped_lock lock(Lock_);
        Cache_->UsePackage(record.Package);
      }
      ++SourceMetadataHits_;
      SourceMetadataReadBytes_ += (**loaded).Bytes().size();
      return RequestedSource{
          .BaseKey = record.Parent, .Bounds = record.Bounds, .Metadata = *metadata};
    }
  }
  ++SourceMetadataMisses_;
  auto basis = LoadBasisRequest(requestKey);
  if (!basis) { return std::unexpected(basis.error()); }
  if (!*basis) { return std::optional<RequestedSource>{}; }
  const auto &source = **basis;
  const auto metadata = BuildingSourceMetadata::Of(*source.Product);
  auto bytes = metadata ? EncodeBuildingSourceMetadata(*metadata) : std::nullopt;
  if (!bytes || source.PackageKey.empty()) {
    return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct);
  }
  const auto basisKey = BasisKey(source.BaseKey);
  const AssetRecord summary{.Key = MetadataKey(basisKey, source.PackageKey),
                            .Kind = "building-source-metadata",
                            .Bounds = source.Bounds,
                            .Package = {},
                            .ByteCount = bytes->size(),
                            .Parent = basisKey};
  const auto published = [&] {
    const std::scoped_lock lock(Lock_);
    return Cache_->Publish({&summary, 1}, *bytes);
  }();
  if (!published) { return std::unexpected(CacheFailure(published.error())); }
  ++SourceMetadataWrites_;
  return RequestedSource{.BaseKey = source.BaseKey, .Bounds = source.Bounds, .Metadata = *metadata};
}
}
