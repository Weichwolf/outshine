#include <Outshine.h>
#include "Check.h"
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Engine engine;
  Scenario::Document original;
  CHECK(engine.declare(original).has_value(), "initial declaration accepted");
  const auto before = engine.writeScenario();
  for (const double budget :
       {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    auto candidate = original;
    candidate.Compositors.push_back({.Kind = "detail", .BudgetPx = budget, .On = false});
    CHECK(!engine.declare(candidate), "invalid compositor refused even when disabled");
    CHECK(engine.writeScenario() == before, "invalid budget preserves declaration");
  }
  auto candidate = original;
  candidate.Compositors.push_back({});
  CHECK(!engine.declare(candidate), "empty category rejected");
  CHECK(engine.writeScenario() == before, "invalid category preserves declaration");
  for (const double budget : {0.0, 0.125}) {
    candidate.Compositors = {{.Kind = "detail", .BudgetPx = budget}};
    CHECK(engine.declare(candidate).has_value(), "valid retry accepted");
    CHECK(engine.writeScenario().has_value(), "valid metadata can be exported");
  }
  return Report();
}
