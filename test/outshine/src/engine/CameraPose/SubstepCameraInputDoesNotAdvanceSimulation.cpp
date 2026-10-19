#include <Outshine.h>
#include "Check.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video is available for native camera publication");
  Scenario::Document scene;
  scene.Render.Declared = true;
  scene.Render.Frame = {32, 32};
  scene.Render.Outputs = {"surface"};
  scene.Motion.StepS = 1.0 / 60.0;
  scene.Ground.GravityMs2 = 2;
  Scenario::Body body;
  body.Name = "falling";
  body.Placed = true;
  body.MassKg = 1;
  scene.Bodies.push_back(body);
  scene.Events.push_back({.Name = "rest", .Carries = {}});
  scene.Events.push_back({.Name = "one-step", .Carries = {}});
  for (const int step : {0, 1}) {
    Scenario::Volume volume;
    volume.Id = step == 0 ? "at-rest" : "after-one-step";
    volume.Shape = "box";
    volume.AtM[1] = -step * scene.Ground.GravityMs2 * scene.Motion.StepS * scene.Motion.StepS;
    volume.ExtentM = {{1e-8, 1e-8, 1e-8}};
    volume.When = "enter";
    volume.Fires = step == 0 ? "rest" : "one-step";
    scene.Volumes.push_back(volume);
  }
  Scenario::View view;
  view.Id = "start";
  view.Placement = Scenario::CameraPlacement::Local;
  view.Sees.PositionM = {{0, 0, 2}};
  view.Sees.FovDeg = 70;
  scene.Views.push_back(view);
  Engine engine;
  const auto stood = engine.setRenderTarget({32, 32}) && engine.declare(scene) &&
                     engine.assemble() && engine.setView(view.Id);
  CHECK(stood, "a native scene is prepared without a measured frame");
  if (!stood) { return Report(); }
  const auto fired = [&engine] {
    return std::ranges::count_if(engine.unacted(), [](const std::string &line) {
      return line.starts_with("a volume fired event ");
    });
  };
  Camera selected;
  CHECK(engine.advance(0.0) && engine.renderer().camera(selected) &&
            selected.PositionM == view.Sees.PositionM &&
            std::abs(selected.FovDeg - view.Sees.FovDeg) < 1e-10,
        "zero-time publication applies the selected declared view before measurement");
  CHECK(fired() == 0, "selecting the starting camera does not integrate or probe triggers");
  engine.keepSamples(8);
  Camera desired;
  desired.PositionM = {{4, 2, 3}};
  desired.FovDeg = 5;
  CHECK(engine.setCameraPose(desired), "camera input is staged independently of simulation time");
  CHECK(engine.advance(0.0), "zero elapsed time can apply pending camera input");
  Camera actual;
  CHECK(engine.renderer().camera(actual) && actual.PositionM == desired.PositionM &&
            std::abs(actual.FovDeg - view.Sees.FovDeg) < 1e-10,
        "the pending pose is applied while retaining the declared projection");
  std::vector<double> steps;
  engine.stepTimesMs(steps);
  CHECK(steps.empty(), "a camera-only update is not an extra simulation step");
  CHECK(fired() == 0, "zero-time camera input does not probe the body resting in a trigger");
  desired.PositionM[0] = 6;
  CHECK(engine.setCameraPose(desired) && engine.advance(1.0 / 120.0),
        "the first120Hz camera update is accepted inside a60Hz simulation interval");
  CHECK(engine.renderer().camera(actual) && actual.PositionM == desired.PositionM,
        "rendering can use the new camera before the next simulation step");
  engine.stepTimesMs(steps);
  CHECK(steps.empty(), "the first half step preserves the simulation accumulator");
  CHECK(fired() == 0, "the first half interval causes no physical or trigger event");
  CHECK(engine.renderer().render({}), "the actual GPU frame accepts the substep camera");
  auto invalid = desired;
  invalid.PositionM[0] = std::numeric_limits<double>::quiet_NaN();
  CHECK(!engine.setCameraPose(invalid), "invalid input cannot replace the published camera");
  CHECK(engine.renderer().camera(actual) && actual.PositionM == desired.PositionM,
        "rejection preserves the last valid logical pose");
  CHECK(engine.advance(1.0 / 120.0), "the second half interval reaches one physical step");
  engine.stepTimesMs(steps);
  CHECK(steps.size() == 1, "exactly one simulation step was run across both half intervals");
  CHECK(fired() == 1,
        "the body reaches exactly the one-step gravity position and fires its spatial event");
  CHECK(engine.renderer().flushAndWait(), "all submitted device work completes");
  return Report();
}
