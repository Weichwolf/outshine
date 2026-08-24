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

    // board:1785, reopened: the first version of this arm asserted against the analytically
    // recomputed cornering limit -- the same source the defect came from -- so it passed
    // while the published telemetry described a value three sweeps out of date. The plan's
    // OWN samples are the oracle now: whatever Slowest() says must be the minimum of what
    // the plan actually answers.
    double leastMs = plan.SampleAt(0);
    size_t leastAt = 0;
    for (size_t at = 1; at < plan.SampleCount(); ++at) {
      if (plan.SampleAt(at) < leastMs) {
        leastMs = plan.SampleAt(at);
        leastAt = at;
      }
    }
    Note("the slowest sample the plan holds", leastMs * 3.6, "km/h");
    Note("where that sample is", (double)leastAt * plan.StepM(), "m");
    CHECK_NEAR(slowest.Ms, leastMs, 1.0e-12, "m/s",
               "**THE PLAN'S SLOWEST STATION IS THE PLAN'S OWN MINIMUM**: the telemetry "
               "describes the finished plan and not an intermediate one -- Over() lowers "
               "Held_ three more times after the sampling loop, and a figure taken before "
               "those sweeps is a figure about a plan that was never used (board:1785)");
    CHECK_NEAR(slowest.AtM, (double)leastAt * plan.StepM(), 1.0e-9, "m",
               "and it names the station that minimum stands at");
    CHECK(slowest.By != SpeedProfile::Held::Free,
          "and a station the plan holds below its own top speed is never reported free");

    size_t counted = 0;
    for (size_t at = 0; at < (size_t)SpeedProfile::Held::kCount; ++at) {
      counted += plan.BoundBy((SpeedProfile::Held)at);
    }
    CHECK(counted == plan.SampleCount(),
          "and every station is accounted to exactly one term -- a tally that does not add "
          "up to the plan is a tally of something else");
    // and the tally must agree with the plan station by station: a station below topMs is
    // held by SOMETHING, and one at topMs is free. The old tally counted 52 free stations on
    // a plan whose every station was below its top speed.
    size_t belowTop = 0;
    for (size_t at = 0; at < plan.SampleCount(); ++at) {
      belowTop += plan.SampleAt(at) < f31.TopMs() ? 1 : 0;
    }
    Note("stations the plan holds below its top speed", (double)belowTop, "stations");
    Note("stations the tally calls free", (double)plan.BoundBy(SpeedProfile::Held::Free),
         "stations");
    CHECK(plan.SampleCount() - plan.BoundBy(SpeedProfile::Held::Free) == belowTop,
          "**AND THE TALLY AGREES WITH THE PLAN STATION BY STATION**: every station below "
          "the top speed is held by a named term, and every station at it is free -- a tally "
          "that sums correctly while describing another plan sums nothing (board:1785)");
  }

  Covers("V.9 a seam bound stands at the station it binds: the straight before a bend is "
         "planned at the speed the brake allows, not at the bend's own limit (board:1773)");
  return Report();
}
