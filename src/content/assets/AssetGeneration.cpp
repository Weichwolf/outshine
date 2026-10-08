#include "content/AssetGeneration.h"
#include <algorithm>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace outshine {
namespace {

const char *Describe(AssetCacheError error) {
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

std::expected<std::optional<CachedAsset>, AssetCacheError>
Load(const AssetCache &cache, std::string_view identity, size_t packageBytesMost, Address address) {
  if (address == Address::Product) { return cache.Load(identity, packageBytesMost); }
  auto found = cache.FindRequest(identity);
  if (!found) { return std::unexpected(found.error()); }
  if (!*found) { return std::optional<CachedAsset>{}; }
  return cache.Load((**found).Key, packageBytesMost);
}

std::string_view Identity(const AssetRecord &record, Address address) {
  if (address == Address::Product) { return record.Key; }
  return record.RequestKey ? std::string_view(*record.RequestKey) : std::string_view{};
}

std::expected<CachedAsset, std::string> Resolve(AssetCache &cache,
                                                std::string_view key,
                                                size_t packageBytesMost,
                                                const AssetFactory &factory,
                                                Address address) {
  auto loaded = Load(cache, key, packageBytesMost, address);
  if (!loaded) { return std::unexpected(Describe(loaded.error())); }
  if (*loaded) { return std::move(**loaded); }
  if (!factory) { return std::unexpected("asset miss has no registered generator"); }
  auto generated = factory(packageBytesMost);
  if (!generated) { return std::unexpected(std::move(generated.error())); }
  if (generated->Bytes.size() > packageBytesMost) {
    return std::unexpected(Describe(AssetCacheError::CapacityExceeded));
  }
  if (std::ranges::count_if(generated->Records, [key, address](const AssetRecord &record) {
        return Identity(record, address) == key;
      }) != 1) {
    return std::unexpected("generator omitted the requested asset");
  }
  const auto published = cache.Publish(generated->Records, generated->Bytes);
  if (!published) { return std::unexpected(Describe(published.error())); }
  loaded = Load(cache, key, packageBytesMost, address);
  if (!loaded) { return std::unexpected(Describe(loaded.error())); }
  if (!*loaded) { return std::unexpected("published asset package is unavailable"); }
  return std::move(**loaded);
}
}

std::expected<CachedAsset, std::string> ResolveAsset(AssetCache &cache,
                                                     std::string_view key,
                                                     size_t packageBytesMost,
                                                     const AssetFactory &factory) {
  return Resolve(cache, key, packageBytesMost, factory, Address::Product);
}

std::expected<CachedAsset, std::string> ResolveAssetRequest(AssetCache &cache,
                                                            std::string_view requestKey,
                                                            size_t packageBytesMost,
                                                            const AssetFactory &factory) {
  return Resolve(cache, requestKey, packageBytesMost, factory, Address::Request);
}

}
