#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "ReferenceLine.h"

using outshine::Curve;
using outshine::Placed;
using outshine::ReferenceLine;
using outshine::Segment;

namespace {

constexpr double kRadiusM = 400.0;
constexpr double kCurvature = 1.0 / kRadiusM;
constexpr double kStraightM = 200.0;
constexpr double kSpiralM = 120.0;
constexpr double kArcM = 300.0;

std::vector<Segment> WithSpirals() {
  return {{Curve::Straight, kStraightM, 0.0, 0.0},
          {Curve::Spiral, kSpiralM, 0.0, kCurvature},
          {Curve::Arc, kArcM, kCurvature, kCurvature},
          {Curve::Spiral, kSpiralM, kCurvature, 0.0},
          {Curve::Straight, kStraightM, 0.0, 0.0}};
}

std::vector<Segment> WithoutSpirals() {
  return {{Curve::Straight, kStraightM, 0.0, 0.0},
          {Curve::Arc, kArcM, kCurvature, kCurvature},
          {Curve::Straight, kStraightM, 0.0, 0.0}};
}

double WorstStep(const ReferenceLine &line, double (*of)(const Placed &), double stepM) {
  double worst = 0.0;
  Placed before;
  bool have = false;
  for (double at = 0.0; at <= line.LengthM(); at += stepM) {
    Placed here;
    if (!line.At(at, here)) { continue; }
    if (have) { worst = std::fmax(worst, std::fabs(of(here) - of(before))); }
    before = here;
    have = true;
  }
  return worst;
}

double CurvatureOf(const Placed &at) { return at.CurvaturePerM; }
double HeadingOf(const Placed &at) { return at.HeadingRad; }

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const Placed origin{0.0, 0.0, 0.0, 0.0};
  std::string why;

  ReferenceLine smooth;
  const bool laid = smooth.Lay(origin, WithSpirals(), why);
  if (!laid) { std::printf("REFUSED %s\n", why.c_str()); }
  CHECK(laid, "a line, a spiral, an arc, a spiral and a line lay as one reference line");
  if (!laid) { return Report(); }

  CHECK_NEAR(smooth.LengthM(), 2 * kStraightM + 2 * kSpiralM + kArcM, 1e-12, "m",
             "and its length is the sum of its segments");

  constexpr double kStepM = 0.1;
  const double curvatureStep = WorstStep(smooth, CurvatureOf, kStepM);
  const double headingStep = WorstStep(smooth, HeadingOf, kStepM);
  Note("worst curvature step over 0.1 m", curvatureStep, "1/m");
  Note("what a 0.1 m step of the spiral alone accounts for", kCurvature * kStepM / kSpiralM, "1/m");
  CHECK(curvatureStep < 2.0 * kCurvature * kStepM / kSpiralM,
        "**THE CURVATURE NEVER LEAPS.** Over 0.1 m it changes by no more than the spiral's own linear "
        "rate, which is what a driver produces at a constant steering rate and what a railway's "
        "lateral force needs");
  Note("worst heading step over 0.1 m", headingStep, "rad");
  CHECK(headingStep < 1.1 * kCurvature * kStepM,
        "and the heading never leaps either, so there is no kink for a wheel to find");

  ReferenceLine kinked;
  const bool leapt = kinked.Lay(origin, WithoutSpirals(), why);
  CHECK(!leapt,
        "**THE NEGATIVE CONTROL: a straight meeting an arc with no spiral is REFUSED.** That is the "
        "whole of this item -- a crack is not detected downstream, it has no spelling upstream");
  if (!leapt) { std::printf("NOTE the refusal says: %s\n", why.c_str()); }
  CHECK(!leapt && why.find("leap in curvature") != std::string::npos,
        "and the refusal names what it refused rather than reporting a bad shape");
  CHECK(!leapt && why.find("spiral") != std::string::npos,
        "and names what would carry the transition, so the sentence is a repair and not a verdict");

  Placed entry, exit;
  CHECK(smooth.At(0.0, entry) && smooth.At(smooth.LengthM(), exit),
        "the line is placeable at both of its ends");
  CHECK_NEAR(entry.CurvaturePerM, 0.0, 1e-15, "1/m", "it enters straight");
  CHECK_NEAR(exit.CurvaturePerM, 0.0, 1e-12, "1/m", "and leaves straight");

  Placed midArc;
  CHECK(smooth.At(kStraightM + kSpiralM + 0.5 * kArcM, midArc), "and inside its arc");
  CHECK_NEAR(midArc.CurvaturePerM, kCurvature, 1e-12, "1/m",
             "where the curvature is the radius the arc declared");

  Placed nowhere;
  CHECK(!smooth.At(-1.0, nowhere) && !smooth.At(smooth.LengthM() + 1.0, nowhere),
        "and a station off either end places nothing rather than extrapolating");

  ReferenceLine none;
  CHECK(!none.Lay(origin, {}, why), "a reference line of no segments is refused");
  CHECK(!none.Lay(origin, {{Curve::Straight, 0.0, 0.0, 0.0}}, why),
        "and so is one whose segment has no length");

  Covers("I.30 a corridor is a reference line of straights, arcs and spirals: the curvature is "
         "continuous along it by construction, and a transition that would leap has no spelling -- "
         "which is what makes a crack at a junction unbuildable rather than findable");
  return Report();
}
