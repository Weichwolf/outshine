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

    // board:1785, sharpened: Slowest() is honest and USELESS in production, because both
    // callsites hand entryMs = 0 and the slowest station is then always the start, held by
    // 'entry'. The question the item was filed for -- "12.158 km/h at km 552.939, which
    // physics?" -- needs the slowest station the ROAD holds, not the one the standing start
    // holds.
    const SpeedProfile::Standing road = plan.SlowestBound();
    std::printf("NOTE the slowest station the road holds = %.3f km/h at %.1f m, by '%s'\n",
                road.Ms * 3.6, road.AtM, SpeedProfile::NameOf(road.By));
    // named, not derived from IsGeometry: this road is a straight, an easement and an arc,
    // and the arc is the only thing on it that a ROAD imposes. Asserting through IsGeometry
    // would let a term move into that set and keep the claim green -- oracle and defect from
    // one source, which is how the first version of this item shipped wrong.
    CHECK(road.By == SpeedProfile::Held::Curvature,
          "**AND THE PLAN NAMES THE SLOWEST STATION THE ROAD ITSELF HOLDS**: entry, traction "
          "and braking are facts about the DRIVE -- how fast it arrived, how hard it can "
          "pull, what it must shed for what comes next -- while curvature, slip, ramp, climb "
          "and crest are facts about the ROAD, and only the second kind answers 'why is this "
          "corridor slow here' (board:1785)");
    CHECK_NEAR(road.AtM, kStraightM + kEaseM, 1.0e-9, "m",
               "at the station where the easement hands over to the arc, which is where this "
               "road is tightest");
    CHECK(road.Ms >= slowest.Ms,
          "and it is at or above the plan's outright minimum, because it is that minimum "
          "with the driver's own arrival excluded");

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

  // board:1785's residue, closed here: `entryMs` is a degree of freedom that BOTH callers in
  // src/ pass as 0.0 -- the corridor is laid once, for the whole route, and the car starts at
  // Marienplatz from rest, which is correct. A parameter with one value everywhere is a
  // parameter nobody has ever exercised, and the corridor relay the driver's TARGET asks for
  // is exactly the caller that will pass it something else.
  {
    SpeedProfile rolling;
    std::string why;
    CHECK(rolling.Over(line, f31, kStepM, 30.0, why), "the same road plans from a rolling start");
    SpeedProfile stopped;
    CHECK(stopped.Over(line, f31, kStepM, 0.0, why), "and from rest, for comparison");

    Note("the plan's first station from rest", stopped.SampleAt(0) * 3.6, "km/h");
    Note("the plan's first station rolling at 30 m/s", rolling.SampleAt(0) * 3.6, "km/h");
    CHECK_NEAR(stopped.SampleAt(0), 0.0, 1.0e-12, "m/s",
               "a plan from rest starts at rest, which is what both callers in src/ ask for "
               "today");
    CHECK_NEAR(rolling.SampleAt(0), 30.0, 1.0e-12, "m/s",
               "**AND A PLAN HANDED A ROLLING START BEGINS AT IT**: the entry speed is the "
               "only way a corridor laid AHEAD of a moving car can be planned without telling "
               "it to stop first, and until now no caller passed anything but zero -- so the "
               "degree of freedom existed and had never been driven (board:1785)");

    // and the difference must fade: a rolling start buys distance, not a different road. Where
    // the geometry binds, both plans agree.
    size_t agree = 0, apart = 0;
    double lastApartM = 0.0;
    for (size_t at = 0; at < stopped.SampleCount(); ++at) {
      const double gap = std::fabs(rolling.SampleAt(at) - stopped.SampleAt(at));
      if (gap < 1.0e-9) {
        ++agree;
      } else {
        ++apart;
        lastApartM = (double)at * kStepM;
      }
    }
    Note("stations where the two plans agree", (double)agree, "stations");
    Note("stations where they differ", (double)apart, "stations");
    Note("the last station they differ at", lastApartM, "m");
    CHECK(agree > apart,
          "and the two plans agree over most of the road, because a rolling start changes how "
          "the first stations are reached and not what the geometry allows");
    CHECK(lastApartM < kStraightM,
          "with the difference gone before the bend -- the road decides the bend's speed, and "
          "how the car arrived at the straight does not reach it");
  }

  // board:1785, the reviewer's box 4: with entryMs = 0 the minimum is station 0 by
  // definition, so the Slowest() arm above holds no matter what the three sweeps write into
  // Why_[] for stations 1..n. Entering ABOVE every limit the road carries puts the minimum in
  // the bend, where only the sweeps can have put it.
  {
    SpeedProfile fast;
    const Envelope f31Again = Standing();
    CHECK(fast.Over(line, f31Again, kStepM, f31Again.TopMs(), error),
          "the same road is planned by a car that arrives at its own top speed");
    if (!error.empty()) { std::printf("REFUSED %s\n", error.c_str()); }

    double leastMs = fast.SampleAt(0);
    size_t leastAt = 0;
    for (size_t at = 1; at < fast.SampleCount(); ++at) {
      if (fast.SampleAt(at) < leastMs) {
        leastMs = fast.SampleAt(at);
        leastAt = at;
      }
    }
    const SpeedProfile::Standing slowest = fast.Slowest();
    std::printf("NOTE entering at top speed the slowest station = %.3f km/h at %.1f m by '%s'\n",
                slowest.Ms * 3.6, slowest.AtM, SpeedProfile::NameOf(slowest.By));
    Note("the slowest sample that plan holds", leastMs * 3.6, "km/h");
    Note("where it is", (double)leastAt * fast.StepM(), "m");
    Note("where the road stops being straight", kStraightM, "m");

    CHECK(leastAt > 0,
          "**A CAR THAT ARRIVES AT SPEED HAS ITS SLOWEST STATION SOMEWHERE ELSE THAN THE "
          "START** -- at entryMs = 0 the minimum is station 0 whatever the sweeps do, so that "
          "arm proves nothing about them (board:1785)");
    CHECK_NEAR(slowest.Ms, leastMs, 1.0e-12, "m/s",
               "and the published slowest station is that minimum, found after the seam clamp "
               "and both sweeps rather than inside the sampling loop");
    CHECK_NEAR(slowest.AtM, (double)leastAt * fast.StepM(), 1.0e-9, "m",
               "at the station it stands at");
    CHECK(slowest.AtM >= kStraightM,
          "**AND IT IS IN THE BEND, WHICH IS WHERE THE ROAD IS SLOW** -- the straight "
          "kilometre before it is only slow on its approach, and that approach is the "
          "backward brake sweep the first arm could not see");
    CHECK(slowest.By != SpeedProfile::Held::Entry &&
              slowest.By != SpeedProfile::Held::Free,
          "and the term that holds it is neither the entry it did not use nor free");
  }

  Covers("V.9 a seam bound stands at the station it binds: the straight before a bend is "
         "planned at the speed the brake allows, not at the bend's own limit (board:1773)");
  return Report();
}
