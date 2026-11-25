#include "AssetCache.h"
#include "PreparedBuildingAssets.h"
#include "PreparedTerraceAsset.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "Tasks.h"
#include "UrbanPrismPreparation.h"
#include <charconv>
#include <chrono>
#include <cstddef>
#include <expected>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace {
using namespace outshine;
namespace Osm = Generators::Osm;
using Clock = std::chrono::steady_clock;

std::expected<void, std::string> Resolve(const std::shared_ptr<AssetCache> &cache,
                                         std::shared_ptr<Generators::PreparedBuildingAssets> source,
                                         const Osm::TerraceAssetRequest &request,
                                         const char *path) {
  const auto key = request.Key();
  if (!key) { return std::unexpected(key.error()); }
  Tasks pool(1);
  Osm::UrbanPrismPreparation job(pool, cache, *key);
  constexpr size_t sampleBudget = 8 * (size_t{1} << 20);
  const auto capture =
      [&](const std::stop_token &stop) -> std::optional<Osm::UrbanPrismPreparation::Captured> {
    return Osm::CapturePreparedTerraces(
        source, request, {.SamplesMost = sampleBudget, .PointTestsMost = 4 * sampleBudget}, stop);
  };
  const auto deadline = Clock::now() + std::chrono::seconds(30);
  while (Clock::now() < deadline) {
    const auto result = job.Advance(capture);
    if (!result) { return std::unexpected(result.error()); }
    if (*result) {
      const auto payload = cache->LoadRequest(*key, Osm::kUrbanPrismBytesMost);
      if (!payload || !*payload || (**payload).Record().Parent != request.BaseKey) {
        return std::unexpected("published terrace asset has no matching source identity");
      }
      const auto bytes = (**payload).Bytes();
      std::ofstream output(path, std::ios::binary);
      output.write(reinterpret_cast<const char *>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
      if (!output) { return std::unexpected("could not export cached terrace payload"); }
      const auto &cost = job.Measurements();
      std::cout << "patches " << (*result)->Placements.size() << " bytes " << bytes.size()
                << " generations " << cost.Generations << " build " << cost.BuildWorkerMs
                << " ms lookup " << cost.ReadWorkerMs << " ms\n";
      return {};
    }
    (void)job.AwaitProgress(.05);
  }
  return std::unexpected("terrace demand did not complete before the replay deadline");
}
}

int main(int argc, const char *const *argv) {
  if (argc != 6 && argc != 7) {
    std::cerr
        << "usage: prepared_terraces BASE_DB|'-' OUTPUT_DB BASE_KEY SPACING OUTPUT [CELLS_HEX]\n";
    return 2;
  }
  const std::string_view spacing = argv[4];
  Osm::TerraceAssetRequest request{.BaseKey = argv[3]};
  const auto parsed =
      std::from_chars(spacing.data(), spacing.data() + spacing.size(), request.SpacingM);
  if (parsed.ec != std::errc{} || parsed.ptr != spacing.data() + spacing.size()) { return 2; }
  if (argc == 7) {
    const std::string_view cells = argv[6];
    const auto mask = std::from_chars(cells.begin(), cells.end(), request.Cells, 16);
    if (mask.ec != std::errc{} || mask.ptr != cells.end()) { return 2; }
  }
  auto output = AssetCache::Open(argv[2]);
  if (!output) { return 3; }
  std::shared_ptr<Generators::PreparedBuildingAssets> source;
  if (std::string_view(argv[1]) != "-") {
    auto cache = AssetCache::Open(argv[1]);
    if (!cache) { return 3; }
    Data::RawSourceCache raw({.Directory = std::string(argv[2]) + "-sources"});
    const Data::SourceSet sources(raw);
    auto inputs = Generators::PreparedBuildingAssets::Open(
        std::shared_ptr<AssetCache>(std::move(*cache)), sources);
    if (!inputs) { return 3; }
    source = std::move(*inputs);
  }
  const auto result =
      Resolve(std::shared_ptr<AssetCache>(std::move(*output)), std::move(source), request, argv[5]);
  if (!result) {
    std::cerr << result.error() << '\n';
    return 1;
  }
}
