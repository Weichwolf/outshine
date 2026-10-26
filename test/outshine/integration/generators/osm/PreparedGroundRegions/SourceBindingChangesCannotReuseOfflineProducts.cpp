#include "Check.h"
#include "GroundRegionAsset.h"
#include "OsmField.h"
#include "PreparedGroundRegions.h"
#include "Sha256.h"
#include "SourceMetadataFixture.h"
#include "TilePool.h"
#include "WaterAsset.h"
#include "Wayfinding.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <spawn.h>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>
#ifdef __APPLE__
#include <crt_externs.h>
#endif

namespace {
using namespace outshine;
using Generators::Osm::MvtSchema;
using Generators::Osm::PreparedGroundRegions;
namespace Fixture = Test::SourceMetadata;
constexpr auto kLegacyRequest = "c2e51b9d79875b1bbe3c2bfa418eadd4d70224b20cf180b0fe2e41bea495383c";
constexpr std::array<float, 4> kTerrainHeightsM{100, 101, 102, 103};
constexpr double kNetworkSpanDeg = 0.001;
constexpr uint64_t kNetworkSourceTag = 42;

std::string Request(const PreparedGroundRegions &cache) {
  const std::array<Ground::TileSpot, 1> tiles{{{.Zoom = 2, .X = 1, .Y = 1}}};
  const std::array<uint8_t, 3> parameters{1, 2, 3};
  return cache.RequestKey(2, MvtSchema::Shortbread, {}, parameters, tiles);
}

std::shared_ptr<const Path::Network> SourceNetwork() {
  auto graph = Path::Network::Create({.CellM = 1}, {});
  const std::array<double, 4> points{0, 0, 0, kNetworkSpanDeg};
  if (!graph || !graph->Lay(points, {.HalfWidthM = 1, .Oneway = true, .Tag = kNetworkSourceTag})) {
    return {};
  }
  std::string error;
  if (!graph->Weave(error) || graph->Elevate([](LongitudeLatitude) { return 0.0; }).Refused != 0) {
    return {};
  }
  return std::make_shared<const Path::Network>(std::move(*graph));
}

bool Prime(const std::filesystem::path &root) {
  Data::RawSourceCache raw({.Directory = {}, .Using = Data::RawSourceCache::Use::Off});
  Data::SourceSet sources(raw);
  Fixture::RegisterRecipe(sources, false);
  const auto opened = PreparedGroundRegions::Open(root.string(), sources, "fixture-rules");
  CHECK(opened, "the native producer opens its test-owned asset directory");
  if (!opened) { return false; }
  GroundRegionAsset region{};
  region.Network = SourceNetwork();
  CHECK(region.Network, "the region owns a complete source-bearing directed native network");
  if (!region.Network) { return false; }
  region.NetworkWays = region.Network->WayCount();
  region.Anchor = {.LongitudeDeg = 0, .LatitudeDeg = 0};
  const auto material = region.Surfaces.addSurface("ground", Material{});
  CHECK(material, "the complete native fixture owns its ground material");
  if (!material) { return false; }
  region.GroundSurface = material->index();
  const auto part = region.Surfaces.addPart("pavement", *material);
  const std::array<float, 9> positions{0, 100, 0, 10, 101, 0, 0, 102, 10};
  const std::array<uint32_t, 3> indices{0, 1, 2};
  CHECK(part && region.Surfaces.setPositions(*part, positions) &&
            region.Surfaces.setTriangles(*part, indices),
        "the cached product contains codec-valid native geometry");
  region.Terrain.Sheets = {{.Tile = {.Zoom = 2, .X = 1, .Y = 1},
                            .Nodes = {kTerrainHeightsM.begin(), kTerrainHeightsM.end()},
                            .Side = 2,
                            .Postings = 2}};
  region.Terrain.Tiles = 1;
  const auto key = Sha256Hex("source-framing-native-fixture-product");
  const auto stored = (*opened)->Store(
      key, GroundRegionBoundsEcef(region), region, Generators::WaterAsset{}, Request(**opened));
  CHECK(stored, "the complete native product is committed with its current source binding");
  if (!stored) { return false; }
  const auto bound = (*opened)->BindRequest(key, kLegacyRequest);
  CHECK(bound, "the actual pre-framing alias is retained for the migration counterexample");
  return bound.has_value();
}

int Offline(const std::filesystem::path &root, bool pair) {
  Data::RawSourceCache raw({.Directory = {}, .Using = Data::RawSourceCache::Use::Off});
  Data::SourceSet sources(raw);
  Fixture::RegisterRecipe(sources, pair);
  const auto opened = PreparedGroundRegions::Open(root.string(), sources, "fixture-rules");
  CHECK(opened, "the fresh process opens metadata and assets with no source data available");
  if (!opened) { return Test::Report(); }
  const auto request = Request(**opened);
  const auto loaded = (*opened)->LoadRequest(request);
  CHECK(loaded, "the real early request lookup completes without a storage error");
  const bool hit = loaded && loaded->has_value();
  if (pair) {
    CHECK(!hit && request != kLegacyRequest,
          "the different source binding cannot inherit the current or old ambiguous product");
  } else {
    CHECK(loaded && loaded->has_value(), "the same binding remains an offline hit");
    if (loaded && loaded->has_value()) {
      const auto &region = (**loaded).Region;
      CHECK(region.Terrain.Sheets.size() == 1 &&
                region.Terrain.Sheets.front().Nodes ==
                    std::vector<float>(kTerrainHeightsM.begin(), kTerrainHeightsM.end()),
            "the same binding restores the exact native product in a fresh offline process");
      CHECK(region.Network && region.NetworkWays == 1 &&
                region.Network->TagOf(0) == kNetworkSourceTag &&
                region.Network->ConnectionsFrom(0).size() == 1 &&
                region.Network->ConnectionsFrom(0).front().Source.Way == 0 &&
                region.Network->ConnectionsFrom(0).front().Source.Step() == 1 &&
                !region.Network->ConnectionsFrom(0).front().Source.Reverse(),
            "the early region hit retains source way, segment and direction without rebuilding");
    }
  }
  CHECK(raw.Counters().ReadCalls == 0 && sources.Counters().ProviderStarts == 0,
        "lookup uses neither raw source reads nor provider work");
  std::cout << "SOURCE_REGION_OFFLINE changed_binding=" << pair << " hit=" << hit
            << " source_reads=" << raw.Counters().ReadCalls
            << " provider_calls=" << sources.Counters().ProviderStarts << '\n';
  return Test::Report();
}

bool Spawn(const std::string &executable, std::string mode, const std::filesystem::path &root) {
  std::array<std::string, 3> values{executable, std::move(mode), root.string()};
  std::array<char *, 4> arguments{values[0].data(), values[1].data(), values[2].data(), nullptr};
#ifdef __APPLE__
  char *const *const environment = *_NSGetEnviron();
#else
  char *const *const environment = environ;
#endif
  pid_t child = 0;
  if (posix_spawn(&child, executable.c_str(), nullptr, nullptr, arguments.data(), environment) !=
      0) {
    return false;
  }
  int status = 0;
  return waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
}

int main(int argc, char *const *argv) {
  if (argc == 3) {
    const std::string_view mode(argv[1]);
    if (mode == "--offline-same" || mode == "--offline-pair") {
      return Offline(argv[2], mode == "--offline-pair");
    }
    return 2;
  }
  using namespace outshine::Test;
  const auto root = std::filesystem::path(PlantedPath("source-recipe-offline"));
  std::error_code error;
  std::filesystem::create_directories(root, error);
  CHECK(!error, "the producer owns an isolated asset directory");
  const bool primed = !error && Prime(root);
  CHECK(primed, "the native package positive control completes before offline lookup");
  if (primed) {
    const auto executable = std::filesystem::absolute(argv[0]).string();
    CHECK(Spawn(executable, "--offline-same", root),
          "the original source binding remains a real fresh-process hit");
    CHECK(Spawn(executable, "--offline-pair", root),
          "the changed source binding cannot load the first process's product");
  }
  std::filesystem::remove_all(root, error);
  CHECK(!error, "all native test packages are removed after the separate processes exit");
  return Report();
}
