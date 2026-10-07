#include "content/AssetGeneration.h"
#include <algorithm>
#include <cstddef>
#include <expected>
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

}

std::expected<CachedAsset, std::string> ResolveAsset(AssetCache &cache,
                                                     std::string_view key,
                                                     size_t packageBytesMost,
                                                     const AssetFactory &factory) {
  auto loaded = cache.Load(key, packageBytesMost);
  if (!loaded) { return std::unexpected(Describe(loaded.error())); }
  if (*loaded) { return std::move(**loaded); }
  if (!factory) { return std::unexpected("asset miss has no registered generator"); }
  auto generated = factory(packageBytesMost);
  if (!generated) { return std::unexpected(std::move(generated.error())); }
  if (generated->Bytes.size() > packageBytesMost) {
    return std::unexpected(Describe(AssetCacheError::CapacityExceeded));
  }
  if (std::ranges::none_of(generated->Records,
                           [key](const AssetRecord &record) { return record.Key == key; })) {
    return std::unexpected("generator omitted the requested asset");
  }
  const auto published = cache.Publish(generated->Records, generated->Bytes);
  if (!published) { return std::unexpected(Describe(published.error())); }
  loaded = cache.Load(key, packageBytesMost);
  if (!loaded) { return std::unexpected(Describe(loaded.error())); }
  if (!*loaded) { return std::unexpected("published asset package is unavailable"); }
  return std::move(**loaded);
}

}
