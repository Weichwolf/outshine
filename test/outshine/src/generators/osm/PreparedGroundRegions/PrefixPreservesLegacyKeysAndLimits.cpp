#include "PreparedGroundRegions.h"
#include "AssetSourceRecipe.h"
#include "BinaryValueArchive.h"
#include "Sha256.h"
#include "OsmField.h"
#include "TilePool.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "Check.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <string_view>
#include <cstdint>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators::Osm;
constexpr size_t kLegacyLimit = size_t{4} * 1024u * 1024u;

std::vector<uint8_t> LegacyMessage(std::string_view recipe,
                                   const Ground::ShapedGround &shape,
                                   int zoom,
                                   MvtSchema schema,
                                   std::span<const uint8_t> parameters,
                                   std::span<const Ground::TileSpot> tiles) {
  BinaryValueWriter out(kLegacyLimit);
  if (!out.Text("native-ground-region-demand-2") || !out.Text(recipe) || !out.Text(shape.Kind) ||
      !out(shape.AmplitudeM,
           shape.WavelengthM,
           shape.Gradient,
           shape.BearingDeg,
           shape.FocusLatDeg,
           shape.FocusLonDeg,
           shape.Seed) ||
      !out(zoom, schema, static_cast<uint64_t>(tiles.size()))) {
    return {};
  }
  for (const auto &tile : tiles) {
    if (!out(tile.Zoom, tile.X, tile.Y)) { return {}; }
  }
  if (!out.Array(parameters)) { return {}; }
  return std::move(out.Out).TakeBytes();
}

void CheckKeys(const PreparedGroundRegions &cache, std::string_view recipe) {
  using namespace outshine::Test;
  const std::array<Ground::TileSpot, 2> tiles{
      {{.Zoom = 4, .X = 7, .Y = 5}, {.Zoom = 4, .X = 8, .Y = 5}}};
  std::array<uint8_t, 13> parameters{};
  for (const double amplitude : {0.0, -0.0, 17.25}) {
    Ground::ShapedGround shape;
    shape.AmplitudeM = amplitude;
    for (const auto schema : {MvtSchema::Shortbread, MvtSchema::OpenMapTiles}) {
      for (uint8_t variant = 0; variant < 4; ++variant) {
        parameters.back() = variant;
        const auto message = LegacyMessage(recipe, shape, 4, schema, parameters, tiles);
        CHECK(!message.empty() && cache.RequestKey(4, schema, shape, parameters, tiles) ==
                                      Sha256Hex(message.data(), message.size()),
              "immutable prefix preserves the entire previous key serialization");
      }
    }
  }
  Ground::ShapedGround shape;
  const auto empty = LegacyMessage(recipe, shape, 4, MvtSchema::Shortbread, {}, tiles);
  std::vector<uint8_t> largest(kLegacyLimit - empty.size(), 7);
  const auto message = LegacyMessage(recipe, shape, 4, MvtSchema::Shortbread, largest, tiles);
  CHECK(message.size() == kLegacyLimit &&
            cache.RequestKey(4, MvtSchema::Shortbread, shape, largest, tiles) ==
                Sha256Hex(message.data(), message.size()),
        "the exact previous four-MiB encoding boundary remains accepted");
  largest.push_back(7);
  CHECK(cache.RequestKey(4, MvtSchema::Shortbread, shape, largest, tiles).empty() &&
            LegacyMessage(recipe, shape, 4, MvtSchema::Shortbread, largest, tiles).empty(),
        "prefix bytes still count against the existing encoding limit");
  const auto expected = cache.RequestKey(4, MvtSchema::Shortbread, shape, parameters, tiles);
  std::array<std::string, 4> concurrent{};
  {
    std::vector<std::jthread> workers;
    workers.reserve(concurrent.size());
    for (auto &key : concurrent) {
      workers.emplace_back([&, output = &key] {
        *output = cache.RequestKey(4, MvtSchema::Shortbread, shape, parameters, tiles);
      });
    }
  }
  for (const auto &key : concurrent) {
    CHECK(key == expected, "parallel callers finalize independent copies of the immutable prefix");
  }
}
}

int main() {
  using namespace outshine::Test;
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-request-prefix-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::error_code error;
  std::filesystem::create_directories(root / "oversized", error);
  CHECK(!error, "isolated request-key cache directories are created");
  if (error) { return Report(); }
  Data::RawSourceCache store({.Directory = {}, .Using = Data::RawSourceCache::Use::Off});
  const Data::SourceSet sources(store);
  constexpr size_t kLargeRecipeBytes = size_t{160} * 1024;
  std::string rules(kLargeRecipeBytes, 'r');
  auto recipe = Generators::AssetSourceRecipe(
      "prepared-ground-region-96",
      sources,
      std::array{Data::DataKind::Elevation, Data::DataKind::VectorMap});
  recipe.append(rules);
  {
    const auto opened = PreparedGroundRegions::Open(root.string(), sources, rules);
    CHECK(opened, "the source-independent request service opens with a large recipe");
    if (opened) {
      rules.assign(rules.size(), 'x');
      CheckKeys(**opened, recipe);
    }
  }
  {
    const std::string oversized(kLegacyLimit, 'r');
    const auto opened =
        PreparedGroundRegions::Open((root / "oversized").string(), sources, oversized);
    const Ground::ShapedGround shape;
    CHECK(opened && (*opened)->RequestKey(4, MvtSchema::Shortbread, shape, {}, {}).empty(),
          "an oversized immutable prefix remains an invalid request");
  }
  std::filesystem::remove_all(root, error);
  CHECK(!error, "the isolated request-key cache is removed after closing it");
  return Report();
}
