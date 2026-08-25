#include <cmath>
#include <cstdio>
#include <numbers>
#include <vector>

#include "Check.h"

#include "Alignment.h"
#include "ReferenceLine.h"

using outshine::Align;
using outshine::Aligned;
using outshine::LayAligned;
using outshine::Placed;
using outshine::ReferenceLine;

namespace {

constexpr double kTrueRadiusM = 400.0;
constexpr double kWithinM = 8.0;
constexpr double kTightestM = 4.9017;
constexpr double kSweepRad = 0.5;

// The run-in and run-out are 200 m and not three chords: a transition curve needs tangent
// length, and at a 10 m chord three of them is 30 m, which cannot carry the spirals a 400 m
// bend owes. A fixture that varies the digitisation must hold the approach fixed, or it
// measures two things at once (board:1795).
constexpr double kApproachM = 200.0;

std::vector<double> ArcOf(double radiusM, double chordM) {
  const double perVertex = 2.0 * std::asin(0.5 * chordM / radiusM);
  const size_t vertices = (size_t)(kSweepRad / perVertex) + 1u;
  std::vector<double> out;
  out.reserve(2 * (vertices + 3));
  out.push_back(-kApproachM);
  out.push_back(0.0);
  for (size_t step = 0; step <= vertices; ++step) {
    const double angle = (double)step * perVertex;
    out.push_back(radiusM * std::sin(angle));
    out.push_back(radiusM * (1.0 - std::cos(angle)));
  }
  const double last = (double)vertices * perVertex;
  out.push_back(radiusM * std::sin(last) + kApproachM * std::cos(last));
  out.push_back(radiusM * (1.0 - std::cos(last)) + kApproachM * std::sin(last));
  return out;
}

[[nodiscard]] double FurthestFromLineM(const ReferenceLine &line,
                                      std::span<const double> points) {
  double worst = 0.0;
  for (size_t at = 0; at < points.size() / 2; ++at) {
    double alongM = 0.0;
    if (!line.Nearest(points[2 * at], points[2 * at + 1], 0.5 * line.LengthM(), line.LengthM(),
                      alongM)) {
      continue;
    }
    Placed on;
    if (!line.At(alongM, on)) { continue; }
    const double east = points[2 * at] - on.EastM, north = points[2 * at + 1] - on.NorthM;
    const double away = std::sqrt(east * east + north * north);
    worst = away > worst ? away : worst;
  }
  return worst;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Note("the radius the polyline describes", kTrueRadiusM, "m");

  double worstShare = 1.0e9, bestShare = 0.0, worstAwayM = 0.0;
  for (const double chordM : {10.0, 20.0, 50.0, 100.0}) {
    const std::vector<double> arc = ArcOf(kTrueRadiusM, chordM);
    const auto aligned = Align(arc, kWithinM, kTightestM);
    if (!aligned) { std::printf("REFUSED %s\n", aligned.error().c_str()); }
    CHECK(aligned.has_value(), "a true circular arc aligns");
    if (!aligned) { continue; }
    const double share = aligned->TightestRadiusM / kTrueRadiusM;
    std::printf("NOTE a %.0f m chord: %zu bends, %zu in the longest run, R %.3f m (%.4f of the "
                "truth), %.4f m from the vertices\n",
                chordM, aligned->Bends.size(), aligned->LongestRunVertices,
                aligned->TightestRadiusM, share, aligned->WorstAwayM);
    worstShare = share < worstShare ? share : worstShare;
    bestShare = share > bestShare ? share : bestShare;
    worstAwayM = aligned->WorstAwayM > worstAwayM ? aligned->WorstAwayM : worstAwayM;

    ReferenceLine line;
    const auto laid = LayAligned(arc, *aligned, line);
    if (!laid) { std::printf("REFUSED %s\n", laid.error().c_str()); }
    CHECK(laid.has_value(), "and the alignment lays as a reference line");
    if (laid) {
      const double departsM = FurthestFromLineM(line, arc);
      std::printf("NOTE   laid %.3f m long, furthest from a vertex %.4f m\n", line.LengthM(),
                  departsM);
      // The departure is the transition's own shift, p = R * swing^2 / 96 -- 1.04 m for a
      // 400 m bend through half a radian. It is a property of laying spirals at all, not of
      // this fitter, and it is what `withinM` exists to bound.
      const double shiftM = aligned->TightestRadiusM * kSweepRad * kSweepRad / 96.0;
      std::printf("NOTE   the transition's own shift is %.4f m\n", shiftM);
      CHECK(std::fabs(departsM - shiftM) < 0.15,
            "and what it departs by is the SPIRAL SHIFT and nothing else -- a fitter that "
            "departed by more would be missing the polyline, one that departed by less would "
            "have no transition");
      CHECK(departsM < kWithinM,
            "**AND THE LINE IT LAYS STAYS WITH THE POLYLINE**: the run-merge attempt recovered "
            "the radius and then walked off the end, because one radius against per-chord "
            "tangency demands leaves a remainder that can neither stand nor be dropped -- an "
            "alignment places its arc by the straights' intersection and has no remainder "
            "(board:1795)");
    }
    CHECK(aligned->Bends.size() == 1,
          "and the whole sweep is ONE bend, because the curvature never reverses in it");
  }

  Note("the worst share of the truth over the sweep", worstShare, "of it");
  Note("the best", bestShare, "of it");
  Note("the furthest it leaves a vertex", worstAwayM, "m");

  CHECK(worstShare > 0.99 && bestShare <= 1.0 + 1.0e-6,
        "**A POLYLINE THAT DESCRIBES A CIRCLE ALIGNS AS THAT CIRCLE**: the arc is placed by the "
        "point where the entering and leaving straights MEET, so its radius is a property of "
        "the curve rather than of the room between two consecutive chords -- which is what put "
        "the corner table at 1/(1 + alpha) of the truth at every digitisation density "
        "(board:1795)");
  CHECK(worstAwayM < 1.0e-6,
        "and an arc through vertices that lie exactly on a circle leaves none of them");

  {
    const std::vector<double> ess = {0.0,   0.0,   100.0, 0.0,   200.0, 20.0,
                                     300.0, 60.0,  400.0, 60.0,  500.0, 20.0,
                                     600.0, 0.0,   700.0, 0.0};
    const auto aligned = Align(ess, kWithinM, kTightestM);
    if (!aligned) { std::printf("REFUSED %s\n", aligned.error().c_str()); }
    CHECK(aligned.has_value(), "an S bend aligns");
    if (aligned) {
      std::printf("NOTE the S carries %zu bends\n", aligned->Bends.size());
      for (const auto &bend : aligned->Bends) {
        std::printf("NOTE   vertices %zu..%zu turn %.4f rad at R %.2f m\n", bend.FirstVertex,
                    bend.LastVertex, bend.TurnRad, bend.RadiusM);
      }
      // Left, right, left: the polyline rises, crests flat, falls and levels, so the sign
      // reverses twice and three runs is what it describes.
      CHECK(aligned->Bends.size() == 3,
            "**AND A CURVATURE REVERSAL IS WHERE ONE BEND ENDS AND THE NEXT BEGINS**: a run of "
            "same-sign turns is one arc however many vertices a digitiser put in it, and the "
            "sign change is the only thing that ends it (board:1795)");
      if (aligned->Bends.size() == 3) {
        CHECK(aligned->Bends[0].TurnRad * aligned->Bends[1].TurnRad < 0.0 &&
                  aligned->Bends[1].TurnRad * aligned->Bends[2].TurnRad < 0.0,
              "and each turns against the one before it, which is what made them three");
      }
    }
  }

  {
    // A spiral: the radius shrinks with every chord, so no one arc holds the whole run within
    // the bound. The run's curvature never reverses, which is what makes it the case the
    // accuracy bound must end rather than the sign change.
    std::vector<double> tightening;
    double east = 0.0, north = 0.0, heading = 0.0;
    tightening.push_back(-60.0);
    tightening.push_back(0.0);
    for (int step = 0; step < 26; ++step) {
      tightening.push_back(east);
      tightening.push_back(north);
      const double radiusM = 900.0 - 30.0 * (double)step;
      const double chordM = 30.0;
      heading += chordM / radiusM;
      east += chordM * std::cos(heading);
      north += chordM * std::sin(heading);
    }
    tightening.push_back(east + 60.0 * std::cos(heading));
    tightening.push_back(north + 60.0 * std::sin(heading));

    for (const double boundM : {40.0, 8.0, 1.0}) {
      const auto aligned = Align(tightening, boundM, kTightestM);
      if (!aligned) { std::printf("REFUSED %s\n", aligned.error().c_str()); }
      CHECK(aligned.has_value(), "a tightening spiral aligns");
      if (!aligned) { continue; }
      std::printf("NOTE within %.0f m the spiral takes %zu bends, longest run %zu, worst %.4f m\n",
                  boundM, aligned->Bends.size(), aligned->LongestRunVertices,
                  aligned->WorstAwayM);
      CHECK(aligned->WorstAwayM <= boundM + 1.0e-9,
            "**AND THE ACCURACY BOUND ENDS A RUN THE SIGN CHANGE DOES NOT**: a spiral turns one "
            "way throughout, so nothing but the bound says where one arc must become two -- and "
            "a fitter that only splits on reversal would lay one arc through all of it "
            "(board:1795)");
    }
  }

  Covers("I.4.12 an alignment is one arc per RUN of same-sign turns, placed by where the "
         "entering and leaving straights meet -- so a polyline describing a circle aligns as "
         "that circle at every digitisation density (board:1795)");
  return Report();
}
