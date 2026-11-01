#include "ImageCapture.h"
#include <Engine.h>
#include "NativeTriangle.h"
#include <SDL3/SDL.h>
#include "Check.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for the public camera contract");
  const std::filesystem::path directory(PlantedPath("follow-camera"));
  std::filesystem::create_directories(directory);
  const NativeTriangle generator;
  const std::string assetName(generator.kind());
  Scenario::Document scene;
  scene.Render.Declared = true;
  scene.Render.Frame = {.WidthPx = 64, .HeightPx = 64};
  scene.Render.Outputs = {"sceneLinear", "surface"};
  Scenario::Asset asset;
  asset.Uri = assetName;
  asset.Kind = "generated";
  scene.Assets.push_back(asset);
  Scenario::Body body;
  body.Name = "template";
  scene.Bodies.push_back(body);
  body.Placed = true;
  body.MassKg = 0;
  body.Asset = assetName;
  body.Name = "left";
  body.Stands.AtM[0] = -4;
  scene.Bodies.push_back(body);
  body.Name = "right";
  body.Stands.AtM[0] = 4;
  scene.Bodies.push_back(body);
  Scenario::View view;
  view.Id = "right-view";
  view.Follows = "right";
  view.Person = "first";
  scene.Views.push_back(view);
  view.Id = "left-view";
  view.Follows = "left";
  scene.Views.push_back(view);
  {
    Engine engine;
    CHECK(engine.registerGenerator(generator), "native camera fixture generator registers");
    const auto ready =
        engine.setRenderTarget(Extent{.WidthPx = 64, .HeightPx = 64}) && engine.loadScenario(scene);
    CHECK(ready, "camera targets resolve before rendering");
    if (!ready) { return Report(); }
    const auto checkEye = [&](double expected) {
      const auto advanced = engine.advance();
      CHECK(advanced.has_value(), advanced ? "camera advanced" : advanced.error().c_str());
      const std::span<const DiagnosticSample> values = engine.measures();
      const auto found =
          std::ranges::find(values, "the carried eye, east", &DiagnosticSample::Name);
      CHECK(found != values.end() && found->Value == expected,
            "camera position comes from the named entity independently of array order");
    };
    checkEye(4);
    CHECK(engine.renderer().render({}).has_value(), "render right camera");
    CHECK(
        outshine::App::SaveImage(engine, (directory / "outshine-follow-camera-right.png").string())
            .has_value(),
        "capture right target");
    CHECK(engine.setView("left-view").has_value(), "switch active target");
    checkEye(-4);
    CHECK(engine.renderer().render({}).has_value(), "render left camera");
    CHECK(outshine::App::SaveImage(engine, (directory / "outshine-follow-camera-left.png").string())
              .has_value(),
          "capture left target");
    const auto rejected = [&](const Scenario::Document &invalid) {
      Engine probe;
      CHECK(probe.registerGenerator(generator), "camera validation generator registers");
      CHECK(probe.setRenderTarget(Extent{64, 64}), "camera validation target opens");
      const auto loaded = probe.loadScenario(invalid);
      CHECK(!loaded && !loaded.error().empty(), "startup rejects an unresolved camera target");
    };
    auto invalid = scene;
    invalid.Views[0].Follows = "missing";
    rejected(invalid);
    invalid.Views[0].Follows = "template";
    rejected(invalid);
    invalid = scene;
    invalid.Bodies.push_back(scene.Bodies[1]);
    rejected(invalid);
  }
  std::filesystem::remove_all(directory);
  SDL_Quit();
  return Report();
}
