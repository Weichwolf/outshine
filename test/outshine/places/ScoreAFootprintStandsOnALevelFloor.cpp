#include <cstdio>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include <Outshine.h>
#include <scenario/Scenario.h>

#include "Check.h"

namespace {

constexpr int kWidePx = 320;
constexpr int kHighPx = 180;
constexpr double kPatienceS = 15.0;
constexpr double kSightM = 8000.0;
constexpr double kLatDeg = 49.3777;
constexpr double kLonDeg = 10.179;
constexpr double kBearingDeg = 70.0;
constexpr double kEyeAglM = 1.7;
constexpr double kPitchDeg = 0.0;
constexpr double kFovDeg = 55.0;
constexpr double kFloorToleranceM = 0.01;
constexpr double kStampWorthM = 0.25;

[[nodiscard]] double Measured(const std::vector<outshine::Measure> &measures,
                              const std::string &what) {
  for (const outshine::Measure &one : measures) {
    if (one.What == what) { return one.How; }
  }
  return -1.0;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so nothing can be drawn");
    return Report();
  }

  outshine::Engine engine;
  engine.setRoots(
      outshine::Roots{"src/assets/drive", "src/assets", "/tmp/outshine-drive-cache", false});
  if (!engine.drawsInto(outshine::Extent{kWidePx, kHighPx})) {
    Unprepared("the device stood no canvas");
    return Report();
  }

  outshine::Scenario::Document stands;
  stands.Ground.Declared = true;
  stands.Ground.Origin.LatitudeDeg = kLatDeg;
  stands.Ground.Origin.LongitudeDeg = kLonDeg;
  stands.Ground.PatienceS = 3.0;
  stands.Ground.SightM = kSightM;
  stands.Render.Declared = true;
  stands.Render.Frame = outshine::Extent{kWidePx, kHighPx};
  stands.Render.Fill = 0.6;
  stands.Lit.Declared = true;
  outshine::Scenario::View watches;
  watches.Id = "street";
  watches.Person = "first";
  watches.Sees.Stands.GlobeAnchor = true;
  watches.Sees.Stands.Geodetic.LatitudeDeg = kLatDeg;
  watches.Sees.Stands.Geodetic.LongitudeDeg = kLonDeg;
  watches.Sees.Stands.Geodetic.HeightM = kEyeAglM;
  watches.Sees.Stands.SamplesHeight = true;
  watches.Sees.Stands.BearingDeg = kBearingDeg;
  watches.Sees.Stands.PitchDeg = kPitchDeg;
  watches.Sees.FovDeg = kFovDeg;
  stands.Views.push_back(watches);
  if (!(engine.declare(stands) && engine.assemble() && engine.preload(kPatienceS) &&
        engine.advance())) {
    Unprepared(("this place needs terrain and vector tiles and this machine has none cached: " +
                engine.error())
                   .c_str());
    return Report();
  }

  const std::vector<outshine::Measure> &told = engine.measures();
  const double pads = Measured(told, "ground: pads with a lattice node inside");
  const double padsUnreached = Measured(told, "ground: pads no lattice node reaches");
  const double padNodes = Measured(told, "ground: nodes inside those pads");
  const double padContested = Measured(told, "ground: of those pads nodes, another stamp decided");
  const double padAbove =
      Measured(told, "ground: nodes inside pads above their plane after the press, worst");
  const double padWasAbove =
      Measured(told, "ground: those pads nodes above it before the press, worst");
  const double padFoundation =
      Measured(told, "ground: nodes inside pads that do not fill, below it, worst");
  const double pieces = Measured(told, "ground: corridor pieces with a lattice node inside");
  const double pieceAbove = Measured(
      told, "ground: nodes inside corridor pieces above their plane after the press, worst");
  const double pieceBelow = Measured(
      told, "ground: nodes inside corridor pieces that fill, below it after the press, worst");
  const double pieceWasAbove =
      Measured(told, "ground: those corridor pieces nodes above it before the press, worst");
  const double pieceWasBelow = Measured(
      told, "ground: those filling corridor pieces nodes below it before the press, worst");
  const double pieceDeck =
      Measured(told, "ground: nodes inside corridor pieces that do not fill, below it, worst");

  std::printf("PADS      %6.0f reached, %6.0f below the lattice's resolution; %6.0f nodes inside, "
              "%6.0f another stamp decided\n",
              pads,
              padsUnreached,
              padNodes,
              padContested);
  std::printf("          above the seat: %.3f m before the press, %.6f m after; the foundation "
              "reaches %.3f m down\n",
              padWasAbove,
              padAbove,
              padFoundation);
  std::printf("CORRIDORS %6.0f pieces reached; above the grade %.3f m before, %.6f m after; "
              "below it %.3f m before, %.6f m after; a deck clears %.3f m\n",
              pieces,
              pieceWasAbove,
              pieceAbove,
              pieceWasBelow,
              pieceBelow,
              pieceDeck);

  CHECK(pads > 0.0 && padNodes > 0.0,
        "**THE LATTICE REACHES INTO FOOTPRINTS**: at least one pad holds a lattice node inside "
        "its ring, so the claims below are about real nodes. Zero here means the resolution "
        "never reached a footprint and every claim below is vacuous");

  CHECK(padWasAbove > kStampWorthM,
        "**THE NEGATIVE CONTROL: THE GROUND STOOD ABOVE THE SEAT BEFORE THE PRESS**. Without the "
        "stamp the terrain rises through a footprint by more than a stamp is worth; if this "
        "reads zero there was nothing to cut and the press proves nothing");

  CHECK(padAbove <= kFloorToleranceM,
        "**NO GROUND STANDS ABOVE A BUILDING'S SEAT**: every lattice node inside a footprint "
        "that the stamp decided reads the seat or lies below it, to a centimetre, on the page "
        "the shader draws. Unreal's flatten brush and RAGE's cook write the same number into "
        "the height field; a node above the seat is a floor with ground poking through it");

  CHECK(pieces > 0.0 && pieceWasAbove > kStampWorthM && pieceWasBelow > kStampWorthM,
        "**THE NEGATIVE CONTROL FOR THE ROADS**: corridor pieces exist with nodes inside, and "
        "the terrain stood above AND below the designed grade before the press");

  CHECK(pieceAbove <= kFloorToleranceM && pieceBelow <= kFloorToleranceM,
        "**A ROAD IS LEVEL ACROSS AND ON ITS GRADE ALONG**: every node inside a filling corridor "
        "piece that the stamp decided reads the piece's graded plane to a centimetre, neither "
        "above (a cut) nor below (a fill); a bridge piece cuts only and its clearance is the "
        "deck's, reported beside");

  Covers("board:2121 -- a footprint is stamped flat and a road follows its grade");
  return Report();
}
