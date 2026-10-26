#include "ShippedProviders.h"
#include "Check.h"
#include "Sink.h"
#include "SurfacePreparation.h"
#include "MvtWireFixture.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

namespace {

class StreetTransport final : public outshine::Data::Transport {
public:
  outshine::Data::FetchStart Begin(const std::string & /*url*/) override {
    return static_cast<outshine::Data::Ticket>(1);
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket /*ticket*/) override {
    using outshine::Test::Mvt::Append;
    using outshine::Test::Mvt::Bytes;
    constexpr uint8_t kLayerKeys = 0x1a;
    constexpr uint8_t kLayerValues = 0x22;
    constexpr uint8_t kLayerFeatures = 0x12;
    constexpr uint8_t kFeatureTags = 0x12;
    constexpr uint8_t kFeatureGeometry = 0x22;
    constexpr uint8_t kTileLayers = 0x1a;
    constexpr int kHttpOk = 200;
    constexpr std::array<uint8_t, 13> kLayerHeader{
        0x0a, 7, 's', 't', 'r', 'e', 'e', 't', 's', 0x78, 2, 0x28, 64};
    constexpr std::array<uint8_t, 6> kPathValue{0x0a, 4, 'p', 'a', 't', 'h'};
    constexpr std::array<uint8_t, 2> kGroundLayerValue{0x28, 0};
    constexpr std::array<uint8_t, 4> kFeatureHeader{0x08, 1, 0x18, 2};
    constexpr std::array<uint8_t, 6> kLineGeometry{9, 0, 0, 10, 64, 64};
    Bytes layer(kLayerHeader.begin(), kLayerHeader.end());
    Append(layer, kLayerKeys, Bytes{'k', 'i', 'n', 'd'});
    Append(layer, kLayerKeys, Bytes{'l', 'a', 'y', 'e', 'r'});
    Append(layer, kLayerValues, kPathValue);
    Append(layer, kLayerValues, kGroundLayerValue);
    Bytes feature(kFeatureHeader.begin(), kFeatureHeader.end());
    Append(feature, kFeatureTags, Bytes{0, 0, 1, 1});
    Append(feature, kFeatureGeometry, kLineGeometry);
    Append(layer, kLayerFeatures, feature);
    Bytes tile;
    Append(tile, kTileLayers, layer);
    return outshine::Data::Wire::Answered(kHttpOk, std::move(tile));
  }

  void Cancel(outshine::Data::Ticket /*ticket*/) override {}
};

class SilentSink final : public outshine::Sink {
public:
  void Number(const char * /*what*/, double /*value*/, const char * /*unit*/) override {}

  void Claim(bool /*held*/, const char * /*why*/) override {}

  void Near(double /*got*/,
            double /*want*/,
            double /*within*/,
            const char * /*unit*/,
            const char * /*why*/) override {}

  void Say(const std::string & /*line*/) override {}
};

struct TemporaryCache {
  std::filesystem::path Path =
      std::filesystem::temp_directory_path() /
      ("outshine-cached-network-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

  ~TemporaryCache() {
    std::error_code error;
    std::filesystem::remove_all(Path, error);
  }
};

}

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  TemporaryCache cache;
  SilentSink sink;
  StreetTransport wire;
  Data::SourceProvider source;
  source.Kind = "vector";
  source.Dataset = "fixture.shortbread";
  source.Endpoint = "https://fixture.invalid/{z}/{x}/{y}.pbf";
  source.Schema = "shortbread";
  const std::array providers{source};
  Tasks compute(1);
  Data::ProviderRegistry registry;
  Generators::RegisterShippedProviders(registry);
  SurfacePreparation stack;
  constexpr LongitudeLatitude focus{.LongitudeDeg = 8.5659, .LatitudeDeg = 49.3274};
  constexpr LongitudeLatitude moved{.LongitudeDeg = 8.6159, .LatitudeDeg = 49.3274};
  const auto open = [&] {
    return stack.Open({.BuiltinData = "data", .Sources = cache.Path.string(), .Assets = {}},
                      providers,
                      registry,
                      focus,
                      wire,
                      compute,
                      sink,
                      nullptr,
                      1.0);
  };
  const auto advance = [&](LongitudeLatitude at) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    auto result = stack.AdvanceAt(at, {.IngestTilesMost = 1, .VectorRing = 0});
    while (result && stack.Vectors() && !stack.Vectors()->SettledWithin(0) &&
           std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      result = stack.AdvanceAt(at, {.IngestTilesMost = 1, .VectorRing = 0});
    }
    return result;
  };
  stack.UseCachedRegionNetwork(focus);
  CHECK(!stack.UsesCachedRegionNetwork(), "a closed stack cannot publish cached readiness");
  CHECK(open(), "vector preparation opens for the native-network path");
  if (!stack.Opened()) { return Report(); }
  stack.UseCachedRegionNetwork(focus);
  CHECK(advance(focus).has_value(), "cached network advances without source street assembly");
  CHECK(stack.Vectors() && stack.Ways().IngestedTiles() == 0 &&
            !stack.Ways().Ingested(*stack.Vectors()) && stack.Ways().HeapBytes() == 0 &&
            stack.LastAdvance().StreetsMs == 0.0,
        "a region hit allocates no street field and reports no street assembly time");
  if (stack.Vectors() == nullptr) { return Report(); }
  constexpr LongitudeLatitude nearby{.LongitudeDeg = 8.565901, .LatitudeDeg = 49.327401};
  const auto initialTile = Generators::Osm::OsmField::Locate(focus, stack.VectorZoom());
  const auto nearbyTile = Generators::Osm::OsmField::Locate(nearby, stack.VectorZoom());
  CHECK(initialTile && nearbyTile && initialTile->X == nearbyTile->X &&
            initialTile->Y == nearbyTile->Y,
        "the nearby camera requires exactly the same source tile");
  const auto generation = stack.Vectors()->Generation();
  CHECK(advance(nearby).has_value() && stack.UsesCachedRegionNetwork() &&
            stack.Vectors()->Generation() == generation && stack.Ways().IngestedTiles() == 0 &&
            stack.Ways().HeapBytes() == 0 && stack.LastAdvance().StreetsMs == 0.0,
        "movement inside the source tile preserves the cached network without source work");
  stack.RequestStreets();
  stack.UseCachedRegionNetwork(focus);
  CHECK(!stack.UsesCachedRegionNetwork(), "a source request survives repeated region-hit polls");
  CHECK(advance(focus).has_value() && stack.Ways().IngestedTiles() == 1 &&
            stack.Ways().Ways().size() == 1 && stack.Ways().Ingested(*stack.Vectors()),
        "a native building miss resumes the real street-source digest path");
  stack.Close();
  CHECK(open(), "reopening resets the required-source state");
  if (!stack.Opened()) { return Report(); }
  stack.UseCachedRegionNetwork(focus);
  CHECK(stack.UsesCachedRegionNetwork() && advance(focus).has_value() &&
            stack.Ways().IngestedTiles() == 0,
        "a fresh cached region can defer streets again");
  CHECK(advance(moved).has_value() && !stack.UsesCachedRegionNetwork() &&
            stack.Ways().IngestedTiles() == 1,
        "camera movement leaves the cached-network scope and prepares new street inputs");
  stack.Close();
  CHECK(open(), "a separate cached scope opens for explicit scenario changes");
  if (!stack.Opened()) { return Report(); }
  stack.UseCachedRegionNetwork(focus);
  stack.ForgetCachedRegionNetwork();
  CHECK(!stack.UsesCachedRegionNetwork(), "a different region request retires the cached scope");
  stack.UseCachedRegionNetwork(focus);
  stack.UseCachedRegionNetwork(
      {.LongitudeDeg = std::numeric_limits<double>::quiet_NaN(), .LatitudeDeg = focus.LatitudeDeg});
  CHECK(!stack.UsesCachedRegionNetwork(),
        "an invalid source scope cannot publish cached readiness");
  stack.UseCachedRegionNetwork(focus);
  const std::array<Generators::Osm::OsmField::Declared, 1> declared{{
      {.Layer = Generators::Osm::OsmLayerName(Generators::Osm::OsmLayer::Streets),
       .Key = "kind",
       .Value = "path",
       .WidthM = 4.0,
       .LatLon = {49.3274, 8.5659, 49.3275, 8.5660}},
  }};
  CHECK(stack.Declares(declared), "declared contents receive a persistent input identity");
  stack.UseCachedRegionNetwork(focus);
  CHECK(!stack.UsesCachedRegionNetwork() && advance(focus).has_value() &&
            stack.Ways().Ways().size() == 1,
        "declared roads invalidate the cached network and publish actual street geometry");
  stack.Close();
  CHECK(!stack.UsesCachedRegionNetwork(), "closing retires cached-network readiness");
  return Report();
}
