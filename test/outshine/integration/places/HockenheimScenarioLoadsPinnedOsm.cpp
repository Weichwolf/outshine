#include <Outshine.h>
#include <SDL3/SDL.h>
#include "Check.h"

#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  Engine engine;
  CHECK(engine.setRoots({.Shipped = "src/assets", .Offline = true}),
        "scenario sources resolve inside the shipped asset root");
  CHECK(engine.readScenario("src/assets/places/Hockenheimring.scenario"),
        "the authored Hockenheim scenario declares a pinned semantic OSM source");
  CHECK(engine.assemble(), "the pinned source queues without a renderer or terrain fetch");

  double sourceBytes = -1.0;
  double namedRoutes = -1.0;
  double routeEdges = -1.0;
  for (int attempt = 0; attempt < 200 && sourceBytes < 0.0; ++attempt) {
    const auto advanced = engine.advance();
    CHECK(advanced, advanced ? "headless scenario advances" : advanced.error().c_str());
    for (const DiagnosticSample &sample : engine.measures()) {
      if (sample.Name == "semantic OSM source bytes") { sourceBytes = sample.Value; }
      if (sample.Name == "semantic OSM named routes") { namedRoutes = sample.Value; }
      if (sample.Name == "semantic OSM route edges") { routeEdges = sample.Value; }
    }
    if (sourceBytes < 0.0) { SDL_Delay(10); }
  }
  CHECK(sourceBytes == 33557.0 && namedRoutes == 1.0 && routeEdges == 267.0 &&
            engine.unsettledReasons(WorldQuality::Playable).find("semantic OSM") ==
                std::string::npos,
        "the authored source is verified and published independently of visual tiles");
  return Report();
}
