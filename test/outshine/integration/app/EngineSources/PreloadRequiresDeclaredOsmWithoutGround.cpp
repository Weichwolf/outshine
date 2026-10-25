#include "HockenheimCircuitFixture.h"
#include "EngineFactory.h"
#include <Engine.h>
#include <SDL3/SDL.h>
#include <utility>
#include "Check.h"

#include <cstdlib>
#include <filesystem>
#include <system_error>

namespace Circuit = outshine::Test::HockenheimFixture;

namespace {
constexpr outshine::Camera::Perspective kSourceProjection{.FovDeg = 60, .NearM = 0.2, .FarM = 100};
constexpr double kMissingPreloadS = 0.2;
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-original-preload-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated empty cache created");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  {
    Scenario::Document scene;
    Scenario::View view;
    view.Id = "source";
    view.Placement = Scenario::CameraPlacement::Local;
    view.Sees.setProjection(kSourceProjection);
    scene.Views.push_back(view);
    auto localSource = Circuit::LocalSource();
    localSource.Dataset = "openstreetmap.original";
    localSource.Revision = "original-r1";
    scene.Providers.push_back(std::move(localSource));
    auto local = App::CreateEngine();
    CHECK(local.setRoots({.Assets = {},
                          .Shipped = ".",
                          .Cache = directory,
                          .Offline = true,
                          .AssetCache = {}}) &&
              local.setRenderTarget({64, 64}) && local.declare(scene) && local.assemble(),
          "groundless scene queues original source and graph");
    CHECK(!local.settled() && !local.settled(WorldQuality::Refined),
          "queued source prevents either readiness level");
    CHECK(!local.beginCapture(), "capture cannot pin a scene with a pending original source");
    CHECK(local.preload(2.0, WorldQuality::Refined) && local.settled() &&
              local.settled(WorldQuality::Refined),
          "preload completes original source and graph without ground or advance calls");
    const Loading loading = local.loading();
    CHECK(loading.PreloadPumps > 0 && loading.PreloadAwaits > 0 &&
              loading.Waited.WorldWorkerCalls > 0,
          "source-only preload pumps and awaits its actual workers");
    double bytes = 0.0;
    double pending = -1.0;
    for (const DiagnosticSample &sample : local.measures()) {
      if (sample.Name == "semantic OSM source bytes") { bytes = sample.Value; }
      if (sample.Name == "semantic OSM jobs pending") { pending = sample.Value; }
    }
    CHECK(bytes > Circuit::MinimumReportedBytes && pending == 0.0,
          "successful preload has published actual original bytes and completed jobs");
    CHECK(local.beginCapture().has_value(), "completed original world permits capture");

    scene.Providers.front().Location.clear();
    scene.Providers.front().Endpoint = "https://api.openstreetmap.org/api/0.6";
    auto missing = App::CreateEngine();
    CHECK(
        missing.setRoots(
            {.Assets = {}, .Shipped = {}, .Cache = directory, .Offline = true, .AssetCache = {}}) &&
            missing.setRenderTarget({64, 64}) && missing.declare(scene) && missing.assemble(),
        "offline remote declaration assembles without fetching or a fallback");
    const auto absent = missing.preload(kMissingPreloadS);
    CHECK(!absent && !missing.settled() && !missing.beginCapture(),
          "missing original cache cannot produce successful preload or capture");
    CHECK(!absent && absent.error().contains("api.openstreetmap.org"),
          "failed preload identifies the required official source");

    scene.Providers.front().Endpoint.clear();
    scene.Providers.front().Location = "missing-original-preload.osm";
    auto invalid = App::CreateEngine();
    CHECK(
        invalid.setRoots(
            {.Assets = {}, .Shipped = directory, .Cache = {}, .Offline = true, .AssetCache = {}}) &&
            invalid.setRenderTarget({64, 64}) && invalid.declare(scene) && invalid.assemble(),
        "missing local source remains an asynchronous request");
    CHECK(!invalid.preload(kMissingPreloadS) && !invalid.settled() && !invalid.beginCapture(),
          "source read failure cannot become a complete groundless world");
  }
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "isolated cache removed");
  SDL_Quit();
  return Report();
}
