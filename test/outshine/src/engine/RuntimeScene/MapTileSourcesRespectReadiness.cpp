#include <Outshine.h>
#include <SDL3/SDL.h>

#include "Check.h"

#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for the public engine boundary");
  Scenario::View view;
  view.Id = "overview";
  view.Placement = Scenario::CameraPlacement::Geodetic;
  view.Geographic.Geodetic = {.LongitudeDeg = 9.44, .LatitudeDeg = 54.79, .HeightM = 50};
  {
    Scenario::Document scene;
    scene.Ground.Declared = true;
    scene.Ground.Origin = {.LatitudeDeg = 54.79, .LongitudeDeg = 9.44};
    scene.Render.Declared = true;
    scene.Render.Frame = {80, 45};
    scene.Views.push_back(view);
    Engine engine;
    CHECK(engine.setRoots({.Shipped = ".", .Offline = true}) && engine.setRenderTarget({80, 45}) &&
              engine.declare(scene),
          "natural world enters the public engine without source acquisition");
    CHECK(engine.assemble(), "assembly queues world acquisition without requiring original OSM");
    CHECK(!engine.settled(WorldQuality::Refined) &&
              !engine.unsettledReasons(WorldQuality::Refined).empty(),
          "queued assembly cannot claim a complete world without required sources");
    CHECK(!engine.preload(0.02, WorldQuality::Refined) && !engine.settled(WorldQuality::Refined),
          "offline missing inputs remain a failure rather than an empty ready city");
  }
  {
    Scenario::Document scene;
    scene.Views.push_back(view);
    scene.Providers.push_back({.Kind = "vector",
                               .Dataset = "test.map",
                               .Endpoint = "https://fixture.invalid/{z}/{x}/{y}"});
    Engine engine;
    CHECK(engine.setRoots({.Shipped = ".", .Offline = true}) && engine.setRenderTarget({80, 45}) &&
              engine.declare(scene),
          "explicit vector-tile declaration reaches the runtime boundary");
    CHECK(engine.assemble() && engine.settled(WorldQuality::Refined),
          "an unused explicit vector provider is permitted without blocking an empty scene");
  }
  SDL_Quit();
  return Report();
}
