#include <Outshine.h>
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Engine engine;
  Scenario::Document original;
  original.Events.push_back({.Name = "ready"});
  CHECK(engine.declare(original) && engine.assemble(), "events without volumes can be assembled");
  const auto *previous = &engine.entities();
  auto invalid = original;
  invalid.Events.push_back(invalid.Events.front());
  CHECK(engine.declare(invalid).has_value(), "event catalog is prepared by assembly");
  CHECK(!engine.assemble(), "duplicate catalog rejects assembly without needing a volume");
  CHECK(&engine.entities() == previous, "rejected event catalog preserves published simulation");
  CHECK(engine.declare(original) && engine.assemble(), "valid retry remains possible");
  return Report();
}
