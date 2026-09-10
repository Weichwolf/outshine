#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Check.h"
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (const double step : {0.0, 17.123456789012345}) {
    Scenario::Document source;
    source.WheelStepPx = step;
    source.Input = {{.Event = "KeyW", .Action = "move & <forward>\"\t"},
                    {.Event = "MouseX", .Action = "look'\n"}};
    const auto text = WriteScenario(source);
    CHECK(text.has_value(), "input export succeeds");
    if (!text) { continue; }
    Scenario::Document copy;
    std::string error;
    CHECK(ReadScenario(text->data(), text->size(), copy, error), error.c_str());
    CHECK(copy.WheelStepPx == step, "wheel step survives exactly, including zero");
    CHECK(copy.Input.size() == source.Input.size(), "binding count survives");
    if (copy.Input.size() != source.Input.size()) { continue; }
    for (size_t i = 0; i < source.Input.size(); ++i) {
      CHECK(copy.Input[i].Event == source.Input[i].Event &&
                copy.Input[i].Action == source.Input[i].Action,
            "binding order and literal escaped action preserved");
    }
  }
  return Report();
}
