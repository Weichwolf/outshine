#include <Outshine.h>
#include <SDL3/SDL.h>
#include "Check.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for the public camera contract");
  const auto directory = std::filesystem::temp_directory_path();
  const auto path = directory / "outshine-follow-camera.gltf";
  std::ofstream file(path);
  file << R"GLTF({
    "asset":{"version":"2.0"},"extensionsUsed":["KHR_materials_unlit"],
    "buffers":[{"byteLength":36,"uri":"data:application/octet-stream;base64,AACAvwAAgL8AAADAAACAPwAAgL8AAADAAAAAAAAAgD8AAADA"}],
    "bufferViews":[{"buffer":0,"byteLength":36}],
    "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[-1,-1,-2],"max":[1,1,-2]}],
    "materials":[{"pbrMetallicRoughness":{"baseColorFactor":[1,0.25,0.05,1]},"extensions":{"KHR_materials_unlit":{}}}],
    "meshes":[{"primitives":[{"attributes":{"POSITION":0},"material":0}]}],
    "nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0
  })GLTF";
  file.close();
  Scenario::Document scene;
  scene.Render.Declared = true;
  scene.Render.Frame = {64, 64};
  scene.Render.Outputs = {"sceneLinear", "surface"};
  scene.Assets.push_back({.Uri = path.string(), .Kind = "gltf"});
  Scenario::Body body;
  body.Name = "template";
  scene.Bodies.push_back(body);
  body.Placed = true;
  body.MassKg = 0;
  body.Asset = path.string();
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
    const auto ready =
        engine.drawsInto(Extent{64, 64}) && engine.declare(scene) && engine.assemble();
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
    CHECK(engine.renderer()
              .saveScreenshot((directory / "outshine-follow-camera-right.png").string())
              .has_value(),
          "capture right target");
    std::vector<float> beforeReplacement;
    CHECK(engine.renderer().readPixels(Buffer::Linear, beforeReplacement).has_value(),
          "the accepted imported asset has a readable frame");
    auto missingAsset = scene;
    missingAsset.Assets.front().Uri = (directory / "outshine-absent-asset.gltf").string();
    const auto refusedAsset = engine.declare(missingAsset);
    CHECK(!refusedAsset && refusedAsset.error().find("outshine-absent-asset") != std::string::npos,
          "a late imported-asset failure is rejected by the public declaration");
    CHECK(engine.declaration().Assets.front().Uri == scene.Assets.front().Uri,
          "the rejected asset keeps the published declaration");
    std::vector<float> afterReplacement;
    CHECK(engine.renderer().render({}).has_value() &&
              engine.renderer().readPixels(Buffer::Linear, afterReplacement).has_value() &&
              afterReplacement == beforeReplacement,
          "a rejected imported-asset replacement preserves the rendered world");
    CHECK(engine.declare(scene) && engine.assemble(),
          "the accepted asset immediately retries after the rejected replacement");
    CHECK(engine.setView("left-view").has_value(), "switch active target");
    checkEye(-4);
    CHECK(engine.renderer().render({}).has_value(), "render left camera");
    CHECK(engine.renderer()
              .saveScreenshot((directory / "outshine-follow-camera-left.png").string())
              .has_value(),
          "capture left target");
    std::swap(scene.Bodies[1], scene.Bodies[2]);
    CHECK(engine.declare(scene) && engine.assemble(), "reordered bodies reassemble");
    checkEye(4);
    auto invalid = scene;
    invalid.Views[0].Follows = "missing";
    CHECK(engine.declare(invalid).has_value(), "target validation waits for assembly");
    const EntityRegistry *previous = &engine.entities();
    CHECK(!engine.assemble() && &engine.entities() == previous,
          "missing target preserves old assembly");
    CHECK(!engine.advance(), "new camera cannot silently follow old assembly");
    invalid.Views[0].Follows = "template";
    CHECK(engine.declare(invalid) && !engine.assemble(), "unplaced target rejected");
    invalid = scene;
    invalid.Bodies.push_back(scene.Bodies[1]);
    CHECK(engine.declare(invalid) && !engine.assemble(), "ambiguous target rejected");
  }
  std::filesystem::remove(path);
  SDL_Quit();
  return Report();
}
