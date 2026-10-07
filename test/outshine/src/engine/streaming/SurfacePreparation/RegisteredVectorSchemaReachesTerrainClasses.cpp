#include "Check.h"
#include "SurfacePreparation.h"
#include "Sink.h"
#include "test/outshine/src/generators/osm/MvtLayer/WireFixture.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <string>
#include <system_error>
#include <thread>

namespace {

outshine::Test::Mvt::Bytes WaterTile() {
  using outshine::Test::Mvt::Append;
  using outshine::Test::Mvt::Bytes;
  Bytes layer{0x0a, 5, 'w', 'a', 't', 'e', 'r', 0x78, 2, 0x28, 64};
  Append(layer, 0x1a, Bytes{'c', 'l', 'a', 's', 's'});
  Append(layer, 0x22, Bytes{0x0a, 5, 'o', 'c', 'e', 'a', 'n'});
  Bytes feature{0x08, 1, 0x18, 3};
  Append(feature, 0x12, Bytes{0, 0});
  Append(feature, 0x22, Bytes{9, 0, 0, 26, 128, 1, 0, 0, 128, 1, 127, 0, 15});
  Append(layer, 0x12, feature);
  Bytes tile;
  Append(tile, 0x1a, layer);
  return tile;
}

class VectorTransport final : public outshine::Data::Transport {
public:
  outshine::Data::FetchStart Begin(const std::string &) override {
    return static_cast<outshine::Data::Ticket>(1);
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket) override {
    return outshine::Data::Wire::Answered(200, WaterTile());
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
      ("outshine-vector-schema-" +
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
  VectorTransport wire;
  const std::array providers{
      Data::SourceProvider{.Kind = "vector",
                           .Dataset = "fixture.openmaptiles",
                           .Endpoint = "https://fixture.invalid/{z}/{x}/{y}.pbf",
                           .Schema = "openmaptiles"}};
  Tasks compute(1);
  SurfacePreparation stack;
  constexpr LongitudeLatitude focus{.LongitudeDeg = 1, .LatitudeDeg = 1};
  CHECK(stack.Open({.Shipped = "src/assets", .Cache = cache.Path.string()},
                   providers,
                   focus,
                   wire,
                   compute,
                   sink,
                   nullptr,
                   1),
        "the registered OpenMapTiles source opens through production preparation");
  if (!stack.Opened()) { return Report(); }
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  do {
    const auto advanced = stack.AdvanceAt(focus, {.IngestTilesMost = 1, .VectorRing = 0});
    CHECK(advanced.has_value(), "source acquisition and terrain classes advance together");
    if (!advanced || stack.Classes().Complete()) { break; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < deadline);
  CHECK(stack.Classes().Complete(), "both classification tiers finish from the registered source");
  CHECK(stack.Classes().FeaturesHeld() > 0 && stack.Classes().FeaturesTaken() > 0,
        "the water layer reaches terrain classification rather than an empty publication");
  const auto *water = stack.Vegetation().Find("water_polygons", "ocean");
  CHECK(water && stack.Classes().ClassAt(focus, nullptr, nullptr) == water->Tpl,
        "the original OpenMapTiles ocean polygon classifies the camera position as water");
  while (!stack.InputsReady() && std::chrono::steady_clock::now() < deadline) {
    CHECK(stack.AdvanceAt(focus, {.IngestTilesMost = 1, .VectorRing = 0}).has_value(),
          "initial source inputs settle independently of outer-ring acquisition");
  }
  CHECK(stack.InputsReady() && stack.Vectors() &&
            !stack.Vectors()->SettledWithin(Generators::Osm::kEveryRing),
        "initial source readiness permits the first region lookup before requesting outer rings");
  return Report();
}
