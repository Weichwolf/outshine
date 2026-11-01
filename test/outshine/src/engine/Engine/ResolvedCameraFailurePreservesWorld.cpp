#include "Check.h"
#include "Engine.h"
#include <SDL3/SDL.h>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const bool video = SDL_Init(SDL_INIT_VIDEO);
  CHECK(video, "video initializes");
  if (!video) { return Report(); }
  {
    Engine engine;
    CHECK(engine.setRoots({.BuiltinDataDirectory = "data",
                           .SourceCacheDirectory = {},
                           .Offline = true,
                           .AssetCacheDirectory = {}}),
          "isolated offline resource configuration is accepted");
    CHECK(engine.setRenderTarget({32, 32}), "offscreen target opens");
    const Scenario::Document original;
    CHECK(engine.declare(original) && engine.assemble(), "the original world is active");
    const auto saved = engine.writeScenario();
    const auto *previous = &engine.entities();
    CHECK(saved && engine.settled(), "the active world is persistent and ready");

    Scenario::Document candidate;
    candidate.Ground.Declared = true;
    candidate.Ground.VegetationEnabled = false;
    candidate.Ground.SightM = 1000;
    Scenario::View view;
    view.Id = "geodetic";
    view.Person = "first";
    view.Placement = Scenario::CameraPlacement::Geodetic;
    view.Sees.PositionM = {{0, 0, 2}};
    view.Sees.LooksAt = true;
    view.Sees.LookAtM = {{0, 0, 0}};
    candidate.Views.push_back(view);
    CHECK(engine.declare(candidate), "the declared camera has a valid unresolved basis");
    const auto assembled = engine.assemble();
    CHECK(!assembled && assembled.error().contains("no valid local transform"),
          assembled ? "the degenerate resolved camera unexpectedly assembled"
                    : assembled.error().c_str());
    CHECK(engine.writeScenario() == saved && &engine.entities() == previous && engine.settled(),
          "resolved camera failure preserves the active world and simulation together");
    CHECK(engine.declare(original) && engine.assemble(), "a valid declaration recovers");
  }
  SDL_Quit();
  return Report();
}
