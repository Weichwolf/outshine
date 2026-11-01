#include "ScenarioStartup.h"
#include "Check.h"
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document original;
  original.Events.push_back({.Name = "ready", .Carries = {}});
  CHECK(LoadFreshScenario(original), "events without volumes can be assembled");
  auto invalid = original;
  invalid.Events.push_back(invalid.Events.front());
  CHECK(!LoadFreshScenario(invalid), "duplicate catalog rejects startup without a volume");

  for (const auto &fields : {std::vector<std::string>{""},
                             std::vector<std::string>{"speed", "speed"},
                             std::vector<std::string>{"speed", "direction", "speed"}}) {
    invalid = original;
    invalid.Events[0].Carries = fields;
    CHECK(!LoadFreshScenario(invalid), "empty or duplicate fields reject startup without volumes");
  }
  original.Events[0].Carries = {"speed", "Speed"};
  original.Events.push_back({.Name = "another", .Carries = {"speed"}});
  CHECK(LoadFreshScenario(original),
        "distinct case and fields reused across events remain valid at startup");
  return Report();
}
