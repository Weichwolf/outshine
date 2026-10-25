#include <Engine.h>
#include "Check.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <unistd.h>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document scene;
  Scenario::Kind root;
  root.Name = "root";
  root.Attributes = {{.Name = "health", .Value = "+1.25e2"}, {.Name = "speed", .Value = "2.5"}};
  scene.Kinds.push_back(root);
  Scenario::Kind derived;
  derived.Name = "actor";
  derived.Inherits = "root";
  derived.Attributes = {{.Name = "health", .Value = "42"}};
  scene.Kinds.push_back(derived);
  Scenario::Instance instance;
  instance.Of = "actor";
  instance.Id = "player";
  instance.Attributes = {{.Name = "health", .Value = "73.5"}};
  scene.Instances.push_back(instance);
  scene.State = {{.What = "player.health"}, {.What = "player.speed"}};
  Engine engine;
  CHECK(engine.declare(scene) && engine.assemble(), "inherited native traits assemble");
  const auto path = std::filesystem::temp_directory_path() /
                    ("outshine-trait-contract-" + std::to_string(getpid()) + ".save");
  const auto checkTraits = [&] {
    const auto saved = engine.save(path.string());
    CHECK(saved.has_value(), "native trait values are observable through public persistence");
    if (!saved) { return; }
    std::ifstream input(path);
    const std::string text{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    CHECK(text.contains("player.health 73.5\n"),
          "instance override wins over derived and root defaults");
    CHECK(text.contains("player.speed 2.5\n"),
          "unoverridden root trait survives the inheritance chain");
    std::filesystem::remove(path);
  };
  checkTraits();
  const World::EntityRegistry *previous = &engine.entities();
  for (const char *invalid : {"nan",
                              "-nan",
                              "NAN",
                              "inf",
                              "-inf",
                              "Infinity",
                              "1e999",
                              "1e-9999",
                              "12tail",
                              " 1",
                              "1 ",
                              "0x1p2",
                              ""}) {
    for (const bool prefab : {false, true}) {
      auto candidate = scene;
      auto &attribute =
          prefab ? candidate.Kinds[0].Attributes[0] : candidate.Instances[0].Attributes[0];
      attribute.Value = invalid;
      CHECK(engine.declare(candidate).has_value(), "numeric validation belongs to assembly");
      const auto assembled = engine.assemble();
      CHECK(!assembled && !assembled.error().empty(), "invalid numeric trait is rejected");
      CHECK(&engine.entities() == previous, "rejected numeric trait preserves the published owner");
      if (&engine.entities() != previous) { return Report(); }
      checkTraits();
    }
  }
  CHECK(engine.declare(scene) && engine.assemble(), "valid assembly recovers after rejection");
  checkTraits();
  return Report();
}
