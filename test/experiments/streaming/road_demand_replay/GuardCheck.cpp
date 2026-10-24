#include "ConsumerHooks.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace {
bool Check(bool condition, std::string_view stage) {
  if (!condition) { std::println(stderr, "guard check failed: {}", stage); }
  return condition;
}
}

int main(int argc, char *const *argv) {
  if (argc != 2) { return 2; }
  const std::filesystem::path directory(argv[1]);
  std::error_code error;
  std::filesystem::create_directories(directory / "pages", error);
  if (error) { return 1; }
  const auto path = directory / "guard-assets.sqlite";
  if (std::filesystem::exists(path)) { return 1; }
  auto cache = outshine::AssetCache::Open(path.string());
  auto pages = outshine::Generators::PreparedRoadPages::Open((directory / "pages").string());
  if (!Check(cache && pages, "native caches open")) { return 1; }
  constexpr std::array<uint8_t, 4> bytes{1, 3, 5, 7};
  const std::array records{outshine::AssetRecord{.Key = std::string(64, 'a'),
                                                 .Kind = "road-render-page",
                                                 .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                                                 .Package = {},
                                                 .OffsetBytes = 0,
                                                 .ByteCount = bytes.size(),
                                                 .Level = 0,
                                                 .Parent = {},
                                                 .RequestKey = {}}};
  if (!Check((*cache)->Publish(records, bytes).has_value(), "valid native packet publishes")) {
    return 1;
  }
  const auto actual = RealCacheLoad(cache->get(), records.front().Key, 1024);
  if (!Check(actual && *actual && std::ranges::equal((**actual).Bytes(), bytes),
             "unwrapped cached packet is readable")) {
    return 1;
  }
  const auto excludedAsset = (*cache)->Load(records.front().Key, 1024);
  if (!Check(!excludedAsset && excludedAsset.error() == outshine::AssetCacheError::InvalidInput,
             "wrapped cached packet is rejected")) {
    return 1;
  }
  const std::string absent(64, 'b');
  const auto missing = RealLoad(pages->get(), absent, 1024);
  if (!Check(missing && !*missing, "unwrapped missing page is an ordinary miss")) { return 1; }
  const auto excludedPage = (*pages)->LoadKey(absent, 1024);
  if (!Check(!excludedPage, "wrapped page lookup is rejected")) { return 1; }
  size_t factoryCalls = 0;
  const outshine::Generators::PreparedRoadPages::Factory factory =
      [&factoryCalls](
          size_t) -> std::expected<outshine::Generators::RoadPageGeometry, std::string> {
    ++factoryCalls;
    return std::unexpected("guard check factory must remain idle");
  };
  const outshine::Generators::RoadPageAddress address{};
  const auto resolved =
      (*pages)->Resolve(address, std::string(64, 'c'), std::string(64, 'd'), 1024, factory);
  if (!Check(!resolved && factoryCalls == 0, "resolution is intercepted before the factory")) {
    return 1;
  }
  std::println("3 intercepted guards; 2 native positive controls; 0 factory calls");
  return 0;
}
