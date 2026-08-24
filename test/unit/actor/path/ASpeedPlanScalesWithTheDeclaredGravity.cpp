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
  // board:1785: CrestsThatBound() counted BEFORE the three sweeps and over TWO populations
  // -- the sample loop and three evaluations per seam interval -- so it published 804
  // "stations" on an 801-station plan. BoundBy(Crest) answers the same question after the
  // last sweep, over one population, and the tally sums to SampleCount().
  Note("stations the crest bounds under that gravity",
       (double)onEarth.BoundBy(SpeedProfile::Held::Crest), "stations");
  Note("stations in the plan", (double)onEarth.SampleCount(), "stations");
  CHECK(onEarth.BoundBy(SpeedProfile::Held::Crest) <= onEarth.SampleCount(),
        "**A TALLY OF STATIONS DOES NOT EXCEED THE STATIONS THERE ARE**: the crest count is "
        "taken after the sweeps over one population, not before them over two (board:1785)");
  CHECK(onEarth.BoundBy(SpeedProfile::Held::Crest) > 0,
        "the crest is the binding term, not the engine or the tyres");
  CHECK_NEAR(onEarth.SlowestBound().Ms, std::sqrt(kEarthMs2 / kCrestPerM), 1.0e-9, "m/s",
             "**THE CREST SPEED IS sqrt(g / h'') AND NOTHING ELSE** -- the held speed reproduces "
             "the closed form from the declared gravity and the road's own vertical bend");

  SpeedProfile onMoon;
  const Envelope moon = Standing(kMoonMs2);
  CHECK(onMoon.Over(line, moon, kStepM, kEntryMs, error),
        "the SAME road plans again under a declared 1.62 m/s2 (measured, lunar surface mean) -- "
        "no code changes, only the declaration");
  CHECK(onMoon.BoundBy(SpeedProfile::Held::Crest) > 0,
        "the crest binds there too, at a quarter of the speed");
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

  {
    // the tail is PRICED at its true length: 20.5 m at step 5 leaves a 0.5 m last gap,
    // and the entry speed at 20 m must be what the declared brake can shed over 0.5 m --
    // the full-step price promised sqrt(v^2 + 2 b 5) where only 0.5 m exist
    ReferenceLine bent;
    CHECK(bent.Lay(Placed{}, {{Curve::Straight, 20.0, 0.0, 0.0},
                              {Curve::Spiral, 0.25, 0.0, 0.2},
                              {Curve::Arc, 0.25, 0.2, 0.2}},
                   error),
          "a 20.5 m line with its bend behind the partial step lays");
    SpeedProfile priced;
    const Envelope earthly = Standing(kEarthMs2);
    CHECK(priced.Over(bent, earthly, 5.0, kEntryMs, error), "and plans at step 5");
    const double bendMs = std::sqrt(earthly.Grip * kEarthMs2 / 0.2);
    const double honestMs = std::sqrt(bendMs * bendMs + 2.0 * earthly.BrakeMs2() * 0.5);
    Note("the entry speed the plan allows at 20 m", priced.At(20.0), "m/s");
    Note("what the brake can honestly shed over the real tail", honestMs, "m/s");
    CHECK(priced.At(20.0) <= honestMs + 1.0e-9,
          "**THE LAST INTERVAL IS AS LONG AS IT REALLY IS**: the backward pass prices the "
          "0.5 m tail at 0.5 m -- the full-step price allowed an entry the declared brake "
          "cannot land (board:1718)");
    const double justBefore = priced.At(20.5 - 1.0e-9);
    CHECK(std::fabs(justBefore - priced.At(20.5)) < 1.0e-3,
          "and At() is continuous at the plan's end -- the tail interpolates over its true "
          "length, no jump where the tick reads live");
  }

  // board:1830: Envelope::TopMs() is infinity in a declared vacuum -- legal since board:1627 --
  // and the histogram's span used to come from it, so every station fell in bin 0, Quantile
  // answered infinity and StationsUnder answered zero for any speed at all.
  {
    Envelope vacuum = Standing(kEarthMs2);
    vacuum.AirDensity = 0.0;
    SpeedProfile inVacuum;
    CHECK(inVacuum.Over(line, vacuum, kStepM, 30.0, error),
          "a plan is taken in a declared vacuum, which is legal and unbounded by drag");
    if (!error.empty()) { std::printf("REFUSED %s\n", error.c_str()); }

    const bool topIsInfinite = !(vacuum.TopMs() < 1.0e30);
    Note("the top speed drag would allow in vacuum", topIsInfinite ? 1.0 : 0.0, "infinite");
    Note("the fastest the plan actually holds", inVacuum.Fastest().Ms * 3.6, "km/h");
    Note("the resolution the histogram works at", inVacuum.BinMs() * 3.6, "km/h");
    Note("what it answers at p50", inVacuum.Quantile(0.5) * 3.6, "km/h");
    Note("stations it finds under the fastest", (double)inVacuum.StationsUnder(inVacuum.Fastest().Ms), "stations");

    CHECK(topIsInfinite, "and the declaration's own top speed is indeed unbounded there");
    CHECK(inVacuum.Fastest().Ms < 1.0e30 && inVacuum.Fastest().Ms > 0.0,
          "**THE PLAN STILL HOLDS A FINITE FASTEST STATION**, because the acceleration sweep "
          "bounds it from a finite entry however much drag allows");
    CHECK(inVacuum.BinMs() > 0.0 && inVacuum.BinMs() < 1.0e30,
          "**AND THE HISTOGRAM'S RESOLUTION COMES FROM WHAT THE PLAN HOLDS, NOT FROM WHAT DRAG "
          "WOULD ALLOW**: a span taken from an infinite top speed puts every station in bin 0, "
          "and the quantiles then answer infinity while StationsUnder answers zero for every "
          "speed there is (board:1830)");
    CHECK(inVacuum.Quantile(0.5) < 1.0e30,
          "so a median in vacuum is a speed and not an infinity");
    CHECK(inVacuum.StationsUnder(inVacuum.Fastest().Ms) > 0,
          "and stations under the fastest one are counted rather than answered as none");
  }

  Covers("I.9.3 the speed plan derives every gravity-borne bound -- cornering, holding, braking "
         "and the crest's sqrt(g/h'') -- from the world the scenario declares, so the same road "
         "and the same car plan differently on a different sphere with no engine change");
  return Report();
}
