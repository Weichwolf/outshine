#include <Outshine.h>
#include <array>
#include <vector>
#include <string>
#include "Check.h"

namespace {
enum class Setting { Outputs, Transfer, Precision };

outshine::Scenario::Document Scene() {
  outshine::Scenario::Document scene;
  scene.Render.Declared = true;
  scene.Render.Frame = {32, 32};
  scene.Render.Stages = {"subjects"};
  return scene;
}

void ChangedPlanIsValidated(Setting setting) {
  using namespace outshine;
  using namespace outshine::Test;
  Engine reused;
  Engine fresh;
  CHECK(reused.drawsInto({32, 32}).has_value() && fresh.drawsInto({32, 32}).has_value(),
        "independent engines have the same target");
  auto scene = Scene();
  CHECK(reused.declare(scene).has_value(), "the original render plan is initialized");
  CHECK(reused.declare(scene).has_value(), "an unchanged declaration is reusable");
  switch (setting) {
    case Setting::Outputs: scene.Render.Outputs = {"invalid-output"}; break;
    case Setting::Transfer: scene.Render.Transfer = "invalid-transfer"; break;
    case Setting::Precision: scene.Render.Precision = "invalid-precision"; break;
  }
  const auto cold = fresh.declare(scene);
  const auto warm = reused.declare(scene);
  CHECK(!cold && cold.error().find("invalid-") != std::string::npos,
        "first declaration rejects the independently invalid plan parameter");
  CHECK(!warm && !cold && warm.error() == cold.error(),
        "redeclaration validates the changed parameter exactly as first declaration");
  scene = Scene();
  scene.Render.Outputs = {"sceneLinear"};
  scene.Render.Transfer = "filmic";
  CHECK(reused.declare(scene).has_value(), "a valid plan can replace the refused declaration");
  CHECK(reused.renderer().flushAndWait().has_value(), "the replacement device state can settle");
}

void AdditionalOutputIsApplied() {
  using namespace outshine;
  using namespace outshine::Test;
  Engine engine;
  CHECK(engine.drawsInto({32, 32}).has_value(), "output transition has an offscreen target");
  auto scene = Scene();
  Scenario::View view;
  view.Id = "plan-output";
  view.Person = "first";
  view.Placement = Scenario::CameraPlacement::Local;
  view.Sees.PositionM = {{0, 0, 2}};
  view.Sees.setProjection(Camera::Ortho{.XMagM = 2, .YMagM = 2, .NearM = 0.1, .FarM = 10});
  scene.Views.push_back(view);
  CHECK(engine.declare(scene) && engine.assemble() && engine.advance(), "initial scene is ready");
  std::vector<float> velocity{17};
  const auto missing = engine.renderer().readPixels(Buffer::Velocity, velocity);
  CHECK(!missing && missing.error().find("no velocity") != std::string::npos,
        "the initial explicit plan does not retain velocity");
  scene.Render.Outputs = {"sceneVelocity"};
  CHECK(engine.declare(scene) && engine.assemble() && engine.advance(),
        "only the additional output changes");
  CHECK(engine.renderer().readPixels(Buffer::Velocity, velocity).has_value(),
        "the changed plan actually retains the requested velocity attachment");
  CHECK(velocity.size() == 32u * 32u * 2u,
        "the newly retained attachment has two components for every target pixel");
}

}

int main() {
  using namespace outshine::Test;
  const bool initialized = SDL_Init(SDL_INIT_VIDEO);
  CHECK(initialized, "SDL video initializes");
  if (!initialized) { return Report(); }
  for (const Setting setting :
       std::array{Setting::Outputs, Setting::Transfer, Setting::Precision}) {
    ChangedPlanIsValidated(setting);
  }
  AdditionalOutputIsApplied();
  SDL_Quit();
  return Report();
}
