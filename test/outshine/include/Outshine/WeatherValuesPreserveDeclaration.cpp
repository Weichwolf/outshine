#include <Outshine.h>
#include "Check.h"
#include <array>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Engine engine;
  Scenario::Document original;
  CHECK(engine.declare(original).has_value(), "initial declaration accepted");
  const auto before = engine.writeScenario();
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
      CHECK(!engine.declare(candidate), "nonfinite weather rejected even for inactive world");
      CHECK(engine.writeScenario() == before, "rejected weather preserves declaration");
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
    CHECK(!engine.declare(candidate), "negative physical magnitude rejected");
    CHECK(engine.writeScenario() == before, "negative weather preserves declaration");
  }
  auto candidate = original;
  candidate.Ground.Sky.Haze = std::numeric_limits<double>::max();
  CHECK(!engine.declare(candidate), "haze is checked before float conversion");
  candidate.Ground.Sky.Haze = 2.5;
  candidate.Ground.Sky.WindDeg = -90;
  CHECK(engine.declare(candidate).has_value(), "unwrapped wind and aerosol multiplier accepted");
  return Report();
}
