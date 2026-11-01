#include <Engine.h>
#include "Check.h"
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Engine engine;
  Scenario::Document space;
  space.Views.push_back({.Id = "space",
                         .Sees = {},
                         .Placement = Scenario::CameraPlacement::Local,
                         .Geographic = {},
                         .Route = {},
                         .Viewport = {},
                         .In = {},
                         .Follows = {},
                         .Person = {},
                         .OffsetM = {}});
  CHECK(engine.loadScenario(space), "a groundless scenario is valid");
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
  Engine terrestrial;
  CHECK(terrestrial.loadScenario(earth), "a headless earth scenario starts independently");
  CHECK(!terrestrial.prepareViewData(1.0, 1.0),
        "data preparation requires a rendered ground world");
  return Report();
}
