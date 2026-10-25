#include "Engine.h"
#include "TestEngine.h"
#include "NativeTriangle.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <atomic>
#include <memory>
#include <print>
#include <string>
#include <utility>

namespace {
namespace Native = outshine::Render::Native;

struct SubmittedWork {
  uint64_t LastSerial = 0;
  size_t Count = 0;

  static Native::Result<Native::Completion>
  Submit(void *context, Native::Device *device, std::unique_ptr<Native::Commands> commands) {
    auto completion = device->Submit(std::move(commands));
    if (completion) {
      auto &record = *static_cast<SubmittedWork *>(context);
      record.LastSerial = completion->Serial;
      ++record.Count;
    }
    return completion;
  }
};

void CheckPublication() {
  using namespace outshine;
  using namespace outshine::Test;
  const NativeTriangle triangle;
  const std::string asset(triangle.kind());
  Scenario::Document scene;
  scene.Render.Declared = true;
  scene.Render.Frame = {.WidthPx = 32, .HeightPx = 32};
  scene.Render.Outputs = {"sceneLinear"};
  scene.Assets.push_back(
      {.Uri = asset, .Digest = {}, .Kind = "generated", .Variant = {}, .Surfaces = {}});
  scene.Buses.emplace_back().Id = "master";
  Scenario::Body body;
  body.Placed = true;
  body.Asset = asset;
  body.Name = "camera";
  scene.Bodies.push_back(body);
  body.Name = "source";
  body.Stands.AtM[0] = 1;
  scene.Bodies.push_back(body);
  Scenario::View view;
  view.Id = "following";
  view.Follows = "camera";
  view.Person = "first";
  scene.Views.push_back(view);
  Audio::SoundSource sound;
  sound.Id = "tone";
  sound.Body = "source";
  sound.Spatial.Positional = true;
  sound.Graph.emplace_back().Id = "osc";
  sound.Graph.back().Parameters = {{.Name = "frequency", .Value = "1000"}};
  scene.Sounds.push_back(sound);
  SubmittedWork submissions;
  auto engine = TestEngine::WithGpu({.Context = &submissions, .Submit = SubmittedWork::Submit});
  const bool ready = engine.registerGenerator(triangle) &&
                     engine.setRenderTarget({.WidthPx = 32, .HeightPx = 32}) &&
                     engine.declare(scene) && engine.assemble() && engine.prepareAudio(48000) &&
                     engine.advance();
  CHECK(ready, "one native world publishes a followed camera and positional sound");
  if (!ready) { return; }
  Geometry geometry;
  const auto surface = geometry.addSurface("native-pose", Material{});
  const auto part =
      surface ? geometry.addPart("pose", *surface) : std::expected<int, GeometryPartError>{};
  const bool described =
      surface && part &&
      geometry.setPositions(*part, std::array<float, 9>{-1, -1, 0, 1, -1, 0, 0, 1, 0}) &&
      geometry.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2});
  CHECK(described, "the fixture describes valid native geometry");
  if (!described) { return; }
  const auto installed = engine.setGeometry(geometry);
  CHECK(installed, installed ? "native geometry installs" : installed.error().c_str());
  if (!installed) { return; }
  const auto placed = engine.advance();
  CHECK(placed, placed ? "native body instances prepare" : placed.error().c_str());
  if (!placed) { return; }
  const auto initialFrame = engine.renderer().render({});
  CHECK(initialFrame, initialFrame ? "initial poses render" : initialFrame.error().c_str());
  if (!initialFrame) { return; }
  const uint64_t firstFrameSerial = submissions.LastSerial;
  auto &state = TestEngine::StateOf(engine);
  const auto held = state.BodyFrame;
  const auto camera = state.ScenarioState->ViewBodies.front();
  const auto source = state.Session.AudioBodies.front();
  CHECK(camera && source, "camera and audio have stable native identities");
  if (!camera || !source) { return; }
  constexpr double kCameraM = 5;
  constexpr double kSourceM = 7;
  CHECK(state.ScenarioState->Entities.EditBody(
            *camera, [](World::RigidBodyState &pose) { pose.PositionM[0] = kCameraM; }) &&
            state.ScenarioState->Entities.EditBody(
                *source, [](World::RigidBodyState &pose) { pose.PositionM[0] = kSourceM; }),
        "the single writer changes both native poses before publication");
  state.PublishAudioSnapshot();
  const unsigned oldAudio = state.Session.Told.load(std::memory_order_acquire);
  CHECK(state.Session.Sources[oldAudio].front().AtM[0] == 1 &&
            state.Session.Ear[oldAudio].AtM[0] == 0 &&
            state.BodyFrame.Revision() == held.Revision(),
        "audio cannot accidentally read unpublished mutable poses");
  CHECK(engine.advance(), "the next frame publishes native bodies before all output consumers");
  const unsigned audio = state.Session.Told.load(std::memory_order_acquire);
  CHECK(state.Session.Sources[audio].front().AtM[0] == kSourceM &&
            state.Session.Ear[audio].AtM[0] == kCameraM &&
            state.BodyFrame.Body(*camera)->PositionM[0] == kCameraM &&
            held.Body(*camera)->PositionM[0] == 0,
        "current listener, source and immutable world agree while the held old frame stays valid");
  CHECK(state.Picture.Standing && state.Picture.Standing->InstancesStanding() == 2 &&
            state.Picture.Standing->PlacementStanding(0)[12] == kCameraM,
        "the actual native render placement consumes the same published pose");
  const auto rendered = engine.renderer().render({});
  CHECK(rendered, rendered ? "coherent poses render" : rendered.error().c_str());
  std::println("body-frame-submissions total={} first={} second={} delta={}",
               submissions.Count,
               firstFrameSerial,
               submissions.LastSerial,
               submissions.LastSerial - firstFrameSerial);
  CHECK(firstFrameSerial != 0 && submissions.LastSerial == firstFrameSerial + 1,
        "all changed body placements travel with the next frame without per-body submissions");
  const auto revision = state.BodyFrame.Revision();
  CHECK(engine.advance() && state.BodyFrame.Revision() == revision &&
            state.ScenarioState->Entities.PublicationCost().CopiedBytes == 0,
        "unchanged static frames reuse the publication without a new pose copy");
}
}

int main() {
  using namespace outshine::Test;
  const bool video = SDL_Init(SDL_INIT_VIDEO);
  CHECK(video, "native video initializes");
  if (video) { CheckPublication(); }
  SDL_Quit();
  return Report();
}
