#include "ScenarioStartup.h"
#include "Check.h"
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document valid;
  valid.Motion.StepS = 0.02;
  valid.Motion.MostStepsInArrears = 4;
  CHECK(LoadFreshScenario(valid).has_value(), "valid simulation timing is accepted");
  const auto rejected = [&](const Scenario::Document &candidate) {
    const auto result = LoadFreshScenario(candidate);
    CHECK(!result && result.error().contains("finite positive step"),
          "invalid timing is rejected explicitly");
  };
  for (const double step : {0.0,
                            -1.0,
                            std::numeric_limits<double>::quiet_NaN(),
                            std::numeric_limits<double>::infinity(),
                            -std::numeric_limits<double>::infinity(),
                            std::numeric_limits<double>::max()}) {
    auto candidate = valid;
    candidate.Motion.StepS = step;
    rejected(candidate);
  }
  for (const int count : {0, -1, std::numeric_limits<int>::min()}) {
    auto candidate = valid;
    candidate.Motion.MostStepsInArrears = count;
    rejected(candidate);
  }
  CHECK(LoadFreshScenario(valid).has_value(), "valid simulation timing starts independently");
  return Report();
}
