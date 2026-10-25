#include "Engine.h"
#include "TestEngine.h"
#include "NativeTriangle.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <atomic>
#include <cmath>
#include <string>

namespace {
using namespace outshine;
using namespace outshine::Test;

void CheckRight(Engine &engine) {
  std::array<float, 128> samples{};
  CHECK(engine.mix(samples), "prepared native audio produces a block");
  double left = 0;
  double right = 0;
  for (size_t frame = 0; frame < samples.size() / 2; ++frame) {
    left += std::abs(samples[2 * frame]);
    right += std::abs(samples[2 * frame + 1]);
  }
  CHECK(left == 0 && right > 1, "the named right-hand entity remains the audible source");
}

void CheckCamera() {
  const NativeTriangle triangle;
  const std::string asset(triangle.kind());
  Scenario::Document scene;
  scene.Render.Declared = true;
  scene.Render.Frame = {.WidthPx = 32, .HeightPx = 32};
  scene.Render.Outputs = {"sceneLinear"};
  scene.Assets.push_back(
      {.Uri = asset, .Digest = {}, .Kind = "generated", .Variant = {}, .Surfaces = {}});
  Scenario::Body body;
  body.Placed = true;
  body.Asset = asset;
  body.Name = "left";
  body.Stands.AtM[0] = -1;
  scene.Bodies.push_back(body);
  body.Name = "right";
  body.Stands.AtM[0] = 1;
  scene.Bodies.push_back(body);
  Scenario::View view;
  view.Id = "following";
  view.Follows = "right";
  view.Person = "first";
  scene.Views.push_back(view);
  Engine engine;
  const bool ready = engine.registerGenerator(triangle) &&
                     engine.setRenderTarget({.WidthPx = 32, .HeightPx = 32}) &&
                     engine.declare(scene) && engine.assemble() && engine.advance();
  CHECK(ready, "the named camera follows a native body before row replacement");
  if (!ready) { return; }
  auto &world = TestEngine::StateOf(engine).ScenarioState->Entities;
  const auto rows = world.Bodies();
  CHECK(world.SetBodies({rows.rbegin(), rows.rend()}), "camera body rows are replaced");
  CHECK(engine.advance(), "the native camera advances against the replaced world rows");
  const auto values = engine.measures();
  const auto eye = std::ranges::find(values, "the carried eye, east", &DiagnosticSample::Name);
  CHECK(eye != values.end() && eye->Value == 1,
        "the camera follows entity identity without reassembling its binding");
}
}

int main() {
  Scenario::Document scene;
  scene.Buses.emplace_back().Id = "master";
  Scenario::Body body;
  body.Placed = true;
  body.Name = "left";
  body.Stands.AtM[0] = -1;
  scene.Bodies.push_back(body);
  body.Name = "right";
  body.Stands.AtM[0] = 1;
  scene.Bodies.push_back(body);
  Audio::SoundSource sound;
  sound.Id = "tone";
  sound.Body = "right";
  sound.Spatial.Positional = true;
  sound.Graph.emplace_back().Id = "osc";
  sound.Graph.back().Parameters = {{.Name = "frequency", .Value = "1000"}};
  scene.Sounds.push_back(sound);
  Engine engine;
  const bool ready = engine.declare(scene) && engine.assemble() && engine.prepareAudio(48000);
  CHECK(ready, "named audio binds once to the native world");
  if (!ready) { return Report(); }
  CheckRight(engine);
  auto &state = TestEngine::StateOf(engine);
  auto &world = state.ScenarioState->Entities;
  const auto rows = world.Bodies();
  CHECK(world.SetBodies({rows.rbegin(), rows.rend()}),
        "native body rows are replaced in reverse order");
  state.PublishAudioSnapshot();
  CheckRight(engine);
  const unsigned told = state.Session.Told.load(std::memory_order_acquire);
  CHECK(state.Session.Sources[told].front().AtM[0] == 1,
        "the published native source pose follows identity rather than its old row");
  const bool video = SDL_Init(SDL_INIT_VIDEO);
  CHECK(video, "video initializes for the native camera integration");
  if (video) { CheckCamera(); }
  SDL_Quit();
  return Report();
}
