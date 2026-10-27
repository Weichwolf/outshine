#include "HockenheimCircuitFixture.h"
#include "EngineFactory.h"
#include <Engine.h>
#include <SDL3/SDL.h>
#include <utility>
#include "Check.h"

#include <string>

namespace Circuit = outshine::Test::HockenheimFixture;

namespace {
constexpr outshine::Camera::Perspective kSourceProjection{.FovDeg = 60, .NearM = 0.2, .FarM = 100};
constexpr int kPublicationPolls = 200;
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    auto engine = App::CreateEngine();
    Scenario::Document document;
    Scenario::View view;
    view.Id = "source-probe";
    view.Placement = Scenario::CameraPlacement::Local;
    view.Sees.setProjection(kSourceProjection);
    document.Views.push_back(view);
    auto localSource = Circuit::LocalSource();
    localSource.Dataset = "openstreetmap.original";
    localSource.Revision = "original-r1";
    document.Providers.push_back(std::move(localSource));
    document.Routes.push_back({.Id = "grand-prix", .OsmRelationId = Circuit::RelationId});
    CHECK(engine.setRoots({.BuiltinDataDirectory = ".",
                           .SourceCacheDirectory = {},
                           .Offline = true,
                           .AssetCacheDirectory = {}}) &&
              engine.setRenderTarget({64, 64}) && engine.declare(document) && engine.assemble(),
          "groundless public declaration queues the pinned semantic source");
    CHECK(engine.unsettledReasons(WorldQuality::Playable)
              .contains("semantic OSM transport source pending"),
          "public readiness reports the in-flight source");
    bool published = false;
    for (int attempt = 0; attempt < kPublicationPolls && !published; ++attempt) {
      const auto advanced = engine.advance();
      CHECK(advanced,
            advanced ? "the world advances while its semantic graph loads"
                     : advanced.error().c_str());
      published = !engine.unsettledReasons(WorldQuality::Playable)
                       .contains("semantic OSM transport source pending");
      if (!published) { SDL_Delay(10); }
    }
    CHECK(published, "public readiness observes the complete graph publication");
    double publishedBytes = -1.0;
    double graphMs = -1.0;
    double pendingJobs = -1.0;
    for (const DiagnosticSample &sample : engine.measures()) {
      if (sample.Name == "semantic OSM source bytes") { publishedBytes = sample.Value; }
      if (sample.Name == "semantic OSM graph time") { graphMs = sample.Value; }
      if (sample.Name == "semantic OSM jobs pending") { pendingJobs = sample.Value; }
    }
    CHECK(publishedBytes > Circuit::MinimumReportedBytes && graphMs >= 0.0 && pendingJobs == 0.0,
          "public diagnostics expose the completed source and bounded worker queue");

    Scenario::Document simulationOnly = document;
    simulationOnly.Views.clear();
    auto headless = App::CreateEngine();
    CHECK(headless.setRoots({.BuiltinDataDirectory = ".",
                             .SourceCacheDirectory = {},
                             .Offline = true,
                             .AssetCacheDirectory = {}}) &&
              headless.declare(simulationOnly) && headless.assemble(),
          "renderer-free simulation queues the same semantic source");
    double headlessBytes = -1.0;
    for (int attempt = 0; attempt < kPublicationPolls && headlessBytes < 0.0; ++attempt) {
      CHECK(headless.advance(), "renderer-free simulation advances while source loads");
      for (const DiagnosticSample &sample : headless.measures()) {
        if (sample.Name == "semantic OSM source bytes") { headlessBytes = sample.Value; }
      }
      if (headlessBytes < 0.0) { SDL_Delay(10); }
    }
    CHECK(headlessBytes > Circuit::MinimumReportedBytes,
          "semantic graph publication does not depend on render target or vector tiles");
  }
  SDL_Quit();
  return Report();
}
