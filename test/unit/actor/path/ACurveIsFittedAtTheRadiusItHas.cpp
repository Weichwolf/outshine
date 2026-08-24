#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>
#include <vector>

#include "Check.h"

#include "Fit.h"

using outshine::Fit;
using outshine::Fitted;
using outshine::Placed;
using outshine::ReferenceLine;

namespace {

constexpr double kTrueRadiusM = 400.0;
constexpr double kWithinM = 8.0;
constexpr double kTightestM = 4.9017;
constexpr double kSweepRad = 0.5;

std::vector<double> ArcOf(double radiusM, double chordM) {
  const double perVertex = 2.0 * std::asin(0.5 * chordM / radiusM);
  const size_t vertices = (size_t)(kSweepRad / perVertex) + 1u;
  std::vector<double> out;
  out.reserve(2 * (vertices + 3));
  out.push_back(-3.0 * chordM);
  out.push_back(0.0);
  for (size_t at = 0; at <= vertices; ++at) {
    const double angle = (double)at * perVertex;
    out.push_back(radiusM * std::sin(angle));
    out.push_back(radiusM * (1.0 - std::cos(angle)));
  }
  const double last = (double)vertices * perVertex;
  out.push_back(radiusM * std::sin(last) + 3.0 * chordM * std::cos(last));
  out.push_back(radiusM * (1.0 - std::cos(last)) + 3.0 * chordM * std::sin(last));
  return out;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Note("the radius the polyline describes", kTrueRadiusM, "m");
  Note("how far the fit may leave a vertex", kWithinM, "m");

  double worstShare = 1.0e9;
  double bestShare = 0.0;
  for (const double chordM : {10.0, 20.0, 50.0, 100.0}) {
    const std::vector<double> arc = ArcOf(kTrueRadiusM, chordM);
    ReferenceLine line;
    const Fitted laid = Fit(arc, kWithinM, kTightestM, line);
    if (!laid.Laid) { std::printf("REFUSED %s\n", laid.Error.c_str()); }
    const double share = laid.TightestRadiusM / kTrueRadiusM;
    std::printf("NOTE a %.0f m chord: %zu vertices, tightest %.3f m, %.4f of the truth\n",
                chordM, laid.Vertices, laid.TightestRadiusM, share);
    Note("corners it laid", (double)laid.Corners, "corners");
    Note("runs it found", (double)laid.Runs, "runs");
    Note("the longest run", (double)laid.LongestRunVertices, "vertices");
    Note("how far it leaves the polyline", laid.WorstOffsetM, "m");
    CHECK(laid.Laid, "a true circular arc lays as a corridor");
    worstShare = share < worstShare ? share : worstShare;
    bestShare = share > bestShare ? share : bestShare;
  }

  Note("the worst share of the truth over the sweep", worstShare, "of it");
  Note("the best", bestShare, "of it");

  CHECK(worstShare > 0.95,
        "**A POLYLINE THAT DESCRIBES A CIRCLE IS RECONSTRUCTED AS THAT CIRCLE.** The fit puts "
        "one spiral-arc-spiral at every vertex and returns the curvature to zero between them "
        "-- correct for a road corner between two straights, wrong for a polyline describing a "
        "curve, where a transition is owed where the CURVATURE changes and not where a "
        "digitiser placed a point. The understatement is 1/(1 + alpha) with alpha = 0.5 the "
        "spiral's own length share, so the peak curvature is 1.5x too high at every "
        "digitisation density -- and peak curvature is exactly what SpeedProfile bounds speed "
        "by (board:1795)");
  CHECK(bestShare <= 1.0 + 1.0e-9,
        "and no chord length recovers MORE than the circle, which would be a fit leaving the "
        "polyline on the outside of its own curve");

  Covers("I.4.7 a polyline that describes a circular arc is fitted at the radius it describes, "
         "at every digitisation density -- the corner model owes a transition where the "
         "curvature changes, not where a vertex sits (board:1795)");
  return Report();
}
