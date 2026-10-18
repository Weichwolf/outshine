#include <Outshine.h>
#include <SDL3/SDL.h>

#include "Check.h"

#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for the public engine boundary");
  constexpr double kLongitudeDeg = 9.44;
  constexpr double kLatitudeDeg = 54.79;
  constexpr double kHeightM = 50;
  constexpr double kPreloadSeconds = 0.02;
  constexpr Extent kFramePx{.WidthPx = 80, .HeightPx = 45};
  Roots roots;
  roots.Shipped = ".";
  roots.Offline = true;
  Scenario::View view;
  view.Id = "overview";
  view.Placement = Scenario::CameraPlacement::Geodetic;
  view.Geographic.Geodetic = {
      .LongitudeDeg = kLongitudeDeg, .LatitudeDeg = kLatitudeDeg, .HeightM = kHeightM};
  {
    Scenario::Document scene;
    scene.Ground.Declared = true;
    scene.Ground.Origin = {.LatitudeDeg = kLatitudeDeg, .LongitudeDeg = kLongitudeDeg};
    scene.Render.Declared = true;
    scene.Render.Frame = kFramePx;
    scene.Views.push_back(view);
    Engine engine;
    CHECK(engine.setRoots(roots) && engine.setRenderTarget(kFramePx) && engine.declare(scene),
          "natural world enters the public engine without source acquisition");
    CHECK(engine.assemble(), "assembly queues world acquisition without requiring original OSM");
    CHECK(!engine.settled(WorldQuality::Refined) &&
              !engine.unsettledReasons(WorldQuality::Refined).empty(),
          "queued assembly cannot claim a complete world without required sources");
    CHECK(!engine.preload(kPreloadSeconds, WorldQuality::Refined) &&
              !engine.settled(WorldQuality::Refined),
          "offline missing inputs remain a failure rather than an empty ready city");
  }
  {
    Scenario::Document scene;
    scene.Views.push_back(view);
    Data::SourceProvider provider;
    provider.Kind = "vector";
    provider.Dataset = "test.map";
    provider.Endpoint = "https://fixture.invalid/{z}/{x}/{y}";
    scene.Providers.push_back(provider);
    Engine engine;
    CHECK(engine.setRoots(roots) && engine.setRenderTarget(kFramePx) && engine.declare(scene),
          "explicit vector-tile declaration reaches the runtime boundary");
    CHECK(engine.assemble() && engine.settled(WorldQuality::Refined),
          "an unused explicit vector provider is permitted without blocking an empty scene");
  }
  SDL_Quit();
  return Report();
}
