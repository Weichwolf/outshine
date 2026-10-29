#ifndef OUTSHINE_TEST_PUBLISHTERRAINDEFORMATION_H
#define OUTSHINE_TEST_PUBLISHTERRAINDEFORMATION_H
#include "PreparedTerrainAssets.h"
#include <expected>
#include <string>
#include <utility>

namespace outshine::Test {
[[nodiscard]] inline std::expected<void, Generators::PreparedTerrainAssets::Failure>
PublishTerrainDeformation(Generators::PreparedTerrainAssets &cache,
                          const std::string &key,
                          Generators::PreparedTerrainDeformation product) {
  auto prepared = Generators::PreparedTerrainAssets::PrepareDeformation(key, std::move(product));
  if (!prepared) { return std::unexpected(std::move(prepared.error())); }
  for (;;) {
    const auto stored = cache.StoreDeformation(*prepared);
    if (!stored) { return std::unexpected(stored.error()); }
    if (*stored) { return {}; }
  }
}
}
#endif
