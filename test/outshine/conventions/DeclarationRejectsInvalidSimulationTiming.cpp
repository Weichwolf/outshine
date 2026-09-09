#include <Outshine.h>
#include "Check.h"
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Engine engine;
  Scenario::Document valid;
  valid.Motion.StepS = 0.02;
  valid.Motion.MostStepsInArrears = 4;
  CHECK(engine.declare(valid).has_value(), "valid simulation timing is accepted");
  const auto rejected = [&](const Scenario::Document &candidate) {
    const auto result = engine.declare(candidate);
    CHECK(!result && result.error().contains("finite positive step"),
          "invalid timing is rejected explicitly");
    CHECK(engine.stepSeconds() == valid.Motion.StepS &&
              engine.declaration().Motion.MostStepsInArrears == valid.Motion.MostStepsInArrears,
          "rejected timing preserves previous declaration");
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
  CHECK(engine.declare(valid).has_value(), "valid declaration still works after rejections");
  return Report();
}
