#include "ScenarioStartup.h"
#include "Check.h"
#include <array>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document original;
  CHECK(LoadFreshScenario(original).has_value(), "initial declaration accepted");

  for (const auto field : std::array{
           &Scenario::Player::EyeHeightM, &Scenario::Player::WalkMs, &Scenario::Player::RunMs}) {
    for (const double value : {-1.0,
                               std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()}) {
      auto candidate = original;
      candidate.Played.*field = value;
      CHECK(!LoadFreshScenario(candidate), "invalid player magnitude refused");
    }
  }
  original.Played.EyeHeightM = original.Played.WalkMs = original.Played.RunMs = 0;
  CHECK(LoadFreshScenario(original).has_value(), "zero magnitudes accepted at startup");
  return Report();
}
