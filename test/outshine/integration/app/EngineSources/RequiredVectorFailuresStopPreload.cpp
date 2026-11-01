#include "EngineFactory.h"
#include "OsmCacheFixture.h"
#include "SourceFailureReason.h"
#include <Engine.h>
#include <SDL3/SDL.h>

#include "Check.h"

#include <chrono>
#include <atomic>
#include <filesystem>
#include <print>
#include <string>
#include <memory>
#include <optional>
#include <expected>
#include <span>
#include <cstdint>
#include <string_view>
#include <system_error>

namespace {
using namespace outshine;
using namespace outshine::Data;

class FlatTerrain final : public Source {
public:
  FlatTerrain() {
    Decl.Id = "test.flat.terrain";
    Decl.Revision = "r1";
    Decl.Kind = DataKind::Elevation;
    Decl.Wire = WireFormat::ProviderDefined;
    Decl.MaxZoom = 8;
    Decl.Keeps = Cacheability::Never;
    Decl.MaximumPayloadBytes = 1;
  }

  [[nodiscard]] const SourceDecl &Declaration() const noexcept override { return Decl; }

  [[nodiscard]] Coverage Covers(const SourceRequest &request) const noexcept override {
    return request.Kind() == DataKind::Elevation ? Coverage::Inside : Coverage::Outside;
  }

  [[nodiscard]] Address Serves(const SourceRequest &request) const noexcept override {
    return request.Where();
  }

  [[nodiscard]] FetchStart Begin([[maybe_unused]] const Address &where,
                                 [[maybe_unused]] SourceTransport &transport) const override {
    return Ticket::None;
  }

  [[nodiscard]] SourceResponse Collect([[maybe_unused]] const Address &where,
                                       [[maybe_unused]] Ticket ticket,
                                       [[maybe_unused]] SourceTransport &transport) const override {
    return SourceResponse::Delivered({0});
  }

  [[nodiscard]] std::expected<HeightRaster, DecodeFailure>
  DecodeElevation([[maybe_unused]] std::span<const uint8_t> bytes) const override {
    HeightRaster raster;
    raster.Rows = 2;
    raster.Cols = 2;
    raster.Meters.assign(4, 0.0f);
    return raster;
  }

private:
  SourceDecl Decl;
};

class FlatTerrainProvider final : public Provider {
public:
  [[nodiscard]] std::string_view kind() const override { return "test-terrain"; }

  [[nodiscard]] std::expected<std::unique_ptr<Source>, std::string>
  make([[maybe_unused]] const SourceProvider &declaration,
       [[maybe_unused]] std::string_view builtinDataDirectory) const override {
    return std::make_unique<FlatTerrain>();
  }
};

struct VectorFailureControl {
  std::atomic_bool Release{false};
  std::atomic_uint Polls{0};
};

class UnavailableVectorSource final : public Source {
public:
  explicit UnavailableVectorSource(VectorFailureControl &control) : Control_(control) {
    Decl_.Id = "test.terminal.vector";
    Decl_.Revision = "terminal-r1";
    Decl_.Kind = DataKind::VectorMap;
    Decl_.Wire = WireFormat::MapboxVectorTile;
    Decl_.Schema = "shortbread";
    Decl_.MaxZoom = 14;
    Decl_.Keeps = Cacheability::Never;
    Decl_.MaximumPayloadBytes = 1024;
    Decl_.RetryBudget = 1;
  }

  [[nodiscard]] const SourceDecl &Declaration() const noexcept override { return Decl_; }

  [[nodiscard]] Coverage Covers(const SourceRequest &request) const noexcept override {
    return request.Kind() == Decl_.Kind ? Coverage::Inside : Coverage::Outside;
  }

  [[nodiscard]] Address Serves(const SourceRequest &request) const noexcept override {
    return request.Where();
  }

  [[nodiscard]] FetchStart Begin(const Address & /*where*/,
                                 SourceTransport & /*transport*/) const override {
    return Ticket::None;
  }

  [[nodiscard]] SourceResponse Collect(const Address & /*where*/,
                                       Ticket /*ticket*/,
                                       SourceTransport & /*transport*/) const override {
    ++Control_.Polls;
    if (!Control_.Release.load()) { return SourceResponse::Working(); }
    return SourceResponse::MeantAfter(Meaning::Retry, 0.001, SourceFailureReason::Unavailable, 503);
  }

private:
  VectorFailureControl &Control_;
  SourceDecl Decl_;
};

void CheckTerminalRefusal(Scenario::Document scene,
                          Roots roots,
                          const FlatTerrainProvider &terrainProvider) {
  using namespace outshine::Test;
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-engine-vector-refusal-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  roots.SourceCacheDirectory = (directory / "sources").string();
  roots.AssetCacheDirectory = (directory / "assets").string();
  scene.Providers.front() = SourceProvider{.Kind = "osm-cache-fixture",
                                           .Revision = "terminal-r1",
                                           .Dataset = "test.terminal.vector",
                                           .Location = {},
                                           .Endpoint = {},
                                           .Coverage = std::nullopt,
                                           .PayloadSha256 = {},
                                           .Schema = "shortbread"};
  VectorFailureControl control;
  const OsmCache::SourceFactory provider(std::make_unique<UnavailableVectorSource>(control));
  {
    auto engine = App::CreateEngine();
    CHECK(engine.registerProvider(terrainProvider) && engine.registerProvider(provider),
          "the engine owns independent local terrain and vector source definitions");
    const bool opened = engine.setRoots(roots) && engine.setRenderTarget(scene.Render.Frame) &&
                        engine.loadScenario(scene);
    CHECK(opened, "the terminal-response fixture queues its real native source requests");
    if (opened) {
      Result pending;
      for (int probe = 0; probe < 64; ++probe) {
        pending = engine.preload(0.05, WorldQuality::Refined);
        if (pending || control.Polls.load() > 0 ||
            !pending.error().contains("did not become resident within")) {
          break;
        }
      }
      CHECK(!pending && control.Polls.load() > 0 &&
                pending.error().contains("did not become resident within"),
            "a provider that is still working cannot become a terminal source refusal");
      control.Release = true;
      const auto refused = engine.preload(5.0, WorldQuality::Refined);
      CHECK(!refused && refused.error().contains("required vector input unavailable") &&
                refused.error().contains("transport unavailable") &&
                refused.error().contains("source=test.terminal.vector") &&
                refused.error().contains("revision=terminal-r1") &&
                refused.error().contains("http=503; retries=1"),
            "exhausted retries report the actual terminal vector failure at the engine boundary");
      CHECK(!refused && !refused.error().contains("did not become resident within") &&
                !engine.settled(WorldQuality::Refined),
            "a terminal non-offline failure cannot wait for a generic timeout or publish ready");
      std::println("terminal vector result: {}", refused ? "ready" : refused.error().c_str());
    }
  }
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "the terminal-refusal fixture removes its private cache after engine teardown");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for the engine preload boundary");
  Roots roots;
  roots.BuiltinDataDirectory = "data";
  roots.Offline = true;
  Scenario::Document scene;
  scene.Ground.Declared = true;
  scene.Ground.SightM = 2400.0;
  scene.Ground.Origin = {.LatitudeDeg = 54.79, .LongitudeDeg = 9.44};
  scene.Render.Declared = true;
  scene.Render.Frame = {.WidthPx = 80, .HeightPx = 45};
  Scenario::View view;
  view.Id = "missing-data";
  view.Placement = Scenario::CameraPlacement::Geodetic;
  view.Geographic.Geodetic = {.LongitudeDeg = 9.44, .LatitudeDeg = 54.79, .HeightM = 50};
  view.Geographic.SamplesHeight = false;
  scene.Views.push_back(view);
  Data::SourceProvider vector;
  vector.Kind = "vector";
  vector.Dataset = "test.offline.vector";
  vector.Revision = "test-r7";
  vector.Endpoint = "https://fixture.invalid/{z}/{x}/{y}";
  vector.Schema = "shortbread";
  scene.Providers.push_back(vector);
  Data::SourceProvider terrain;
  terrain.Kind = "test-terrain";
  terrain.Revision = "test-r1";
  scene.Providers.push_back(terrain);
  const FlatTerrainProvider terrainProvider;
  {
    auto engine = App::CreateEngine();
    CHECK(engine.registerProvider(terrainProvider), "local terrain provider registers");
    CHECK(engine.setRoots(roots) && engine.setRenderTarget(scene.Render.Frame) &&
              engine.loadScenario(scene),
          "offline scene queues its declared source requests");
    const auto playable = engine.preload(0.0, WorldQuality::Playable);
    CHECK(!playable && !playable.error().contains("required vector input unavailable"),
          "playable quality retains its existing zero-budget behavior");
    constexpr double patienceS = 5.0;
    const auto started = std::chrono::steady_clock::now();
    const auto refined = engine.preload(patienceS, WorldQuality::Refined);
    const auto elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    CHECK(!refined && refined.error().contains("required vector input unavailable") &&
              refined.error().contains("offline cache miss") &&
              refined.error().contains("vector/") &&
              refined.error().contains("source=test.offline.vector") &&
              refined.error().contains("revision=test-r7") && refined.error().contains("key="),
          "refined failure reports the actual missing vector and source identity");
    CHECK(!refined && !refined.error().contains("did not become resident within"),
          "a final offline vector miss does not wait for the generic timeout");
    CHECK(!engine.settled(WorldQuality::Refined),
          "failed acquisition never publishes a ready world");
    std::println("refined preload elapsed {:.6f} s, result: {}",
                 elapsed,
                 refined ? "ready" : refined.error().c_str());
  }
  CheckTerminalRefusal(scene, roots, terrainProvider);
  SDL_Quit();
  return Report();
}
