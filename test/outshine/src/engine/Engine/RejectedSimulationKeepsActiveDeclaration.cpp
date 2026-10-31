#include "Check.h"
#include "Engine.h"
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
  Engine engine;
  CHECK(engine.declare(scene) && engine.assemble(), "the original simulation is active");
  auto *const registry = &engine.entities();
  const auto marker = registry->addEntity();
  CHECK(marker, "an independently allocated entity marks the active registry");
  const auto saved = engine.writeScenario();
  CHECK(saved, "the active declaration can be persisted");
  if (!marker || !saved) { return Report(); }
  for (const bool excessive : {false, true}) {
    auto invalid = scene;
    invalid.Motion.StepS = 0.04;
    if (excessive) {
      invalid.Room = std::numeric_limits<size_t>::max();
    } else {
      invalid.Instances[0].Of = "missing";
    }
    CHECK(engine.declare(invalid), "a simulation candidate awaits assembly validation");
    CHECK(engine.stepSeconds() == scene.Motion.StepS,
          "a rejected simulation candidate cannot change the active time step");
    CHECK(!engine.assemble(), "invalid entity references or capacity reject assembly");
    CHECK(&engine.entities() == registry && registry->alive(*marker),
          "the active registry and independent entity survive failed assembly");
    const auto retained = engine.writeScenario();
    CHECK(retained && *retained == *saved && engine.stepSeconds() == scene.Motion.StepS,
          "persistence and simulation still use the same active declaration");
  }
  scene.Motion.StepS = 0.03;
  CHECK(engine.declare(scene) && engine.assemble() && engine.stepSeconds() == scene.Motion.StepS,
        "a valid subsequent declaration can replace the rejected candidate");
  return Report();
}
