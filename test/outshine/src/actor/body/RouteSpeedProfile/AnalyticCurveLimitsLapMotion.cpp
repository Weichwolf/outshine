#include "Check.h"
#include "RouteSpeedProfile.h"

#include <cmath>
#include <limits>
#include <numbers>
#include <optional>

int main() {
  using namespace outshine;
  using namespace outshine::Motion;
  using namespace outshine::Test;

  constexpr double radiusM = 30.0;
  constexpr double lapM = 2.0 * std::numbers::pi * radiusM;
  const RouteSpeedProfile::Sampler circle = [](double stationM) -> std::optional<RouteCurveSample> {
    const double angle = stationM / radiusM;
    return RouteCurveSample{
        .PositionM = {{radiusM * std::cos(angle), 0.0, radiusM * std::sin(angle)}},
        .Forward = {{-std::sin(angle), 0.0, std::cos(angle)}}};
  };
  constexpr RouteSpeedLimits limits{.MaximumSpeedMps = 30.0,
                                    .AccelerationMs2 = 3.0,
                                    .BrakingMs2 = 6.0,
                                    .LateralAccelerationMs2 = 5.0,
                                    .MaximumSampleSpacingM = 1.0};
  const auto profile = RouteSpeedProfile::Build(lapM, circle, limits);
  CHECK(profile && profile->SampleCount() > 180 && profile->DurationS() > 0.0,
        "an analytic closed circle produces a bounded one-lap speed plan");
  if (profile) {
    double previousStationM = 0.0;
    double previousSpeedMps = 0.0;
    double highestSpeedMps = 0.0;
    bool physicallyBounded = true;
    constexpr double tickS = 1.0 / 60.0;
    for (double timeS = 0.0; timeS <= profile->DurationS(); timeS += tickS) {
      const auto at = profile->AtTime(timeS);
      physicallyBounded &=
          at && at->StationM >= previousStationM && at->StationM <= lapM && at->SpeedMps >= 0.0 &&
          at->SpeedMps * at->SpeedMps / radiusM < 5.1 &&
          at->SpeedMps - previousSpeedMps <= limits.AccelerationMs2 * tickS + 1e-8 &&
          previousSpeedMps - at->SpeedMps <= limits.BrakingMs2 * tickS + 1e-8;
      if (at) {
        previousStationM = at->StationM;
        previousSpeedMps = at->SpeedMps;
        highestSpeedMps = std::max(highestSpeedMps, at->SpeedMps);
      }
    }
    const auto start = profile->AtTime(0.0);
    const auto finish = profile->AtTime(profile->DurationS());
    CHECK(
        physicallyBounded && highestSpeedMps > 10.0 && start && start->StationM == 0.0 &&
            start->SpeedMps == 0.0 && finish && finish->StationM == lapM &&
            finish->SpeedMps == 0.0 && !profile->AtTime(-1.0) &&
            !profile->AtTime(std::numeric_limits<double>::quiet_NaN()),
        "the camera reaches the same seam without exceeding lateral, acceleration or brake limits");
  }

  const RouteSpeedProfile::Sampler straight =
      [](double stationM) -> std::optional<RouteCurveSample> {
    return RouteCurveSample{.PositionM = {{stationM, 0.0, 0.0}}, .Forward = {{1.0, 0.0, 0.0}}};
  };
  const auto straightPlan = RouteSpeedProfile::Build(200.0, straight, {.MaximumSpeedMps = 20.0});
  CHECK(straightPlan && straightPlan->AtTime(straightPlan->DurationS() * 0.5)->SpeedMps >= 19.0,
        "a long straight reaches the declared maximum speed before braking");
  const auto tooLong = RouteSpeedProfile::Build(100000.0, straight, limits);
  CHECK(!tooLong && tooLong.error() == RouteSpeedError::SampleBudget,
        "the planner refuses routes beyond its declared sampling budget");
  const RouteSpeedProfile::Sampler absent = [](double) -> std::optional<RouteCurveSample> {
    return std::nullopt;
  };
  const auto missing = RouteSpeedProfile::Build(20.0, absent, limits);
  CHECK(!missing && missing.error() == RouteSpeedError::MissingSample,
        "missing geometry cannot be mistaken for a slow curve");
  const RouteSpeedProfile::Sampler degenerate =
      [](double stationM) -> std::optional<RouteCurveSample> {
    return RouteCurveSample{.PositionM = {{stationM, 0.0, 0.0}}, .Forward = {}};
  };
  const auto invalid = RouteSpeedProfile::Build(20.0, degenerate, limits);
  CHECK(!invalid && invalid.error() == RouteSpeedError::InvalidSample,
        "a zero tangent refuses the route instead of yielding a camera jump");
  return Report();
}
