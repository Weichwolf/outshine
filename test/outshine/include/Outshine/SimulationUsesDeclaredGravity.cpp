#include <Outshine.h>
#include "Check.h"
#include <algorithm>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const double standard = Scenario::WorldSettings{}.GravityMs2;
  for (const double gravity : {0.0, 2.0, standard}) {
    Scenario::Document scene;
    scene.Ground.GravityMs2 = gravity;
    scene.Motion.StepS = 0.5;
    Scenario::Body body;
    body.Name = "falling";
    body.Placed = true;
    body.MassKg = 1;
    scene.Bodies.push_back(body);
    scene.Events.push_back({.Name = "expected-position"});
    for (const int step : {1, 2, 3}) {
      Scenario::Volume volume;
      volume.Id = "step-" + std::to_string(step);
      volume.Shape = "box";
      volume.AtM[1] = -gravity * 0.25 * static_cast<double>(step * (step + 1)) / 2.0;
      volume.ExtentM = {{1e-8, 1e-8, 1e-8}};
      volume.When = "enter";
      volume.Fires = "expected-position";
      scene.Volumes.push_back(volume);
    }
    Engine engine;
    CHECK(engine.declare(scene) && engine.assemble(), "headless gravity scenario assembled");
    for (const int step : {1, 2, 3}) {
      CHECK(engine.advance().has_value(), "fixed gravity step succeeds");
      const auto fired = std::ranges::count_if(engine.unacted(), [](const std::string &line) {
        return line.starts_with("a volume fired event ");
      });
      CHECK(fired == (gravity == 0.0 ? 3 : step),
            "semi-implicit positions match declared acceleration, including weightlessness");
    }
  }
  return Report();
}
