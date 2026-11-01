#include "Check.h"
#include "ScenarioStartup.h"
#include <cstddef>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document scene;
  scene.Room = 4;
  scene.Motion.StepS = 0.02;
  Scenario::Kind actor;
  actor.Name = "actor";
  scene.Kinds.push_back(actor);
  Scenario::Instance instance;
  instance.Of = "actor";
  instance.Id = "kept";
  scene.Instances.push_back(instance);
  for (const bool excessive : {false, true}) {
    auto invalid = scene;
    invalid.Motion.StepS = 0.04;
    if (excessive) {
      invalid.Room = std::numeric_limits<size_t>::max();
    } else {
      invalid.Instances[0].Of = "missing";
    }
    CHECK(!LoadFreshScenario(invalid), "startup rejects invalid entity references or capacity");
  }
  scene.Motion.StepS = 0.03;
  Engine engine;
  CHECK(engine.loadScenario(scene) && engine.stepSeconds() == scene.Motion.StepS,
        "startup applies its declared simulation step");
  return Report();
}
