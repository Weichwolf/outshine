#include "ConsumerHooks.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <print>
#include <span>
#include <string>
#include <system_error>

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
  if (!cache || !pages) { return 1; }
  constexpr std::array<uint8_t, 4> bytes{1, 3, 5, 7};
  const std::array records{outshine::AssetRecord{.Key = "guard-payload",
                                                 .Kind = "road-render-page",
                                                 .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                                                 .Package = {},
                                                 .OffsetBytes = 0,
                                                 .ByteCount = bytes.size(),
                                                 .Level = 0,
                                                 .Parent = {},
                                                 .RequestKey = {}}};
  if (!(*cache)->Publish(records, bytes)) { return 1; }
  const auto actual = RealCacheLoad(cache->get(), records.front().Key, 1024);
  if (!actual || !*actual || !std::ranges::equal((**actual).Bytes(), bytes)) { return 1; }
  const auto excludedAsset = (*cache)->Load(records.front().Key, 1024);
  if (excludedAsset || excludedAsset.error() != outshine::AssetCacheError::InvalidInput) {
    return 1;
  }
  const auto missing = RealLoad(pages->get(), "excluded", 1024);
  if (!missing || *missing) { return 1; }
  const auto excludedPage = (*pages)->LoadKey("excluded", 1024);
  if (excludedPage) { return 1; }
  size_t factoryCalls = 0;
  const outshine::Generators::PreparedRoadPages::Factory factory =
      [&factoryCalls](
          size_t) -> std::expected<outshine::Generators::RoadPageGeometry, std::string> {
    ++factoryCalls;
    return std::unexpected("guard check factory must remain idle");
  };
  const outshine::Generators::RoadPageAddress address;
  const auto resolved = (*pages)->Resolve(address, "parent", "request", 1024, factory);
  if (resolved || factoryCalls != 0) { return 1; }
  std::println("3 intercepted guards; 2 native positive controls; 0 factory calls");
  return 0;
}
