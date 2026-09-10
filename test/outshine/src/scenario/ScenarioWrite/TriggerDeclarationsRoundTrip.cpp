#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Triggers.h"
#include "Check.h"
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document source;
  source.Events = {{.Name = "enter & <room>\"", .Carries = {"speed", "a\tb\nc\r"}},
                   {.Name = "left"}};
  source.Volumes = {
      {.Id = "entry'",
       .In = "region & room",
       .Shape = "sphere",
       .AtM = {1e200, -2.5, 3},
       .ExtentM = {1e-200, 2, 3},
       .Fires = source.Events[0].Name,
       .When = "dwell",
       .DwellS = 0.012345678901234567},
      {.Id = "exit", .Shape = "box", .ExtentM = {1, 2, 3}, .Fires = "left", .When = "exit"}};
  auto check = [&] {
    const std::string text = WriteScenario(source);
    Scenario::Document copy;
    std::string error;
    const bool parsed = ReadScenario(text.data(), text.size(), copy, error);
    CHECK(parsed, error.c_str());
    CHECK(copy.Events.size() == source.Events.size(), "event count survives");
    CHECK(copy.Volumes.size() == source.Volumes.size(), "volume count survives");
    if (copy.Events.size() != source.Events.size() ||
        copy.Volumes.size() != source.Volumes.size()) {
      return;
    }
    for (size_t at = 0; at < source.Events.size(); ++at) {
      CHECK(copy.Events[at].Name == source.Events[at].Name &&
                copy.Events[at].Carries == source.Events[at].Carries,
            "event ordering and escaped fields survive");
    }
    for (size_t at = 0; at < source.Volumes.size(); ++at) {
      const auto &a = source.Volumes[at];
      const auto &b = copy.Volumes[at];
      CHECK(a.Id == b.Id && a.In == b.In && a.Shape == b.Shape && a.Fires == b.Fires &&
                a.When == b.When && a.DwellS == b.DwellS,
            "volume strings and duration survive");
      for (int axis = 0; axis < 3; ++axis) {
        CHECK(a.AtM[axis] == b.AtM[axis] && a.ExtentM[axis] == b.ExtentM[axis],
              "coordinates retain exact doubles");
      }
    }
    CHECK(TriggerField::Stand(copy.Volumes, copy.Events).has_value() ==
              TriggerField::Stand(source.Volumes, source.Events).has_value(),
          "serialization preserves assembly validity");
  };
  check();
  source.Volumes[0].Shape.clear();
  check();
  source.Volumes[0].When.clear();
  const auto invalid = WriteScenario(source);
  Scenario::Document rejected;
  std::string error;
  CHECK(!ReadScenario(invalid.data(), invalid.size(), rejected, error),
        "empty required transition stays invalid after serialization");
  CHECK(WriteScenario({}).find("<volumes>") == std::string::npos, "empty catalogs remain absent");
  return Report();
}
