#include <Engine.h>
#include <array>
#include <cstddef>
#include <vector>
#include <string>
#include "Check.h"

namespace {
enum class Setting { Outputs, Transfer, Precision };

outshine::Scenario::Document Scene() {
  outshine::Scenario::Document scene;
  scene.Render.Declared = true;
  scene.Render.Frame = {.WidthPx = 32, .HeightPx = 32};
  scene.Render.Stages = {"opaqueGeometry"};
  return scene;
}

void InvalidPlanIsRejected(Setting setting) {
  using namespace outshine;
  using namespace outshine::Test;
  Engine engine;
  CHECK(engine.setRenderTarget({32, 32}).has_value(), "startup has an offscreen target");
  auto scene = Scene();
  switch (setting) {
    case Setting::Outputs: scene.Render.Outputs = {"invalid-output"}; break;
    case Setting::Transfer: scene.Render.Transfer = "invalid-transfer"; break;
    case Setting::Precision: scene.Render.Precision = "invalid-precision"; break;
  }
  const auto declared = engine.declare(scene);
  CHECK(!declared && declared.error().contains("invalid-"),
        "startup rejects the independently invalid plan parameter");
}

void DeclaredOutputIsApplied(bool retainVelocity) {
  using namespace outshine;
  using namespace outshine::Test;
  Engine engine;
  CHECK(engine.setRenderTarget({32, 32}).has_value(), "startup has an offscreen target");
  auto scene = Scene();
  if (retainVelocity) {
    scene.Render.Outputs = {"sceneVelocity"};
    scene.Render.Transfer = "filmic";
  }
  Scenario::View view;
  view.Id = "plan-output";
  view.Person = "first";
  view.Placement = Scenario::CameraPlacement::Local;
  view.Sees.PositionM = {{0, 0, 2}};
  view.Sees.setProjection(Camera::Ortho{.XMagM = 2, .YMagM = 2, .NearM = 0.1, .FarM = 10});
  scene.Views.push_back(view);
  CHECK(engine.declare(scene) && engine.assemble() && engine.advance(), "startup scene is ready");
  CHECK(engine.renderer().render({}).has_value(), "render the declared plan");
  std::vector<float> velocity{17};
  const auto pixels = engine.renderer().readPixels(Buffer::Velocity, velocity);
  if (retainVelocity) {
    CHECK(pixels.has_value(), "startup retains the requested velocity attachment");
    CHECK(velocity.size() == size_t{32} * 32 * 2,
          "the retained attachment has two components for every target pixel");
  } else {
    CHECK(!pixels && pixels.error().contains("no velocity"),
          "the explicit plan does not retain an unrequested attachment");
  }
}

}

int main() {
  using namespace outshine::Test;
  const bool initialized = SDL_Init(SDL_INIT_VIDEO);
  CHECK(initialized, "SDL video initializes");
  if (!initialized) { return Report(); }
  for (const Setting setting :
       std::array{Setting::Outputs, Setting::Transfer, Setting::Precision}) {
    InvalidPlanIsRejected(setting);
  }
  DeclaredOutputIsApplied(false);
  DeclaredOutputIsApplied(true);
  SDL_Quit();
  return Report();
}
