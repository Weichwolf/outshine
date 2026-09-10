#include <Outshine.h>
#include "Check.h"
#include <string>
#include <vector>

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
  for (const auto &fields : {std::vector<std::string>{""},
                             std::vector<std::string>{"speed", "speed"},
                             std::vector<std::string>{"speed", "direction", "speed"}}) {
    invalid = original;
    invalid.Events[0].Carries = fields;
    CHECK(engine.declare(invalid).has_value(), "field schema waits for assembly");
    CHECK(!engine.assemble(), "empty or duplicate fields reject assembly without volumes");
    CHECK(&engine.entities() == previous, "field rejection preserves the simulation");
  }
  original.Events[0].Carries = {"speed", "Speed"};
  original.Events.push_back({.Name = "another", .Carries = {"speed"}});
  CHECK(engine.declare(original) && engine.assemble(),
        "distinct case and fields reused across events remain valid on retry");
  return Report();
}
