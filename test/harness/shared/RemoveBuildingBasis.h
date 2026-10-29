#ifndef OUTSHINE_TEST_REMOVEBUILDINGBASIS_H
#define OUTSHINE_TEST_REMOVEBUILDINGBASIS_H
#include "AssetCache.h"
#include <algorithm>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>

namespace outshine::Test {
[[nodiscard]] inline std::expected<void, AssetCacheError>
RemoveBuildingBasis(const std::filesystem::path &directory, const std::string &baseKey) {
  auto cache = AssetCache::Open((directory / "assets.sqlite").string());
  if (!cache) { return std::unexpected(cache.error()); }
  constexpr double kFixtureExtent = 1e12;
  auto records =
      (*cache)->Select({.Bounds = {.Min = {{-kFixtureExtent, -kFixtureExtent, -kFixtureExtent}},
                                   .Max = {{kFixtureExtent, kFixtureExtent, kFixtureExtent}}},
                        .Kind = "building-basis",
                        .Level = std::nullopt,
                        .Radius = std::nullopt,
                        .Planes = {}});
  if (!records) { return std::unexpected(records.error()); }
  const auto found = std::ranges::find(*records, baseKey, &AssetRecord::Parent);
  if (found == records->end()) { return std::unexpected(AssetCacheError::InvalidInput); }
  return (*cache)->Remove(found->Key);
}
}
#endif
