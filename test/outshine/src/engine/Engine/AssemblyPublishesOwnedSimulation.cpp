#include <Engine.h>
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
  const auto declared = engine.declare(scene);
  CHECK(declared.has_value(), "simulation declaration accepted without a render target");
  if (!declared) { return Report(); }
  const auto built = engine.assemble();
  CHECK(built.has_value(), built ? "simulation built" : built.error().c_str());
  if (!built) { return Report(); }
  World::EntityRegistry *const original = &engine.entities();
  const auto marker = original->addEntity();
  CHECK(marker.has_value(), "spare entity capacity remains usable after publication");
  if (!marker) { return Report(); }
  CHECK(original->alive(*marker), "spare entity capacity remains usable after publication");
  CheckPublishedTraits(engine);
  auto invalid = scene;
  invalid.Instances[0].Of = "missing";
  CHECK(engine.declare(invalid).has_value(), "new declaration awaits assembly validation");
  CHECK(!engine.assemble(), "missing prefab rejects the candidate assembly");
  CHECK(&engine.entities() == original,
        "failed assembly preserves the borrowed World::EntityRegistry object");
  if (&engine.entities() != original) { return Report(); }
  CHECK(original->alive(*marker),
        "failed assembly preserves existing entities and spare allocations");
  CheckPublishedTraits(engine);
  for (const size_t reserve : {size_t{65536}, std::numeric_limits<size_t>::max()}) {
    auto excessive = scene;
    excessive.Room = reserve;
    CHECK(engine.declare(excessive).has_value(), "capacity request declared before allocation");
    CHECK(!engine.assemble(), "capacity budget and arithmetic overflow rejected");
    CHECK(&engine.entities() == original && original->alive(*marker),
          "budget rejection preserves live scene");
    CheckPublishedTraits(engine);
  }
  CHECK(engine.declare(scene) && engine.assemble(), "valid reassembly recovers after rejection");
  CheckPublishedTraits(engine);
  CHECK(engine.declare({}) && engine.assemble(), "empty simulation assembles without a renderer");
  CHECK(engine.entities().capacity() == 0, "empty assembly removes previous entity storage");
  return Report();
}
