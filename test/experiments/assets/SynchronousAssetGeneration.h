#ifndef OUTSHINE_TEST_EXPERIMENTS_ASSETS_SYNCHRONOUSASSETGENERATION_H
#define OUTSHINE_TEST_EXPERIMENTS_ASSETS_SYNCHRONOUSASSETGENERATION_H

#include "AssetCache.h"
#include "AssetPackage.h"
#include <algorithm>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Experiments {
namespace SynchronousAssetDetail {

inline const char *Describe(AssetCacheError error) {
  switch (error) {
    case AssetCacheError::InvalidInput: return "invalid asset cache input";
    case AssetCacheError::Storage: return "asset cache storage failed";
    case AssetCacheError::Busy: return "asset cache publication is busy";
    case AssetCacheError::UnsupportedVersion: return "unsupported asset cache version";
    case AssetCacheError::CapacityExceeded: return "asset package exceeds its byte budget";
  }
  return "unknown asset cache failure";
}

enum class Address { Product, Request };

inline std::expected<std::optional<CachedAsset>, AssetCacheError>
Load(const AssetCache &cache, std::string_view identity, size_t packageBytesMost, Address address) {
  if (address == Address::Product) { return cache.Load(identity, packageBytesMost); }
  return cache.LoadRequest(identity, packageBytesMost);
}

inline std::string_view Identity(const AssetRecord &record, Address address) {
  if (address == Address::Product) { return record.Key; }
  return record.RequestKey ? std::string_view(*record.RequestKey) : std::string_view{};
}

inline std::expected<void, std::string> CommitPackage(AssetCache &cache,
                                                      std::string_view key,
                                                      size_t packageBytesMost,
                                                      const AssetPackage &package,
                                                      Address address) {
  if (package.Bytes.size() > packageBytesMost) {
    return std::unexpected(Describe(AssetCacheError::CapacityExceeded));
  }
  if (std::ranges::count_if(package.Records, [key, address](const AssetRecord &record) {
        return Identity(record, address) == key;
      }) != 1) {
    return std::unexpected("generator omitted the requested asset");
  }
  const auto published = cache.Publish(package.Records, package.Bytes);
  if (!published) { return std::unexpected(Describe(published.error())); }
  return {};
}

inline std::expected<CachedAsset, std::string> Resolve(AssetCache &cache,
                                                       std::string_view key,
                                                       size_t packageBytesMost,
                                                       const AssetPackageFactory &factory,
                                                       Address address) {
  auto loaded = Load(cache, key, packageBytesMost, address);
  if (!loaded) { return std::unexpected(Describe(loaded.error())); }
  if (*loaded) { return std::move(**loaded); }
  if (!factory) { return std::unexpected("asset miss has no registered generator"); }
  auto generated = factory(packageBytesMost);
  if (!generated) { return std::unexpected(std::move(generated.error())); }
  auto committed = CommitPackage(cache, key, packageBytesMost, *generated, address);
  if (!committed) { return std::unexpected(std::move(committed.error())); }
  loaded = Load(cache, key, packageBytesMost, address);
  if (!loaded) { return std::unexpected(Describe(loaded.error())); }
  if (!*loaded) { return std::unexpected("published asset package is unavailable"); }
  return std::move(**loaded);
}
}

inline std::expected<AssetRecord, std::string> CommitAsset(AssetCache &cache,
                                                           std::string_view key,
                                                           size_t packageBytesMost,
                                                           const AssetPackage &package) {
  auto committed = SynchronousAssetDetail::CommitPackage(
      cache, key, packageBytesMost, package, SynchronousAssetDetail::Address::Product);
  if (!committed) { return std::unexpected(std::move(committed.error())); }
  auto record = cache.Find(key);
  if (!record) { return std::unexpected(SynchronousAssetDetail::Describe(record.error())); }
  if (!*record) { return std::unexpected("published asset package is unavailable"); }
  return std::move(**record);
}

inline std::expected<CachedAsset, std::string> ResolveAsset(AssetCache &cache,
                                                            std::string_view key,
                                                            size_t packageBytesMost,
                                                            const AssetPackageFactory &factory) {
  return SynchronousAssetDetail::Resolve(
      cache, key, packageBytesMost, factory, SynchronousAssetDetail::Address::Product);
}

inline std::expected<CachedAsset, std::string>
ResolveAssetRequest(AssetCache &cache,
                    std::string_view requestKey,
                    size_t packageBytesMost,
                    const AssetPackageFactory &factory) {
  return SynchronousAssetDetail::Resolve(
      cache, requestKey, packageBytesMost, factory, SynchronousAssetDetail::Address::Request);
}

}
#endif
