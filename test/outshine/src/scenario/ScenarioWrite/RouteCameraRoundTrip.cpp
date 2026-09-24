#include "Check.h"
#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Views.h"

#include <string>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  constexpr std::string_view input = R"(<scenario><views>
    <view id="overview" placement="local"><at x="0" y="10" z="0"/></view>
    <view id="lap" placement="route" route="circuit" person="third" distanceM="5"
          risesBy="0.4" eyeHeightM="1.7" lateralOffsetM="-0.5" lookAheadM="18"
          maximumSpeedMps="27" accelerationMs2="2.5" brakingMs2="5.5"
          lateralAccelerationMs2="4.5" fovDeg="68"/>
  </views></scenario>)";
  Scenario::Document source;
  std::string error;
  CHECK(ReadScenario(input.data(), input.size(), source, error), error.c_str());
  CHECK(source.Views.size() == 2, "overview and route camera parse independently");
  if (source.Views.size() != 2) { return Report(); }
  const auto validated = ViewBook::Stand(source.Views, "overview");
  CHECK(validated,
        validated ? "a valid route rig enters the view catalog" : validated.error().c_str());
  const auto written = WriteScenario(source);
  CHECK(written, written ? "route camera exports" : written.error().c_str());
  if (!written) { return Report(); }
  Scenario::Document copy;
  CHECK(ReadScenario(written->data(), written->size(), copy, error), error.c_str());
  CHECK(copy.Views.size() == 2 && copy.Views[1].Placement == Scenario::CameraPlacement::Route &&
            copy.Views[1].Route.RouteId == "circuit" && copy.Views[1].Person == "third" &&
            copy.Views[1].DistanceM == 5.0 && copy.Views[1].RisesBy == 0.4 &&
            copy.Views[1].Route.EyeHeightM == 1.7 && copy.Views[1].Route.LateralOffsetM == -0.5 &&
            copy.Views[1].Route.LookAheadM == 18.0 && copy.Views[1].Route.MaximumSpeedMps == 27.0 &&
            copy.Views[1].Route.AccelerationMs2 == 2.5 && copy.Views[1].Route.BrakingMs2 == 5.5 &&
            copy.Views[1].Route.LateralAccelerationMs2 == 4.5 && copy.Views[1].Sees.FovDeg == 68.0,
        "route identity, rig and speed bounds survive the scenario round trip");
  source.Views[1].Route.LookAheadM = 0.0;
  CHECK(!ViewBook::Stand(source.Views, "overview"),
        "zero lookahead is rejected before selecting a route camera");
  source.Views[1].Route.LookAheadM = 18.0;
  source.Views[1].Person = "first";
  CHECK(!ViewBook::Stand(source.Views, "overview"),
        "a driver's-eye rig cannot silently retain a third-person chase distance");
  return Report();
}
