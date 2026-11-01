#include <Engine.h>
#include "Check.h"
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Engine engine;
  Scenario::Document original;
  original.Events.push_back({.Name = "ready", .Carries = {}});
  CHECK(engine.loadScenario(original), "events without volumes can be assembled");
  const auto *previous = &engine.entities();
  auto invalid = original;
  invalid.Events.push_back(invalid.Events.front());
  CHECK(!engine.loadScenario(invalid), "duplicate catalog rejects startup without a volume");
  CHECK(&engine.entities() == previous, "rejected event catalog preserves published simulation");
  for (const auto &fields : {std::vector<std::string>{""},
                             std::vector<std::string>{"speed", "speed"},
                             std::vector<std::string>{"speed", "direction", "speed"}}) {
    invalid = original;
    invalid.Events[0].Carries = fields;
    CHECK(!engine.loadScenario(invalid),
          "empty or duplicate fields reject startup without volumes");
    CHECK(&engine.entities() == previous, "field rejection preserves the simulation");
  }
  original.Events[0].Carries = {"speed", "Speed"};
  original.Events.push_back({.Name = "another", .Carries = {"speed"}});
  CHECK(engine.loadScenario(original),
        "distinct case and fields reused across events remain valid on retry");
  return Report();
}
