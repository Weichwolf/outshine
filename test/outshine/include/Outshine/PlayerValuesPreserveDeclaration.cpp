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
  for (const auto field : std::array{
           &Scenario::Player::EyeHeightM, &Scenario::Player::WalkMs, &Scenario::Player::RunMs}) {
    for (double value : {-1.0,
                         std::numeric_limits<double>::infinity(),
                         std::numeric_limits<double>::quiet_NaN()}) {
      auto candidate = original;
      candidate.Played.*field = value;
      CHECK(!engine.declare(candidate), "invalid player magnitude refused");
      CHECK(engine.writeScenario() == before, "invalid player preserves declaration");
    }
  }
  original.Played.EyeHeightM = original.Played.WalkMs = original.Played.RunMs = 0;
  CHECK(engine.declare(original).has_value(), "zero magnitudes accepted on valid retry");
  return Report();
}
