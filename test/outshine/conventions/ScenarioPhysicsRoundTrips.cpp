#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Check.h"
#include <limits>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (const double step : {1.0 / 60.0, 0.012345678901234567, std::numeric_limits<double>::min()}) {
    Scenario::Document source;
    source.Motion.Declared = true;
    source.Motion.Dial = "clock & <mode> \"quoted\" 'single'";
    source.Motion.StepS = step;
    source.Motion.MostStepsInArrears = 7;
    const auto text = WriteScenario(source);
    Scenario::Document copy;
    std::string error;
    CHECK(ReadScenario(text.data(), text.size(), copy, error), "written physics parses");
    CHECK(copy.Motion.Declared && copy.Motion.StepS == step &&
              copy.Motion.MostStepsInArrears == 7 && copy.Motion.Dial == source.Motion.Dial,
          "physics declaration retains exact numeric values and escaped text");
  }
  CHECK(WriteScenario({}).find("<physics") == std::string::npos,
        "undeclared physics remains absent");
  return Report();
}
