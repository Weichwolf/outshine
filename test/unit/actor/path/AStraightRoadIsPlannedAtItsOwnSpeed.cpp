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

constexpr double kStraightM = 1000.0;
constexpr double kEaseM = 500.0;
constexpr double kArcM = 300.0;
constexpr double kBendPerM = 0.01;
constexpr double kStepM = 20.0;

Envelope Standing() {
  Envelope out;
  out.Grip = 0.95;
  out.GravityMs2 = 9.80665;
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
  CHECK(line.Lay(Placed{},
                 {{Curve::Straight, kStraightM, 0.0, 0.0},
                  {Curve::Spiral, kEaseM, 0.0, kBendPerM},
                  {Curve::Arc, kArcM, kBendPerM, kBendPerM}},
                 error),
        "a straight kilometre eases into a bend and holds it");
  if (!error.empty()) { std::printf("REFUSED %s\n", error.c_str()); }

  SpeedProfile plan;
  const Envelope f31 = Standing();
  CHECK(plan.Over(line, f31, kStepM, 0.0, error), "a plan is taken over it");
  if (!error.empty()) { std::printf("REFUSED %s\n", error.c_str()); }

  // board:1773: the seam bound was stamped over every station the seam interval touched, so
  // the bend at 1500 m flattened the straight kilometre in front of it. The road at 1000 m
  // is straight, level and unbanked; the ONLY thing that may hold the car back there is the
  // braking it owes the bend ahead -- which the profile's own backward pass computes.
  Placed atEnd;
  CHECK(line.At(kStraightM, atEnd), "the line places the end of the straight");
  Note("curvature where the straight ends", atEnd.CurvaturePerM, "per m");
  Note("slope there", atEnd.Slope, "");
  CHECK(atEnd.CurvaturePerM == 0.0 && atEnd.Slope == 0.0,
        "the end of the straight is straight and level, by the declaration");

  const double heldMs = plan.At(kStraightM);
  const double inTheBendMs = plan.At(kStraightM + kEaseM + 0.5 * kArcM);
  Note("what the plan allows where the straight ends", heldMs * 3.6, "km/h");
  Note("what it allows in the bend", inTheBendMs * 3.6, "km/h");

  // the SAME straight with nothing after it: whatever the car may do at 1000 m with no bend
  // to brake for is the ceiling, and it is derived, never assumed.
  ReferenceLine alone;
  CHECK(alone.Lay(Placed{}, {{Curve::Straight, kStraightM, 0.0, 0.0}}, error),
        "the straight lays on its own");
  SpeedProfile free;
  CHECK(free.Over(alone, f31, kStepM, 0.0, error), "and plans on its own");
  const double freeMs = free.At(kStraightM);
  Note("what the same straight allows with no bend after it", freeMs * 3.6, "km/h");

  // the ONLY honest reason to be slower than that is the braking the bend ahead owes: from
  // v at 1000 m, full braking must still reach every later station's own limit.
  const double brakeMs2 = f31.BrakeMs2();
  double owedMs = freeMs;
  for (double atM = kStraightM; atM <= line.LengthM(); atM += kStepM) {
    const double there = plan.At(atM);
    const double from = std::sqrt(there * there + 2.0 * brakeMs2 * (atM - kStraightM));
    if (from < owedMs) { owedMs = from; }
  }
  Note("the fastest the brake can still shed every later limit from", owedMs * 3.6, "km/h");

  CHECK_NEAR(heldMs, owedMs, 1.0e-9, "m/s",
             "**A STRAIGHT ROAD IS PLANNED AT ITS OWN SPEED**: where the road is straight "
             "and level, the plan holds the car back by EXACTLY what the brake owes the "
             "bend ahead and by nothing else -- a bound that belongs to a seam belongs at "
             "the station it binds, never smeared over every station the seam interval "
             "touches (board:1773)");
  CHECK(inTheBendMs < heldMs,
        "and the bend IS slower than the straight, so this is not a plan that lost its "
        "bounds altogether");

  // board:1785: the plan published its crest term and nothing else, so the four other bounds
  // held the car back without saying so. Finding the corridor's own 5.6 m radius (board:1784)
  // took a probe built by hand -- the plan should have named it.
  {
    const SpeedProfile::Standing slowest = plan.Slowest();
    std::printf("NOTE the slowest station = %.3f km/h at %.1f m, held by '%s'\n",
                slowest.Ms * 3.6, slowest.AtM, SpeedProfile::NameOf(slowest.By));
    for (size_t at = 0; at < (size_t)SpeedProfile::Held::kCount; ++at) {
      const SpeedProfile::Held term = (SpeedProfile::Held)at;
      std::printf("NOTE stations held by '%s' = %zu\n", SpeedProfile::NameOf(term),
                  plan.BoundBy(term));
    }

    CHECK(slowest.By == SpeedProfile::Held::Curvature,
          "**THE PLAN NAMES THE TERM THAT HOLDS ITS SLOWEST STATION**: this road's slowest "
          "point is in the arc, so curvature is what holds it -- not the crest, which was "
          "the only term the plan could speak of (board:1785)");
    CHECK_NEAR(slowest.Ms, inTheBendMs, 1.0e-9, "m/s",
               "and the station it names is the one the plan actually answers slowest at");
    CHECK(slowest.AtM >= kStraightM,
          "which is in the bend, not on the straight kilometre before it");

    size_t counted = 0;
    for (size_t at = 0; at < (size_t)SpeedProfile::Held::kCount; ++at) {
      counted += plan.BoundBy((SpeedProfile::Held)at);
    }
    CHECK(counted == plan.SampleCount(),
          "and every station is accounted to exactly one term -- a tally that does not add "
          "up to the plan is a tally of something else");
    CHECK(plan.BoundBy(SpeedProfile::Held::Free) > 0 &&
              plan.BoundBy(SpeedProfile::Held::Curvature) > 0,
          "with both a free stretch and a bound one, so the tally distinguishes them");
  }

  Covers("V.9 a seam bound stands at the station it binds: the straight before a bend is "
         "planned at the speed the brake allows, not at the bend's own limit (board:1773)");
  return Report();
}
