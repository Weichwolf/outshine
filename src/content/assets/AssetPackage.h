#ifndef OUTSHINE_CONTENT_ASSETS_ASSETPACKAGE_H
#define OUTSHINE_CONTENT_ASSETS_ASSETPACKAGE_H

#include "AssetRecord.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <string>
#include <vector>

namespace outshine {
struct AssetPackage {
  std::vector<AssetRecord> Records;
  std::vector<uint8_t> Bytes;
};

using AssetPackageFactory = std::function<std::expected<AssetPackage, std::string>(size_t)>;
}
#endif
