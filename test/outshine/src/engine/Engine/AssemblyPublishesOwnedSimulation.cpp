#include "ScenarioStartup.h"
#include "Check.h"
#include "TestEngine.h"
#include <cstddef>
#include <limits>

namespace {
void CheckPublishedTraits(outshine::Engine &engine) {
  using namespace outshine;
  using namespace outshine::Test;
  const auto &runtime = *TestEngine::StateOf(engine).ScenarioState;
  CHECK(&runtime.Entities.Registry() == &engine.entities(),
        "published component columns refer to the live World::EntityRegistry");
  const auto holder = runtime.Stood.InstanceNamed("kept");
  const auto key = runtime.Stood.TraitKey("health");
  const auto *traits = runtime.Attributes.Get(holder);
  const auto *value = traits == nullptr ? nullptr : traits->Named(key);
  CHECK(value != nullptr && *value == 23, "native component value survives publication");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document scene;
  scene.Room = 4;
  Scenario::Kind kind;
  kind.Name = "actor";
  kind.Attributes = {{.Name = "health", .Value = "23"}};
  scene.Kinds.push_back(kind);
  Scenario::Instance instance;
  instance.Of = "actor";
  instance.Id = "kept";
  scene.Instances.push_back(instance);
  Scenario::Body body;
  body.Name = "template";
  scene.Bodies.push_back(body);
  body.Name = "placed";
  body.Placed = true;
  body.Stands.AtM[0] = 2;
  scene.Bodies.push_back(body);
  Engine engine;
  const auto declared = engine.loadScenario(scene);
  CHECK(declared.has_value(), "simulation declaration accepted without a render target");
  if (!declared) { return Report(); }
  World::EntityRegistry *const original = &engine.entities();
  const auto marker = original->addEntity();
  CHECK(marker.has_value(), "spare entity capacity remains usable after publication");
  if (!marker) { return Report(); }
  CHECK(original->alive(*marker), "spare entity capacity remains usable after publication");
  CheckPublishedTraits(engine);
  auto invalid = scene;
  invalid.Instances[0].Of = "missing";
  CHECK(!LoadFreshScenario(invalid), "startup rejects a missing prefab");
  for (const size_t reserve : {size_t{65536}, std::numeric_limits<size_t>::max()}) {
    auto excessive = scene;
    excessive.Room = reserve;
    CHECK(!LoadFreshScenario(excessive), "capacity budget and arithmetic overflow rejected");
  }
  Engine empty;
  CHECK(empty.loadScenario({}), "empty simulation starts without a renderer");
  CHECK(empty.entities().capacity() == 0, "empty startup needs no entity storage");
  return Report();
}
