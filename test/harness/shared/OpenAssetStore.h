#ifndef OUTSHINE_TEST_OPENASSETSTORE_H
#define OUTSHINE_TEST_OPENASSETSTORE_H

#include "AssetCache.h"
#include <filesystem>
#include <memory>
#include <utility>

namespace outshine::Test {
[[nodiscard]] inline std::shared_ptr<AssetCache>
OpenAssetStore(const std::filesystem::path &directory, AssetCache::Runtime runtime = {}) {
  auto opened = AssetCache::Open((directory / "assets.sqlite").string(), std::move(runtime));
  return opened ? std::shared_ptr<AssetCache>(std::move(*opened)) : nullptr;
}
}
#endif
