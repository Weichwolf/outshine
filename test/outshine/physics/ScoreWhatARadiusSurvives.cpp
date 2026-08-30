#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <numbers>
#include <vector>

#include "Check.h"
#include "Fit.h"
#include "ReferenceLine.h"

namespace {

// THE ORACLE IS A CIRCLE, and a circle's radius is not a property of how finely it was sampled.
//
// Sample a true arc of radius R at chord length s. Every vertex lies exactly on the circle, so
// the curve those vertices describe HAS radius R -- at s = 5 m, at s = 40 m, at any s. A fit
// that reads a different radius at different densities is reading the digitiser rather than the
// road, and this case is the measurement of that.
//
// TWO NUMBERS, both closed form:
//
//   radius     R                          by construction
//   length     R * (n * s / R) = n * s     the arc a fit lays over the sweep it was GIVEN
//
// The chord subtends s/R at the centre, so n = floor(theta / (s/R)) whole chords fit inside the
// sweep and each vertex sits at angle i * s/R. **The sweep the fit is GIVEN is n * s / R, not
// theta** -- at s = 40 m and R = 400 m that is 1.5 rad against 1.5708, and comparing a fit
// against the sweep it was not handed measures the case's own arithmetic. So the length owed is
// n * s: the arc through n chords of arc-length s each.
//
// WHAT THIS CATCHES. A fit that lays a spiral-arc-spiral at EVERY vertex and returns curvature
// to zero between them collapses, for a smooth curve digitised into short chords, to
//
//   TangentShare(s) -> 0.75 s        so   byRoom -> 0.5 L / 0.75 s = (2/3) (L/s)
//
// while the arc the polyline carries has R = L / (2 sin(s/2)) -> L/s. Two thirds of the radius,
// at EVERY density -- which is why a density sweep is the instrument and a single density is
// not. On a shipped route that error reverses curvature three times in 45 m and reaches a 5.6 m
// radius, tighter than the vehicle's own turning circle, in the middle of a long-distance plan.
//
// A transition is owed where the CURVATURE changes, never where a digitiser put a point.
constexpr double kRadiusM = 400.0;
constexpr double kSweepRad = 0.5 * std::numbers::pi;
constexpr double kWithinM = 0.5;

[[nodiscard]] std::vector<double> Arc(double chordM) {
  const double step = chordM / kRadiusM;
  const size_t chords = (size_t)(kSweepRad / step);
  std::vector<double> points;
  points.reserve(2 * (chords + 1));
  for (size_t at = 0; at <= chords; ++at) {
    const double angle = (double)at * step;
    points.push_back(kRadiusM * std::sin(angle));
    points.push_back(kRadiusM * (1.0 - std::cos(angle)));
  }
  return points;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  constexpr double kChords[4] = {5.0, 10.0, 20.0, 40.0};
  double read[4] = {0.0, 0.0, 0.0, 0.0};
  double laid[4] = {0.0, 0.0, 0.0, 0.0};
  double owed[4] = {0.0, 0.0, 0.0, 0.0};
  size_t runs[4] = {0, 0, 0, 0};
  bool stood = true;

  for (int at = 0; at < 4; ++at) {
    const std::vector<double> points = Arc(kChords[at]);
    outshine::ReferenceLine line;
    const outshine::Fitted fitted = outshine::Fit(points, kWithinM, 0.0, line);
    if (!fitted.Laid) {
      std::printf("  chord %5.1f m   REFUSED %s\n", kChords[at], fitted.Error.c_str());
      stood = false;
      continue;
    }
    read[at] = fitted.TightestRadiusM;
    laid[at] = fitted.LengthM;
    runs[at] = fitted.Runs;
    const size_t chords = points.size() / 2 - 1;
    owed[at] = (double)chords * kChords[at];
    std::printf("  chord %5.1f m over %3zu vertices   radius %8.3f m   length %8.3f m "
                "(owed %8.3f)   runs %zu\n",
                kChords[at],
                chords + 1,
                read[at],
                laid[at],
                owed[at],
                runs[at]);
  }
  std::printf("  the circle itself                 radius %8.3f m\n", kRadiusM);

  if (!stood) {
    Unprepared("a fit refused, so there is no radius to judge");
    return Report();
  }

  double worst = 0.0, worstLength = 0.0;
  for (int at = 0; at < 4; ++at) {
    const double off = std::fabs(read[at] - kRadiusM) / kRadiusM;
    if (off > worst) { worst = off; }
    const double sweptM = owed[at];
    const double along = std::fabs(laid[at] - sweptM) / sweptM;
    if (along > worstLength) { worstLength = along; }
  }
  std::printf("  worst radius error %6.3f %%      worst length error %6.3f %%\n",
              100.0 * worst,
              100.0 * worstLength);

  CHECK(worst < 0.01,
        "**A FIT READS THE RADIUS THE LINE CARRIES**: every vertex of this input lies exactly on "
        "a circle of 400 m, so the curve they describe HAS that radius at every sampling "
        "density. A fit whose answer moves with the chord length is reading the digitiser and "
        "not the road, and a speed profile built on it obeys a line no road has");

  CHECK(worstLength < 0.005,
        "and it lays the LENGTH the sweep demands: n chords of s metres of arc each is n*s of "
        "road, and that is the distance a speed profile integrates over");

  for (int at = 0; at < 4; ++at) {
    CHECK(runs[at] == 1,
          "and the whole sweep is ONE run: every turn in it has the same sign, so a transition "
          "is owed nowhere inside it -- a fit that starts a new alignment at each vertex is "
          "answering a question the digitiser asked rather than the one the curvature did");
  }

  // AND THE FIT MINIMISES WHAT IT ACCEPTS -- asserted here in prose because no input in this
  // file falls against it. The radius is chosen by a ternary search and the run is accepted by an
  // accuracy test, and those read two different things unless the search reads what the test
  // reads: the worst SHARE of each vertex's own allowance, not the worst distance. They agree
  // whenever every vertex carries the same bound, which is every input here and was every input
  // in the tree before the per-vertex span existed (board:1912, 1916).
  //
  // An input was built to separate them -- one vertex a metre off the circle with a loose
  // allowance, another exactly on it with a tight one -- and it splits the run at 348.962 m for a
  // reason that is not the mismatch: a metre of radial push on a 20 m chord reverses the sign of
  // the turn, so the RUN rule breaks it before the accuracy rule is consulted. The case is
  // recorded rather than kept, because a green check over an input that cannot fall proves the
  // input and not the rule.
  Covers("a fitted corridor reads the radius the polyline carries and lays the arc length its "
         "sweep demands, at every digitisation density, because a circle's radius is not a "
         "property of how finely it was sampled");
  return Report();
}
