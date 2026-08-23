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

constexpr double kRoadM = 400.0;
constexpr double kCrestAtM = 205.0;
constexpr double kCrestHalfM = 5.0;
constexpr double kCrestRiseM = 2.0;
constexpr double kStepM = 20.0;
constexpr double kGravityMs2 = 9.80665;

Envelope Standing() {
  Envelope out;
  out.Grip = 0.95;
  out.GravityMs2 = kGravityMs2;
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

  // the rise carries a SHORT crest whose knots fall BETWEEN the profile's stations: at a
  // 20 m step the stations are 200 and 220, and the crest lives at 200..210 with its
  // sharpest bend at 205. Nothing about this is exotic -- elevation knots come from the DEM
  // along the route and owe the profile's step nothing.
  const std::vector<Knot> over = {{0.0, 0.0, 0.0},
                                  {kCrestAtM - kCrestHalfM, 0.0, 0.0},
                                  {kCrestAtM, kCrestRiseM, 0.0},
                                  {kCrestAtM + kCrestHalfM, 0.0, 0.0},
                                  {kRoadM, 0.0, 0.0}};
  CHECK(line.Rise(over, error), "and rises over a short crest");
  if (!error.empty()) { std::printf("REFUSED %s\n", error.c_str()); }

  double sharpest = 0.0;
  double sharpestAtM = 0.0;
  for (double atM = 0.0; atM <= kRoadM; atM += 0.25) {
    Placed here;
    if (!line.At(atM, here)) { continue; }
    if (-here.SlopeRatePerM > sharpest) {
      sharpest = -here.SlopeRatePerM;
      sharpestAtM = atM;
    }
  }
  Note("the sharpest crest the line actually carries", sharpest, "per m");
  Note("where it is", sharpestAtM, "m");
  const double flyingMs = sharpest > 0.0 ? std::sqrt(kGravityMs2 / sharpest) : 0.0;
  Note("the fastest a wheel stays down over it", flyingMs * 3.6, "km/h");
  CHECK(sharpest > 0.0, "the line carries a crest to be bounded");

  SpeedProfile plan;
  CHECK(plan.Over(line, Standing(), kStepM, 0.0, error), "a plan is taken over it");
  if (!error.empty()) { std::printf("REFUSED %s\n", error.c_str()); }

  double fastestOverCrest = 0.0;
  for (double atM = kCrestAtM - kCrestHalfM; atM <= kCrestAtM + kCrestHalfM; atM += 0.25) {
    fastestOverCrest = std::fmax(fastestOverCrest, plan.At(atM));
  }
  Note("what the plan allows over the crest", fastestOverCrest * 3.6, "km/h");
  Note("stations the step puts near it", kStepM, "m apart");

  CHECK(fastestOverCrest <= flyingMs,
        "**A CREST BETWEEN TWO STATIONS IS STILL A CREST**: the plan may not allow a speed "
        "that lifts the wheels off the road, and a term sampled at stations is a statement "
        "about the stations -- never about the road between them (board:1767)");

  Covers("V.8 the speed plan bounds every crest the reference line carries, including the "
         "ones whose elevation knots fall between the plan's own stations (board:1767)");
  return Report();
}
