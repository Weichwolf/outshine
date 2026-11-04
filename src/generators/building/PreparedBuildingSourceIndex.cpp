#include "PreparedBuildingAssets.h"
#include "PageIndexAsset.h"
#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators {
std::expected<std::shared_ptr<const PageIndexAsset>, StructureBakeError>
PreparedBuildingAssets::ResolveSourceIndex(const std::string &key, size_t bytesMost) {
  return SourceIndex(key, bytesMost, {});
}

std::expected<std::shared_ptr<const PageIndexAsset>, StructureBakeError>
PreparedBuildingAssets::SourceIndex(const std::string &key, size_t bytesMost, Base base) {
  auto stored = LoadPageIndex(*Cache_, key, bytesMost);
  if (!stored) { return std::unexpected(CacheFailure(stored.error())); }
  if (*stored) { return std::make_shared<const PageIndexAsset>(std::move(**stored)); }
  if (!base) {
    auto loaded = Load(key);
    if (!loaded) { return std::unexpected(loaded.error()); }
    base = std::move(*loaded);
  }
  if (!base) { return std::shared_ptr<const PageIndexAsset>{}; }
  const auto bytes = PageIndexAsset::EncodedBytes(base->Structures.size());
  if (!bytes || *bytes > bytesMost) {
    return std::unexpected(CacheFailure(AssetCacheError::CapacityExceeded));
  }
  std::vector<Box> bounds;
  bounds.reserve(base->Structures.size());
  for (size_t at = 0; at < base->Structures.size(); ++at) {
    bounds.push_back(StructureBounds(*base, at));
  }
  auto index = ResolvePageIndex(*Cache_, key, bounds, bytesMost);
  if (!index) { return std::unexpected(CacheFailure(index.error())); }
  return std::make_shared<const PageIndexAsset>(std::move(*index));
}
}
