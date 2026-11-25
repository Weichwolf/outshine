#include "AssetCache.h"
#include "BuildingRoofAssets.h"
#include "PreparedBuildingAssets.h"
#include "RawSourceCache.h"
#include "SourceProductSelection.h"
#include "SourceSet.h"
#include "ViewPath.h"
#include <chrono>
#include <cstddef>
#include <iostream>
#include <memory>
#include <ratio>
#include <string>
#include <string_view>
#include <utility>

int main(int argc, const char *const *argv) {
  using namespace outshine;
  using namespace outshine::Generators;
  if (argc != 3) { return 2; }
  ViewPath path;
  Vec2 viewport;
  double focal = 0;
  double support = 0;
  if (!(std::cin >> path.Start[0] >> path.Start[1] >> path.Start[2] >> path.End[0] >> path.End[1] >>
        path.End[2] >> path.RadiusM >> viewport[0] >> viewport[1] >> focal >> support)) {
    return 3;
  }
  auto cache = AssetCache::Open(argv[2]);
  if (!cache) { return 4; }
  Data::RawSourceCache raw({.Directory = std::string(argv[2]) + "-sources"});
  const Data::SourceSet sources(raw);
  std::shared_ptr<PreparedBuildingAssets> inputs;
  if (std::string_view(argv[1]) != "-") {
    auto sourceCache = AssetCache::Open(argv[1]);
    if (!sourceCache) { return 5; }
    auto source =
        PreparedBuildingAssets::Open(std::shared_ptr<AssetCache>(std::move(*sourceCache)), sources);
    if (!source) { return 6; }
    inputs = std::move(*source);
  }
  const auto begin = std::chrono::steady_clock::now();
  size_t tiles = 0;
  size_t blocks = 0;
  size_t rows = 0;
  size_t selected = 0;
  std::string key;
  while (std::cin >> key) {
    auto built = BuildingRoofAssets::Resolve(**cache, inputs.get(), key);
    if (!built) {
      std::cerr << "roof tile unavailable: " << key << '\n';
      return 7;
    }
    ++tiles;
    const double limit = SourceProductSelection::ProjectedSupportLimitM(
        built->Bounds(), path, viewport, focal, support);
    const size_t prefix = built->Prefix(limit);
    for (size_t i = 0; i < built->Blocks().size(); ++i) { rows += built->Blocks()[i].Rows; }
    for (size_t at = 0; at < prefix; ++at) {
      const auto payload = built->LoadBlock(**cache, at);
      if (!payload || !*payload) { return 8; }
      selected += (**payload).size();
    }
    blocks += prefix;
  }
  const auto ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
  std::cout << "{\"tiles\":" << tiles << ",\"candidate_rows\":" << rows
            << ",\"selected_blocks\":" << blocks << ",\"selected_rows\":" << selected
            << ",\"elapsed_ms\":" << ms << "}\n";
  return std::cin.eof() ? 0 : 9;
}
