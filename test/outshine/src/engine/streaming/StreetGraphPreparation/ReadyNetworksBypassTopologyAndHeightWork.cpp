#include "Check.h"
#include "GroundMaterials.h"
#include "OsmField.h"
#include "PreparedStreetGraph.h"
#include "Sha256.h"
#include "SourceSet.h"
#include "StreetGraphPreparation.h"
#include "VegetationTemplates.h"
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using Clock = std::chrono::steady_clock;

std::optional<StreetGraphPreparation::Completed> Await(StreetGraphPreparation &work) {
  const auto deadline = Clock::now() + std::chrono::seconds(5);
  while (!work.Complete() && Clock::now() < deadline) { std::this_thread::yield(); }
  auto completed = work.Collect();
  CHECK(completed && *completed, "native graph preparation reaches successful terminal state");
  if (!completed || !*completed) { return std::nullopt; }
  return std::move(completed->value());
}

StreetGraphPreparation::Resolver
Resolve(std::shared_ptr<Generators::Osm::PreparedStreetGraph> cache, std::string key) {
  return [cache = std::move(cache), key = std::move(key)](const auto &factory) {
    return cache->Resolve(key, factory);
  };
}

std::shared_ptr<Generators::Osm::OsmField> Input(bool bridge) {
  const std::array<std::string, 1> layers{"streets"};
  auto field = std::make_shared<Generators::Osm::OsmField>(14, layers);
  const std::array<Generators::Osm::OsmField::Declared, 2> roads{
      {{.Layer = "streets",
        .Key = "kind",
        .Value = "path",
        .Bridge = bridge,
        .LatLon = {47, 9, 47, 9.002}},
       {.Layer = "streets",
        .Key = "kind",
        .Value = "path",
        .LatLon = {46.999, 9.001, 47.001, 9.001}}}};
  CHECK(field->Declare(roads, {.LongitudeDeg = 9, .LatitudeDeg = 47}).has_value(),
        "a crossing street fixture is declared");
  return field;
}
}

int main() {
  using namespace outshine::Test;
  const auto root =
      std::filesystem::temp_directory_path() /
      ("outshine-native-network-" + std::to_string(Clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root / "assets");
  Data::ContentStore store({.Directory = (root / "sources").string()});
  Data::SourceSet sources(store);
  Ground::GroundMaterials materials;
  Ground::VegetationTemplates templates;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            templates.Load("src/assets/world/vegetation.json", materials),
        "native street rules load");
  auto vectors = Input(false);
  auto ways = std::make_shared<Generators::Osm::StreetField>();
  CHECK(ways->Ingest(*vectors, templates) == 2, "both roads are normalized");
  auto opened = Generators::Osm::PreparedStreetGraph::Open((root / "assets").string(), sources);
  CHECK(opened.has_value(), "street graphs use the common asset database");
  if (!opened) { return Report(); }
  auto cache = std::move(*opened);
  const auto key = cache->Key(*vectors, *ways, {}, 14);
  CHECK(key.has_value(), "complete street input has a persistent identity");
  if (!key) { return Report(); }
  Tasks pool(1);
  size_t factories = 0, heightSamples = 0;
  const StreetGraphPreparation::JobFactory begin = [&] {
    ++factories;
    return Generators::Osm::StreetGraphBuildJob::Begin(*vectors, *ways, [&](LongitudeLatitude at) {
      ++heightSamples;
      return std::optional<double>(100 + at.LatitudeDeg);
    });
  };
  std::optional<StreetGraphPreparation::Completed> cold;
  {
    StreetGraphPreparation work(pool, begin, Resolve(cache, *key));
    cold = Await(work);
    CHECK(cold && !cold->CacheHit && cold->ReadBytes > 0 && work.Advances() > 0 && factories == 1 &&
              heightSamples > 0,
          "cold generation writes and reloads a native network exactly once");
  }
  if (!cold || !cold->Graph.Graph) { return Report(); }
  cache.reset();
  opened = Generators::Osm::PreparedStreetGraph::Open((root / "assets").string(), sources);
  CHECK(opened.has_value(), "a fresh owner reopens the native network");
  if (!opened) { return Report(); }
  cache = std::move(*opened);
  {
    StreetGraphPreparation work(pool, StreetGraphPreparation::JobFactory{}, Resolve(cache, *key));
    auto warm = Await(work);
    CHECK(warm && warm->CacheHit && warm->ReadBytes == cold->ReadBytes && work.Advances() == 0 &&
              factories == 1,
          "fresh warm loading invokes neither street layout, weave nor elevation");
    if (warm) {
      CHECK(warm->Graph.Graph->EncodeAsset(1024 * 1024) ==
                    cold->Graph.Graph->EncodeAsset(1024 * 1024) &&
                warm->Graph.Elevated.Points == cold->Graph.Elevated.Points &&
                warm->Graph.Elevated.SteepestGrade == cold->Graph.Elevated.SteepestGrade &&
                warm->Graph.WeaveMs == 0 && warm->Graph.ElevateMs == 0,
            "native geometry, topology and profiles survive without replaying old timings");
    }
  }
  auto bridge = Input(true);
  Generators::Osm::StreetField bridgeWays;
  CHECK(bridgeWays.Ingest(*bridge, templates) == 2, "bridge input is normalized");
  CHECK(cache->Key(*bridge, bridgeWays, {}, 14) != key,
        "bridge classification invalidates the asset even with unchanged centreline points");
  Ground::ShapedGround changed;
  changed.Seed = 1;
  CHECK(cache->Key(*vectors, *ways, changed, 14) != key &&
            cache->Key(*vectors, *ways, {}, 13) != key,
        "terrain shape and source detail are persistent inputs");
  auto raw = AssetCache::Open((root / "assets" / "assets.sqlite").string());
  CHECK(raw.has_value(), "the generic cache can inspect native publication");
  if (!raw) { return Report(); }
  const std::array<uint8_t, 3> corrupt{1, 2, 3};
  const AssetRecord record{.Key = *key,
                           .Kind = "street-network",
                           .Bounds = cold->Graph.Graph->BoundsEcef(),
                           .Package = {},
                           .ByteCount = corrupt.size(),
                           .Parent = {}};
  CHECK((*raw)->Publish({&record, 1}, corrupt).has_value(),
        "a checksum-valid unsupported native format is installed");
  {
    StreetGraphPreparation work(pool, begin, Resolve(cache, *key));
    auto repaired = Await(work);
    CHECK(repaired && !repaired->CacheHit && factories == 2,
          "native format corruption causes regeneration rather than publication of bad topology");
  }
  const auto incompleteKey = Sha256Hex("incomplete-height-fixture");
  {
    StreetGraphPreparation work(
        pool,
        [&] {
          return Generators::Osm::StreetGraphBuildJob::Begin(
              *vectors, *ways, [](LongitudeLatitude) { return std::optional<double>{}; });
        },
        Resolve(cache, incompleteKey));
    auto incomplete = Await(work);
    const auto persisted = (*raw)->Find(incompleteKey);
    CHECK(incomplete && !incomplete->CacheHit && incomplete->ReadBytes == 0 &&
              incomplete->Graph.Elevated.Refused > 0 && persisted && !*persisted,
          "missing heights retain their existing runtime diagnosis and never become ready assets");
  }
  raw->reset();
  cache.reset();
  std::filesystem::remove_all(root);
  return Report();
}
