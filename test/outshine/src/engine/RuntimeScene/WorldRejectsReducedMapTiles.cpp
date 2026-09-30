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
    const auto absent = engine.assemble();
    CHECK(!absent && absent.error().find("official original OSM") != std::string::npos,
          "missing original source cannot publish an empty city or select a map-tile fallback");
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
          "explicit legacy tile declaration reaches the runtime boundary");
    const auto rejected = engine.assemble();
    CHECK(!rejected && rejected.error().find("not permitted") != std::string::npos,
          "an explicit tile endpoint cannot bypass original OSM world-source policy");
  }
  SDL_Quit();
  return Report();
}
