#ifndef OUTSHINE_TEST_STOREGROUNDPATCH_H
#define OUTSHINE_TEST_STOREGROUNDPATCH_H
#include "PreparedTerrainAssets.h"
#include <expected>
#include <string>
#include <utility>

namespace outshine::Test {
inline std::expected<void, Generators::PreparedTerrainAssets::Failure>
StoreGroundPatch(Generators::PreparedTerrainAssets &cache,
                 const std::string &key,
                 Data::TileId at,
                 const Generators::GroundPatch &patch) {
  auto write = cache.PreparePatch(key, at, patch);
  if (!write) { return std::unexpected(std::move(write.error())); }
  auto stored = cache.StorePatch(*write);
  if (!stored) { return std::unexpected(std::move(stored.error())); }
  return {};
}
}
#endif
