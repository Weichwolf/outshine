#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "ReferenceLine.h"

using outshine::Curve;
using outshine::Knot;
using outshine::Placed;
using outshine::ReferenceLine;
using outshine::Segment;

namespace {

constexpr double kGravityMs2 = 9.80665;
constexpr double kRadiusM = 400.0;
constexpr double kCurvature = 1.0 / kRadiusM;
constexpr double kEnterM = 150.0;
constexpr double kSpiralM = 120.0;
constexpr double kArcM = 300.0;
constexpr double kLeaveM = 150.0;
constexpr double kGrade = 0.04;
constexpr double kCrestM = 6.0;
constexpr double kBankRad = 0.06;
constexpr double kStepM = 0.5;

std::vector<Segment> Road() {
  return {{Curve::Straight, kEnterM, 0.0, 0.0},
          {Curve::Spiral, kSpiralM, 0.0, kCurvature},
          {Curve::Arc, kArcM, kCurvature, kCurvature},
          {Curve::Spiral, kSpiralM, kCurvature, 0.0},
          {Curve::Straight, kLeaveM, 0.0, 0.0}};
}

double HeightAt(const ReferenceLine &line, double alongM) {
  Placed out;
  return line.At(alongM, out) ? out.HeightM : 0.0;
}

double SlopeAt(const ReferenceLine &line, double alongM) {
  Placed out;
  return line.At(alongM, out) ? out.Slope : 0.0;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  ReferenceLine line;
  std::string error;
  const bool laid = line.Lay(Placed{}, Road(), error);
  if (!laid) { std::printf("REFUSED %s\n", error.c_str()); }
  CHECK(laid, "a line is laid in plan before anything is fastened to it");
  if (!laid) { return Report(); }

  const double lengthM = line.LengthM();
  const double summitM = 0.5 * lengthM;

  const std::vector<Knot> crest = {{0.0, 0.0, kGrade},
                                   {summitM, kCrestM, 0.0},
                                   {lengthM, 0.0, -kGrade}};
  const bool rose = line.Rise(crest, error);
  if (!rose) { std::printf("REFUSED %s\n", error.c_str()); }
  CHECK(rose, "**A ROAD RISES AND THAT IS A SECOND PROFILE, NOT A THIRD DIMENSION OF THE FIRST.** "
              "The plan view answers where the line goes; the height answers how far it has climbed "
              "getting there, and the two are declared apart because a survey knows them apart -- "
              "OpenDRIVE says the same thing with `<elevation>` over `<geometry>`");
  if (!rose) { return Report(); }

  CHECK(line.RiseKnotCount() == crest.size(), "and it holds the knots it was given");

  for (const Knot &knot : crest) {
    Placed out;
    const bool there = line.At(knot.AlongM, out);
    CHECK(there && std::fabs(out.HeightM - knot.Value) < 1e-9,
          "**THE PROFILE PASSES THROUGH ITS KNOTS.** A survey height is a measurement and an "
          "interpolant that misses it has replaced the measurement with a smoother opinion of it");
    CHECK(there && std::fabs(out.Slope - knot.RatePerM) < 1e-9,
          "and it leaves each knot at the slope the knot declares, so the grade is stated rather "
          "than inferred from the neighbours");
  }

  double worstHeight = 0.0, worstSlope = 0.0;
  double worstSlopeStep = 0.0;
  double bendMax = 0.0;
  const double dM = 0.01;
  for (double alongM = dM; alongM + dM <= lengthM; alongM += kStepM) {
    Placed here;
    if (!line.At(alongM, here)) { continue; }
    const double bySlope = (HeightAt(line, alongM + dM) - HeightAt(line, alongM - dM)) / (2.0 * dM);
    const double byBend = (SlopeAt(line, alongM + dM) - SlopeAt(line, alongM - dM)) / (2.0 * dM);
    worstHeight = std::fmax(worstHeight, std::fabs(bySlope - here.Slope));
    worstSlope = std::fmax(worstSlope, std::fabs(byBend - here.SlopeRatePerM));
    bendMax = std::fmax(bendMax, std::fabs(here.SlopeRatePerM));

    Placed before;
    if (line.At(alongM - kStepM, before)) {
      worstSlopeStep = std::fmax(worstSlopeStep, std::fabs(here.Slope - before.Slope));
    }
  }

  Note("worst disagreement between the published slope and the height's own derivative",
       worstHeight, "m/m");
  Note("worst disagreement between the published bend and the slope's own derivative", worstSlope,
       "1/m");
  CHECK(worstHeight < 1e-6,
        "**THE SLOPE IS THE HEIGHT'S DERIVATIVE AND NOT A SECOND OPINION OF IT.** Differencing the "
        "published height over 0.02 m reproduces the published slope, so a vehicle reading one and "
        "a planner reading the other are reading one road");
  CHECK(worstSlope < 1e-6,
        "and the published bend is the slope's derivative on the same evidence -- which is what "
        "lets a vertical acceleration be answered before the car is anywhere near the crest");

  Note("worst grade step between two stations 0.5 m apart", worstSlopeStep, "m/m");
  CHECK(worstSlopeStep < 4.0e-4,
        "**A GRADE BREAK IS A STEP IN THE VERTICAL FORCE, exactly as a curvature leap is a step in "
        "the lateral one.** The cubic through height and slope carries the transition without one, "
        "and the residual here is the 0.5 m sampling of a curve that genuinely bends");

  Note("the sharpest vertical bend on this road", bendMax, "1/m");
  const double airborneMs = std::sqrt(kGravityMs2 / bendMax);
  Note("the speed at which that crest throws a car off the road", airborneMs, "m/s");
  Note("the same speed", airborneMs * 3.6, "km/h");
  CHECK(airborneMs > 100.0,
        "**AND THIS IS THE NEGATIVE CONTROL'S WHOLE POINT.** A crest launches a car when v^2 times "
        "the vertical bend exceeds gravity. On a road built this way that speed is far beyond "
        "anything the speed profile will ask for -- so when the drive suite reads a wheel leaving "
        "the ground, the road did not do it, and the finding is the instrument or the vehicle");

  const std::vector<Knot> bank = {{kEnterM, 0.0, 0.0},
                                  {kEnterM + kSpiralM, kBankRad, 0.0},
                                  {kEnterM + kSpiralM + kArcM, kBankRad, 0.0},
                                  {kEnterM + 2.0 * kSpiralM + kArcM, 0.0, 0.0}};
  const bool banked = line.Bank(bank, error);
  if (!banked) { std::printf("REFUSED %s\n", error.c_str()); }
  CHECK(banked, "the same mechanism carries the cross-slope, because a bank is a value and a rate "
                "along a station like a height is -- one interpolant, two profiles, and the road "
                "part it names lives at the call and not in the machinery");
  if (!banked) { return Report(); }

  Placed middle;
  const bool inCurve = line.At(kEnterM + kSpiralM + 0.5 * kArcM, middle);
  CHECK(inCurve && std::fabs(middle.BankRad - kBankRad) < 1e-9,
        "the arc is fully banked in its middle");
  CHECK(inCurve && std::fabs(middle.CurvaturePerM - kCurvature) < 1e-12,
        "and the plan curvature there is untouched by the bank, which is what declaring them apart "
        "buys");

  const double balancedMs = std::sqrt(kGravityMs2 * std::tan(kBankRad) / kCurvature);
  Note("the speed this bank carries with no help from the tyres", balancedMs, "m/s");
  Note("the same speed", balancedMs * 3.6, "km/h");
  CHECK(balancedMs > 10.0 && balancedMs < 40.0,
        "**A BANK IS A SPEED.** g tan(theta) / kappa is the speed at which the curve needs no "
        "lateral friction at all, so the superelevation a road carries is a statement about how "
        "fast it was built to be driven -- and the autopilot may read it rather than guess");

  Placed entering;
  const bool flat = line.At(0.5 * kEnterM, entering);
  CHECK(flat && std::fabs(entering.BankRad) < 1e-12,
        "the straight approaching it is flat, and a station before the first knot extrapolates "
        "along that knot's own rate rather than leaping to it");

  ReferenceLine second;
  std::string refusal;
  CHECK(!second.Rise({{0.0, 0.0, 0.0}}, refusal),
        "a profile is refused before the line it belongs to is laid");
  std::printf("REFUSAL %s\n", refusal.c_str());

  CHECK(!line.Rise({{0.0, 0.0, 0.0}, {lengthM + 1.0, 0.0, 0.0}}, refusal),
        "and a knot past the end of the line is refused rather than clamped, because a clamp turns "
        "a survey that does not fit into a road that does");
  std::printf("REFUSAL %s\n", refusal.c_str());

  CHECK(!line.Rise({{100.0, 0.0, 0.0}, {100.0, 5.0, 0.0}}, refusal),
        "and two knots at one station are refused, because a line reaches a station once and one "
        "station carries one height");
  std::printf("REFUSAL %s\n", refusal.c_str());

  std::vector<Knot> flatRoad;
  CHECK(line.Rise(flatRoad, refusal) && line.RiseKnotCount() == 0 && HeightAt(line, summitM) == 0.0,
        "and NO knots is a road that is level rather than a refusal -- the shape is 0 or 1..N, and "
        "0 is the flat road every negative control starts from");

  Covers("I.9.2 a reference line carries a height and a cross-slope as profiles fastened to it: "
         "each passes through its knots, publishes a slope that is its own derivative and a bend "
         "that is the slope's, and carries every transition without a grade break -- which is what "
         "makes a vertical acceleration answerable from the declaration alone");
  return Report();
}
