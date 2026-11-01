#include "ScenarioStartup.h"
#include "Check.h"
#include <array>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const Scenario::Document original;
  CHECK(LoadFreshScenario(original).has_value(), "initial declaration accepted");

  for (const auto field : std::array{&Scenario::Weather::CloudCover,
                                     &Scenario::Weather::CloudLow,
                                     &Scenario::Weather::CloudMid,
                                     &Scenario::Weather::CloudHigh,
                                     &Scenario::Weather::CloudBaseAglM,
                                     &Scenario::Weather::WindDeg,
                                     &Scenario::Weather::WindMs,
                                     &Scenario::Weather::Haze}) {
    for (const double value :
         {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
      auto candidate = original;
      candidate.Ground.Sky.*field = value;
      CHECK(!LoadFreshScenario(candidate), "nonfinite weather rejected even for inactive world");
    }
  }
  for (const auto field : std::array{&Scenario::Weather::CloudCover,
                                     &Scenario::Weather::CloudLow,
                                     &Scenario::Weather::CloudMid,
                                     &Scenario::Weather::CloudHigh,
                                     &Scenario::Weather::CloudBaseAglM,
                                     &Scenario::Weather::WindMs,
                                     &Scenario::Weather::Haze}) {
    auto candidate = original;
    candidate.Ground.Sky.*field = -1;
    CHECK(!LoadFreshScenario(candidate), "negative physical magnitude rejected");
  }
  auto candidate = original;
  candidate.Ground.Sky.Haze = std::numeric_limits<double>::max();
  CHECK(!LoadFreshScenario(candidate), "haze is checked before float conversion");
  candidate.Ground.Sky.Haze = 2.5;
  candidate.Ground.Sky.WindDeg = -90;
  CHECK(LoadFreshScenario(candidate).has_value(), "unwrapped wind and aerosol multiplier accepted");
  return Report();
}
