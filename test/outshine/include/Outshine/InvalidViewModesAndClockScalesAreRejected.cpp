#include <Outshine.h>
#include <limits>
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Engine engine;
  Scenario::Document original;
  original.Views.push_back({.Id = "original", .Placement = Scenario::CameraPlacement::Local});
  CHECK(engine.declare(original).has_value(), "valid initial view is declared");
  const auto *previous = engine.declaration().Views.data();
  const auto rejected = [&](const Scenario::Document &candidate) {
    CHECK(!engine.declare(candidate), "invalid mode or clock scale is rejected");
    CHECK(engine.declaration().Views.data() == previous,
          "invalid values cannot replace the owned declaration");
    CHECK(engine.setView("original").has_value(), "previous catalog remains selectable");
    CHECK(!engine.setView("candidate"), "invalid catalog is never published");
  };
  for (const int mode : {-1, 3, std::numeric_limits<int>::max()}) {
    auto candidate = original;
    candidate.Views[0].Id = "candidate";
    candidate.Views[0].Placement = static_cast<Scenario::CameraPlacement>(mode);
    rejected(candidate);
  }
  for (const double scale : {0.0,
                             -1.0,
                             std::numeric_limits<double>::quiet_NaN(),
                             std::numeric_limits<double>::infinity(),
                             -std::numeric_limits<double>::infinity()}) {
    auto candidate = original;
    candidate.Views[0].Id = "candidate";
    candidate.Views[0].TimeScale = scale;
    rejected(candidate);
  }
  for (const auto mode : {Scenario::CameraPlacement::Local,
                          Scenario::CameraPlacement::Geodetic,
                          Scenario::CameraPlacement::FollowEntity}) {
    auto valid = original;
    valid.Views[0].Placement = mode;
    valid.Views[0].TimeScale = 0.25;
    if (mode == Scenario::CameraPlacement::FollowEntity) {
      valid.Views[0].Follows = "body-resolved-by-assembly";
      valid.Views[0].Person = "first";
    }
    CHECK(engine.declare(valid).has_value(), "supported mode and finite positive factor accepted");
  }
  return Report();
}
