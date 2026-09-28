#include <Outshine.h>
#include "Check.h"
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Engine engine;
  Scenario::Document space;
  space.Views.push_back({.Id = "space", .Placement = Scenario::CameraPlacement::Local});
  CHECK(engine.declare(space), "a groundless scenario is valid");
  const auto *views = engine.declaration().Views.data();
  for (const double bad :
       {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    CHECK(!engine.prepareViewData(bad, 1.0), "invalid horizons are refused even without ground");
    CHECK(!engine.prepareViewData(1.0, bad),
          "invalid global budgets are refused even without ground");
  }
  CHECK(engine.prepareViewData(3600.0, 0.0), "groundless data preparation is an immediate no-op");
  CHECK(engine.declaration().Views.data() == views && engine.setView("space"),
        "preparation and rejection preserve the owned view catalog");
  auto earth = space;
  earth.Ground.Declared = true;
  earth.Ground.Origin.LatitudeDeg = 49.3;
  earth.Ground.Origin.LongitudeDeg = 8.6;
  CHECK(engine.declare(earth), "an unassembled earth scenario is valid");
  CHECK(!engine.prepareViewData(1.0, 1.0), "data preparation cannot bypass ground assembly");
  return Report();
}
