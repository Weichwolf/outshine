#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "Drive.h"
#include "SpeedProfile.h"

using outshine::Curve;
using outshine::Envelope;
using outshine::Placed;
using outshine::ReferenceLine;
using outshine::Segment;
using outshine::SpeedProfile;
using outshine::Pilot::Axles;
using outshine::Pilot::Demand;
using outshine::Pilot::Drive;
using outshine::Pilot::Hold;
using outshine::Pilot::Locate;
using outshine::Pilot::Placement;
using outshine::Pilot::Reins;
using outshine::Pilot::Steering;
using outshine::Pilot::TightestPerM;

namespace {

constexpr double kWheelbaseM = 2.810;
constexpr double kSteerLimitRad = 0.55;
constexpr double kWindowM = 40.0;
constexpr double kDtS = 0.02;
constexpr double kLaneHalfM = 1.75;

std::vector<Segment> Road() {
  return {{Curve::Straight, 300.0, 0.0, 0.0},
          {Curve::Spiral, 80.0, 0.0, 1.0 / 120.0},
          {Curve::Arc, 150.0, 1.0 / 120.0, 1.0 / 120.0},
          {Curve::Spiral, 80.0, 1.0 / 120.0, 0.0},
          {Curve::Straight, 200.0, 0.0, 0.0},
          {Curve::Spiral, 60.0, 0.0, -1.0 / 90.0},
          {Curve::Arc, 120.0, -1.0 / 90.0, -1.0 / 90.0},
          {Curve::Spiral, 60.0, -1.0 / 90.0, 0.0},
          {Curve::Straight, 400.0, 0.0, 0.0}};
}

Envelope Synthetic() {
  Envelope out;
  out.GravityMs2 = 9.80665;
  out.Grip = 0.95;
  out.MassKg = 1610.0;
  out.DriveN = 400.0 * 3.08 / 0.333;
  out.BrakeN = 2200.0 / 0.333;
  out.DragArea = 0.66 * 2.19;
  out.AirDensity = 1.225;
  return out;
}

Axles Bicycle() {
  Axles out;
  out.WheelbaseM = kWheelbaseM;
  out.SteerLimitRad = kSteerLimitRad;
  return out;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const auto began = std::chrono::steady_clock::now();

  ReferenceLine line;
  std::string error;
  const Placed origin{};
  CHECK(line.Lay(origin, Road(), error), "a synthetic road lays from nine segments, in code, no tile");
  Note("how long the road is", line.LengthM() / 1000.0, "km");

  const Envelope envelope = Synthetic();
  SpeedProfile profile;
  CHECK(profile.Over(line, envelope, 0.5, 0.0, error),
        "the speed profile plans over it from the envelope alone");

  Placed start;
  CHECK(line.At(0.0, start), "the road answers at its start");
  double eastM = start.EastM, northM = start.NorthM, headingRad = start.HeadingRad;
  double speedMs = profile.At(0.0);
  double nearM = 0.0;
  double worstOffsetM = 0.0;
  double worstOverMs = 0.0;
  bool lost = false;
  long steps = 0;
  const Axles axles = Bicycle();
  for (; steps < 200000; ++steps) {
    const Placement at = Locate(line, eastM, northM, 0.0, headingRad, nearM, kWindowM);
    if (!at.Found) {
      lost = true;
      break;
    }
    nearM = at.AlongM;
    if (at.AlongM >= line.LengthM() - 5.0) { break; }
    if (std::fabs(at.OffsetM) > std::fabs(worstOffsetM)) { worstOffsetM = at.OffsetM; }
    const double brakeMs2 = envelope.BrakeN / envelope.MassKg;
    const double brakingM = speedMs * speedMs / (2.0 * brakeMs2);
    double plannedMs = profile.At(at.AlongM);
    for (int look = 1; look <= 12; ++look) {
      const double atM = std::fmin(at.AlongM + brakingM * (double)look / 12.0, line.LengthM());
      const double thereMs = profile.At(atM);
      if (thereMs < plannedMs) { plannedMs = thereMs; }
    }
    if (speedMs - profile.At(at.AlongM) > worstOverMs) {
      worstOverMs = speedMs - profile.At(at.AlongM);
    }

    Reins reins;
    reins.SettleS = 1.0;
    reins.LeastReachM = kWheelbaseM;
    reins.TightestPerM = TightestPerM(axles, envelope, speedMs);
    const Demand asked = Hold(line, reins, at, speedMs, plannedMs);
    const Steering command = Drive(axles, envelope, asked);

    const double dragN = 0.5 * envelope.AirDensity * envelope.DragArea * speedMs * speedMs;
    const double accelMs2 = (command.DriveN - command.BrakeN - dragN) / envelope.MassKg;
    speedMs += accelMs2 * kDtS;
    if (speedMs < 1.0) { speedMs = 1.0; }
    eastM += speedMs * std::cos(headingRad) * kDtS;
    northM += speedMs * std::sin(headingRad) * kDtS;
    headingRad += speedMs * std::tan(command.SteerRad) / kWheelbaseM * kDtS;
  }

  const double tookMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();

  Note("how far the ride reached", nearM / 1000.0, "km");
  Note("worst offset from the centreline", std::fabs(worstOffsetM), "m");
  Note("worst speed over the plan", worstOverMs, "m/s");
  Note("steps integrated", (double)steps, "steps");
  Note("wall time for lay, plan and ride", tookMs, "ms");

  CHECK(!lost, "**THE RIDE NEVER LOSES THE ROAD**: resection holds through every curve");
  CHECK(nearM >= line.LengthM() - 6.0, "and it ARRIVES -- the whole synthetic road is ridden");
  CHECK(std::fabs(worstOffsetM) < kLaneHalfM,
        "**THE LINE IS HELD INSIDE THE LANE** over spirals both ways -- the regression this "
        "gate exists to catch is the one that pushes this number over the kerb");
  CHECK(worstOverMs < 2.0,
        "the speed never runs materially past the plan the profile derived");
  CHECK(tookMs < 2000.0,
        "**AND THE GATE IS FAST** (board:1601): the whole lay-plan-ride is milliseconds, not "
        "the long drive's minutes -- tests fast = engine fast. When board:1581 extracts the "
        "drive system, this becomes its test verbatim; until then it guards the bricks at the "
        "same seam Ride uses");

  Covers("II.11 the drive's integration is guarded by the fast gate: a synthetic road laid in "
         "code is planned and ridden to arrival in milliseconds, holding lane and plan");
  return Report();
}
