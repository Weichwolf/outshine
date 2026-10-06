#ifndef OUTSHINE_CONTENT_ASSETS_ASSETGENERATION_H
#define OUTSHINE_CONTENT_ASSETS_ASSETGENERATION_H

#include "AssetCache.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace outshine {

struct GeneratedAssetPackage {
  std::vector<AssetRecord> Records;
  std::vector<uint8_t> Bytes;
};

using AssetFactory = std::function<std::expected<GeneratedAssetPackage, std::string>(size_t)>;

[[nodiscard]] std::expected<CachedAsset, std::string> ResolveAsset(AssetCache &cache,
                                                                   std::string_view key,
                                                                   size_t packageBytesMost,
                                                                   const AssetFactory &factory);

}
#endif
