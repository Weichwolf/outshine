#include <Engine.h>
#include "Check.h"
#include <array>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document scene;
  scene.Room = 4;
  Scenario::Kind actor;
  actor.Name = "actor";
  scene.Kinds.push_back(actor);
  Scenario::Instance instance;
  instance.Of = "actor";
  instance.Id = "actor";
  scene.Instances.push_back(instance);
  Engine engine;
  CHECK(engine.declare(scene) && engine.assemble(), "kind and instance have separate namespaces");
  World::EntityRegistry *const original = &engine.entities();
  const auto marker = original->addEntity();
  CHECK(marker && original->alive(*marker), "live marker created");
  if (!marker) { return Report(); }
  std::array<Scenario::Document, 4> invalid{scene, scene, scene, scene};
  invalid[0].Kinds[0].Name.clear();
  invalid[0].Instances[0].Of.clear();
  invalid[1].Instances[0].Id.clear();
  invalid[2].Kinds.push_back(scene.Kinds[0]);
  invalid[3].Instances.push_back(scene.Instances[0]);
  for (const auto &candidate : invalid) {
    CHECK(engine.declare(candidate).has_value(), "identity validation occurs at assembly");
    const auto assembled = engine.assemble();
    CHECK(!assembled, "empty or duplicate identity rejected");
    CHECK(assembled || !assembled.error().empty(), "identity failure explains rejection");
    CHECK(&engine.entities() == original, "failed assembly preserves registry object");
    if (&engine.entities() != original) { return Report(); }
    CHECK(original->alive(*marker), "failed assembly preserves live entities");
  }
  auto linked = scene;
  Scenario::Kind derived;
  derived.Name = "derived";
  derived.Inherits = "actor";
  linked.Kinds.push_back(derived);
  Scenario::Instance child;
  child.Of = "derived";
  child.Id = "child";
  child.In = "actor";
  linked.Instances.push_back(child);
  CHECK(engine.declare(linked) && engine.assemble(), "valid inheritance and containment recover");
  return Report();
}
