#include <Outshine.h>
#include <SDL3/SDL.h>
#include "Check.h"

#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Engine engine;
    Scenario::Document document;
    Scenario::View view;
    view.Id = "source-probe";
    view.Placement = Scenario::CameraPlacement::Local;
    view.Sees.setProjection(Camera::Perspective{.FovDeg = 60, .NearM = 0.2, .FarM = 100});
    document.Views.push_back(view);
    document.Providers.push_back(
        {.Kind = "osm",
         .Revision = "pin-r1",
         .Missing = Data::MissingDataPolicy::Fail,
         .Dataset = "openstreetmap",
         .Location = "test/outshine/integration/places/HockenheimringGrandPrix.osm",
         .Coverage = Data::SourceCoverage{
             .WestDeg = 8.54, .SouthDeg = 49.315, .EastDeg = 8.61, .NorthDeg = 49.34}});
    CHECK(engine.setRoots({.Shipped = ".", .Offline = true}) && engine.setRenderTarget({64, 64}) &&
              engine.declare(document) && engine.assemble(),
          "groundless public declaration queues the pinned semantic source");
    CHECK(engine.unsettledReasons(WorldQuality::Playable)
                  .find("semantic OSM transport source pending") != std::string::npos,
          "public readiness reports the in-flight source");
    bool published = false;
    for (int attempt = 0; attempt < 200 && !published; ++attempt) {
      const auto advanced = engine.advance();
      CHECK(advanced,
            advanced ? "the world advances while its semantic graph loads"
                     : advanced.error().c_str());
      published = engine.unsettledReasons(WorldQuality::Playable)
                      .find("semantic OSM transport source pending") == std::string::npos;
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
    CHECK(publishedBytes > 30000.0 && graphMs >= 0.0 && pendingJobs == 0.0,
          "public diagnostics expose the completed source and bounded worker queue");

    Scenario::Document simulationOnly = document;
    simulationOnly.Views.clear();
    Engine headless;
    CHECK(headless.setRoots({.Shipped = ".", .Offline = true}) &&
              headless.declare(simulationOnly) && headless.assemble(),
          "renderer-free simulation queues the same semantic source");
    double headlessBytes = -1.0;
    for (int attempt = 0; attempt < 200 && headlessBytes < 0.0; ++attempt) {
      CHECK(headless.advance(), "renderer-free simulation advances while source loads");
      for (const DiagnosticSample &sample : headless.measures()) {
        if (sample.Name == "semantic OSM source bytes") { headlessBytes = sample.Value; }
      }
      if (headlessBytes < 0.0) { SDL_Delay(10); }
    }
    CHECK(headlessBytes > 30000.0,
          "semantic graph publication does not depend on render target or vector tiles");
  }
  SDL_Quit();
  return Report();
}
