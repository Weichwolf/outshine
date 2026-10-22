#include "PreparedOsmTiles.h"
#include "Check.h"
#include "SourceSet.h"
#include "MvtWireFixture.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Data;
using namespace outshine::Generators::Osm;

class NoTransport final : public Transport {
public:
  FetchStart Begin(const std::string & /*url*/) override { return Ticket::None; }

  Wire Collect(Ticket /*ticket*/) override { return Wire::Never(); }

  void Cancel(Ticket /*ticket*/) override {}
};

class VectorSource final : public Source {
public:
  VectorSource(std::string revision, std::vector<uint8_t> bytes, std::atomic_uint &calls)
      : Decl_{}, Bytes_(std::move(bytes)), Calls_(calls) {
    Decl_.Id = "fixture-vector";
    Decl_.Revision = std::move(revision);
    Decl_.Kind = DataKind::VectorMap;
    Decl_.Wire = WireFormat::MapboxVectorTile;
    Decl_.Keeps = Cacheability::Never;
  }

  [[nodiscard]] const SourceDecl &Declaration() const noexcept override { return Decl_; }

  [[nodiscard]] Coverage Covers(const Fetch & /*request*/) const noexcept override {
    return Coverage::Inside;
  }

  [[nodiscard]] Address Serves(const Fetch &request) const noexcept override {
    return request.Where();
  }

  FetchStart Begin(const Address & /*address*/, Transport & /*transport*/) const override {
    return Ticket::None;
  }

  Fetched Collect(const Address & /*address*/,
                  Ticket /*ticket*/,
                  Transport & /*transport*/) const override {
    ++Calls_;
    return Fetched::Delivered(Bytes_);
  }

private:
  SourceDecl Decl_;
  std::vector<uint8_t> Bytes_;
  std::atomic_uint &Calls_;
};

std::vector<uint8_t> TileBytes() {
  using outshine::Test::Mvt::Append;
  using outshine::Test::Mvt::Bytes;
  constexpr uint8_t layerVersion = 0x78;
  constexpr uint8_t layerExtent = 0x28;
  constexpr uint8_t layerKey = 0x1a;
  constexpr uint8_t layerValue = 0x22;
  constexpr uint8_t featureType = 0x18;
  constexpr uint8_t featureTags = 0x12;
  constexpr uint8_t polygonGeometry = 0x22;
  constexpr uint8_t polygonLineTo = 26;
  constexpr uint8_t buildingHeight = 19;
  constexpr uint8_t side = 20;
  Bytes layer{0x0a, 8, 'b', 'u', 'i', 'l', 'd', 'i', 'n', 'g', layerVersion, 2, layerExtent, 64};
  Append(layer, layerKey, Bytes{'r', 'e', 'n', 'd', 'e', 'r', '_', 'h', 'e', 'i', 'g', 'h', 't'});
  Append(layer, layerValue, Bytes{layerExtent, buildingHeight});
  Bytes feature{0x08, 7, featureType, 3};
  Append(feature, featureTags, Bytes{0, 0});
  Append(feature,
         polygonGeometry,
         Bytes{9, 0, 0, polygonLineTo, side, 0, 0, side, buildingHeight, 0, 15});
  Append(layer, featureTags, feature);
  Bytes tile;
  Append(tile, layerKey, layer);
  return tile;
}

struct Snapshot {
  std::vector<double> Points;
  std::vector<std::string> Digests;
  std::vector<TileSourceIdentity> Sources;
};

Snapshot Replay(const std::filesystem::path &directory, bool warm, std::string revision = "r1") {
  using namespace outshine::Test;
  std::atomic_uint calls{0};
  ContentStore store({.Directory = {}, .Using = ContentStore::Use::Off});
  SourceSet sources(store);
  NoTransport transport;
  Tasks compute(1);
  CHECK(sources.Add(std::make_unique<VectorSource>(std::move(revision),
                                                   warm ? std::vector<uint8_t>{} : TileBytes(),
                                                   calls)) == SourceSet::Registration::Accepted,
        "isolated provider registration succeeds");
  Ground::TilePool pool({.Compute = &compute, .PreparedFields = {}}, sources, transport);
  auto opened = PreparedOsmTiles::Open(directory.string(), sources, compute);
  CHECK(opened, "native service opens the persistent spatial cache");
  if (!opened) { return {}; }
  const std::array<std::string, 2> layers{"buildings", "water_polygons"};
  OsmField field(2, layers, MvtSchema::OpenMapTiles, *opened);
  const LongitudeLatitude eye{.LongitudeDeg = -45, .LatitudeDeg = 40};
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  do {
    CHECK(field.Build(pool, eye, 1, 9), "bounded asynchronous tile work advances");
    (void)compute.AwaitCompletion(
        std::chrono::duration<double>(std::chrono::milliseconds(1)).count());
  } while (!field.SettledWithin(1) && std::chrono::steady_clock::now() < deadline);
  CHECK(field.SettledWithin(1) && field.Tiles().size() == 9 && field.Features().size() == 9,
        "all nine native tiles are published with complete coverage");
  const auto costs = (*opened)->Costs();
  CHECK(warm ? calls == 0 : calls > 0, "warm native products never ask the unavailable provider");
  CHECK(warm ? costs.Hits == 9 && costs.Writes == 0 && costs.Misses == 0
             : costs.Hits == 0 && costs.Writes == 9 && costs.Misses == 9,
        "each tile has one cold publication or one warm native lookup");
  CHECK(field.TotalBuildMetrics().ParseMs == 0,
        "source preparation stays on the worker rather than the admission path");
  CHECK(field.MissingLayers() == 9, "native assembly preserves genuine missing layers");
  Snapshot result{};
  result.Points.assign(field.Points().begin(), field.Points().end());
  for (const auto &tile : field.Tiles()) {
    result.Sources.push_back(tile.Source);
    result.Digests.push_back(tile.InputDigest);
    CHECK(tile.Source.SourceId == "fixture-vector" && tile.Source.Revision == "r1",
          "native tile identity keeps the declared provider revision");
  }
  for (const auto &feature : field.Features()) {
    CHECK(field.Num(feature, "height", 0) == 19 && feature.ProviderFeatureId == 7,
          "merged native features preserve normalized tags and provider IDs");
  }
  return result;
}

void AbandonedRequestsDoNotBlock(const std::filesystem::path &directory) {
  std::atomic_uint calls{0};
  ContentStore store({.Directory = {}, .Using = ContentStore::Use::Off});
  SourceSet sources(store);
  NoTransport transport;
  Tasks compute(1);
  CHECK(sources.Add(std::make_unique<VectorSource>("r1", TileBytes(), calls)) ==
            SourceSet::Registration::Accepted,
        "abandoned-demand provider registers");
  Ground::TilePool pool({.Compute = &compute, .PreparedFields = {}}, sources, transport);
  auto opened = PreparedOsmTiles::Open(directory.string(), sources, compute);
  CHECK(opened, "isolated demand service opens");
  if (!opened) { return; }
  const std::array<std::string, 1> layers{"buildings"};
  constexpr int abandonedCount = 160;
  constexpr auto pollWait = std::chrono::milliseconds(100);
  for (int x = 0; x < abandonedCount; ++x) {
    const OsmField abandoned(9, layers, MvtSchema::OpenMapTiles, *opened);
    CHECK((*opened)->Acquire(pool, {.X = x, .Y = 1}, abandoned),
          "each short-lived owner admits a bounded request");
    (void)compute.AwaitCompletion(std::chrono::duration<double>(pollWait).count());
  }
  const OsmField current(9, layers, MvtSchema::OpenMapTiles, *opened);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  bool ready = false;
  do {
    const auto product = (*opened)->Acquire(pool, {.X = 200, .Y = 1}, current);
    CHECK(product, "current demand continues after more than the pending-job limit");
    if (!product) { break; }
    ready = product->Status == PreparedOsmTiles::State::Ready;
    (void)compute.AwaitCompletion(
        std::chrono::duration<double>(std::chrono::milliseconds(1)).count());
  } while (!ready && std::chrono::steady_clock::now() < deadline);
  CHECK(ready && calls > 0, "abandoned owners release capacity without discarding live work");
}

}

int main() {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-native-osm-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  const auto cold = Replay(directory, false);
  const auto warm = Replay(directory, true);
  CHECK(!cold.Points.empty() && cold.Points == warm.Points && cold.Sources == warm.Sources &&
            cold.Digests == warm.Digests,
        "fresh native replay preserves exact coordinates, ordering and input identity");
  AbandonedRequestsDoNotBlock(directory / "abandoned");
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "isolated native products are removed");
  return outshine::Test::Report();
}
