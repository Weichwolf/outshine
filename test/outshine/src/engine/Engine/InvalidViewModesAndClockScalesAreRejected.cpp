#include "ScenarioStartup.h"
#include <limits>
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document original;
  Scenario::View view;
  view.Id = "original";
  view.Placement = Scenario::CameraPlacement::Local;
  original.Views.push_back(view);
  CHECK(LoadFreshScenario(original).has_value(), "valid initial view is declared");
  const auto rejected = [&](const Scenario::Document &candidate) {
    CHECK(!LoadFreshScenario(candidate), "invalid mode or clock scale is rejected");
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
      Scenario::Body followed;
      followed.Name = valid.Views[0].Follows;
      followed.Placed = true;
      valid.Bodies.push_back(followed);
    }
    CHECK(LoadFreshScenario(valid).has_value(),
          "supported mode and finite positive factor accepted");
  }
  return Report();
}
