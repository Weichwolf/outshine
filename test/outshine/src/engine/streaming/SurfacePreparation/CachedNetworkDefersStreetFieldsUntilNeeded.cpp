#include "Check.h"
#include "Sink.h"
#include "SurfacePreparation.h"
#include "test/outshine/src/generators/osm/MvtLayer/WireFixture.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

namespace {

class StreetTransport final : public outshine::Data::Transport {
public:
  outshine::Data::FetchStart Begin(const std::string &) override {
    return static_cast<outshine::Data::Ticket>(1);
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket) override {
    using outshine::Test::Mvt::Append;
    using outshine::Test::Mvt::Bytes;
    Bytes layer{0x0a, 7, 's', 't', 'r', 'e', 'e', 't', 's', 0x78, 2, 0x28, 64};
    Append(layer, 0x1a, Bytes{'k', 'i', 'n', 'd'});
    Append(layer, 0x1a, Bytes{'l', 'a', 'y', 'e', 'r'});
    Append(layer, 0x22, Bytes{0x0a, 4, 'p', 'a', 't', 'h'});
    Append(layer, 0x22, Bytes{0x28, 0});
    Bytes feature{0x08, 1, 0x18, 2};
    Append(feature, 0x12, Bytes{0, 0, 1, 1});
    Append(feature, 0x22, Bytes{9, 0, 0, 10, 64, 64});
    Append(layer, 0x12, feature);
    Bytes tile;
    Append(tile, 0x1a, layer);
    return outshine::Data::Wire::Answered(200, std::move(tile));
  }

  void Cancel(outshine::Data::Ticket) override {}
};

class SilentSink final : public outshine::Sink {
public:
  void Number(const char *, double, const char *) override {}

  void Claim(bool, const char *) override {}

  void Near(double, double, double, const char *, const char *) override {}

  void Say(const std::string &) override {}
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
  const std::array providers{
      Data::SourceProvider{.Kind = "vector",
                           .Dataset = "fixture.shortbread",
                           .Endpoint = "https://fixture.invalid/{z}/{x}/{y}.pbf",
                           .Schema = "shortbread"}};
  Tasks compute(1);
  SurfacePreparation stack;
  constexpr LongitudeLatitude focus{.LongitudeDeg = 8.5659, .LatitudeDeg = 49.3274};
  constexpr LongitudeLatitude moved{.LongitudeDeg = 8.6159, .LatitudeDeg = 49.3274};
  const auto open = [&] {
    return stack.Open({.Shipped = "src/assets", .Cache = cache.Path.string()},
                      providers,
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
  const std::array<Generators::Osm::OsmField::Declared, 1> declared{{
      {.Layer = Generators::Osm::OsmLayerName(Generators::Osm::OsmLayer::Streets),
       .Key = "kind",
       .Value = "path",
       .WidthM = 4.0,
       .LatLon = {49.3274, 8.5659, 49.3275, 8.5660}},
  }};
  stack.Declares(declared);
  stack.UseCachedRegionNetwork(focus);
  CHECK(!stack.UsesCachedRegionNetwork() && advance(focus).has_value() &&
            stack.Ways().Ways().size() == 1,
        "declared roads invalidate the cached network and publish actual street geometry");
  stack.Close();
  CHECK(!stack.UsesCachedRegionNetwork(), "closing retires cached-network readiness");
  return Report();
}
