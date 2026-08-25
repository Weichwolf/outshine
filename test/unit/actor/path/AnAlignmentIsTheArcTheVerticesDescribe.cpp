#include <cmath>
#include <cstdio>
#include <numbers>
#include <vector>

#include "Check.h"

#include "Alignment.h"

using outshine::Align;
using outshine::Aligned;

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
  for (size_t step = 0; step <= vertices; ++step) {
    const double angle = (double)step * perVertex;
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

  Covers("I.4.8 an alignment is one arc per RUN of same-sign turns, placed by where the "
         "entering and leaving straights meet -- so a polyline describing a circle aligns as "
         "that circle at every digitisation density (board:1795)");
  return Report();
}
