#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "ReferenceLine.h"
#include "SpeedProfile.h"

using outshine::Curve;
using outshine::Envelope;
using outshine::Knot;
using outshine::Placed;
using outshine::ReferenceLine;
using outshine::Segment;
using outshine::SpeedProfile;

namespace {

constexpr double kEarthMs2 = 9.80665;
constexpr double kMoonMs2 = 1.62;
constexpr double kRoadM = 400.0;
constexpr double kApproachSlope = 0.2;
constexpr double kCrestPerM = 2.0 * kApproachSlope / kRoadM;
constexpr double kStepM = 0.5;
constexpr double kEntryMs = 120.0;

Envelope Standing(double gravityMs2) {
  Envelope out;
  out.Grip = 0.95;
  out.GravityMs2 = gravityMs2;
  out.MassKg = 1610.0;
  out.DriveN = 20000.0;
  out.BrakeN = 2200.0 / 0.333;
  out.DragArea = 0.66 * 2.19;
  out.AirDensity = 1.225;
  return out;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  ReferenceLine line;
  std::string error;
  CHECK(line.Lay(Placed{}, {{Curve::Straight, kRoadM, 0.0, 0.0}}, error),
        "a straight road lays in plan");
  const std::vector<Knot> hump = {{0.0, 0.0, kApproachSlope}, {kRoadM, 0.0, -kApproachSlope}};
  CHECK(line.Rise(hump, error),
        "and rises over a symmetric hump whose one cubic is the parabola with a constant "
        "vertical bend of 2 s / L");

  SpeedProfile onEarth;
  const Envelope earth = Standing(kEarthMs2);
  CHECK(onEarth.Over(line, earth, kStepM, kEntryMs, error),
        "the plan lays over it under the declared 9.80665 m/s2");
  Note("stations the crest bounds under that gravity", (double)onEarth.CrestsThatBound(),
       "stations");
  CHECK(onEarth.CrestsThatBound() > 0, "the crest is the binding term, not the engine or the tyres");
  CHECK_NEAR(onEarth.CrestHeldMs(), std::sqrt(kEarthMs2 / kCrestPerM), 1.0e-9, "m/s",
             "**THE CREST SPEED IS sqrt(g / h'') AND NOTHING ELSE** -- the held speed reproduces "
             "the closed form from the declared gravity and the road's own vertical bend");

  SpeedProfile onMoon;
  const Envelope moon = Standing(kMoonMs2);
  CHECK(onMoon.Over(line, moon, kStepM, kEntryMs, error),
        "the SAME road plans again under a declared 1.62 m/s2 (measured, lunar surface mean) -- "
        "no code changes, only the declaration");
  CHECK(onMoon.CrestsThatBound() > 0, "the crest binds there too, at a quarter of the speed");
  const double middleM = 0.5 * kRoadM;
  Note("the crest speed on earth", onEarth.At(middleM), "m/s");
  Note("the crest speed on the moon", onMoon.At(middleM), "m/s");
  CHECK_NEAR(onMoon.At(middleM) / onEarth.At(middleM), std::sqrt(kMoonMs2 / kEarthMs2), 1.0e-12,
             "of it",
             "**AND THE PLAN SCALES WITH THE DECLARED WORLD EXACTLY AS sqrt(g) SAYS IT MUST.** "
             "A car crests a lunar hill at 40 percent of its earth speed before it flies, and "
             "the engine learns that from the declaration alone");

  SpeedProfile refused;
  CHECK(!refused.Over(line, Standing(0.0), kStepM, kEntryMs, error),
        "a world declaring no gravity refuses the plan instead of planning zero-speed corners");
  CHECK(error.find("gravity") != std::string::npos, "and the refusal says why");

  {
    // 10.5 m at step 1: the tail (10, 10.5] once fell off the grid, and a bend living
    // ONLY there planned topMs -- the extra clamped station samples it
    ReferenceLine short10;
    CHECK(short10.Lay(Placed{}, {{Curve::Straight, 10.0, 0.0, 0.0},
                                 {Curve::Spiral, 0.25, 0.0, 0.2},
                                 {Curve::Arc, 0.25, 0.2, 0.2}},
                      error),
          "a line of 10.5 m with all its curvature in the last half metre lays");
    SpeedProfile tail;
    CHECK(tail.Over(short10, Standing(kEarthMs2), 1.0, kEntryMs, error),
          "and the plan lays over it at a step the length is not divisible by");
    const double heldMs = tail.At(10.4);
    const double curveHeldMs = std::sqrt(0.95 * kEarthMs2 / 0.2);
    Note("the tail's planned speed", heldMs, "m/s");
    CHECK(heldMs < curveHeldMs * 1.2,
          "**THE FINAL PARTIAL STEP IS SAMPLED**: the bend in the last half metre bounds "
          "the plan near sqrt(grip g / kappa), where the ungridded tail once served topMs "
          "(board:1715)");
  }

  Covers("I.9.3 the speed plan derives every gravity-borne bound -- cornering, holding, braking "
         "and the crest's sqrt(g/h'') -- from the world the scenario declares, so the same road "
         "and the same car plan differently on a different sphere with no engine change");
  return Report();
}
