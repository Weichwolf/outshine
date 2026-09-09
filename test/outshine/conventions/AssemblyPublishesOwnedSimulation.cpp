#include <Outshine.h>
#include "Check.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <unistd.h>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document scene;
  scene.Room = 4;
  Scenario::Kind kind;
  kind.Name = "actor";
  kind.Attributes = {{"health", "23"}};
  scene.Kinds.push_back(kind);
  scene.Instances.push_back({.Of = "actor", .Id = "kept"});
  scene.State.push_back({.What = "kept.health"});
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
  Scene *const original = &engine.scene();
  const Entity marker = original->addEntity(Role::Tool);
  CHECK(original->alive(marker), "spare entity capacity remains usable after publication");
  const auto path = std::filesystem::temp_directory_path() /
                    ("outshine-assembly-owner-" + std::to_string(getpid()) + ".save");
  const auto saved = engine.save(path.string());
  CHECK(saved.has_value(), "published component columns still refer to the live Scene");
  if (saved) {
    std::ifstream file(path);
    const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    CHECK(text.find("kept.health 23\n") != std::string::npos,
          "native component value survives publication");
  }
  std::filesystem::remove(path);
  auto invalid = scene;
  invalid.Instances[0].Of = "missing";
  CHECK(engine.declare(invalid).has_value(), "new declaration awaits assembly validation");
  CHECK(!engine.assemble(), "missing prefab rejects the candidate assembly");
  CHECK(&engine.scene() == original, "failed assembly preserves the borrowed Scene object");
  if (&engine.scene() != original) { return Report(); }
  CHECK(original->alive(marker),
        "failed assembly preserves existing entities and spare allocations");
  for (const size_t reserve : {size_t{65536}, std::numeric_limits<size_t>::max()}) {
    auto excessive = scene;
    excessive.Room = reserve;
    CHECK(engine.declare(excessive).has_value(), "capacity request declared before allocation");
    CHECK(!engine.assemble(), "capacity budget and arithmetic overflow rejected");
    CHECK(&engine.scene() == original && original->alive(marker),
          "budget rejection preserves live scene");
  }
  CHECK(engine.declare(scene) && engine.assemble(), "valid reassembly recovers after rejection");
  CHECK(engine.save(path.string()).has_value(),
        "replacement component columns retain valid ownership");
  std::filesystem::remove(path);
  CHECK(engine.declare({}) && engine.assemble(), "empty simulation assembles without a renderer");
  CHECK(engine.scene().capacity() == 0, "empty assembly removes previous entity storage");
  return Report();
}
