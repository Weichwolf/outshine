#include <cstdio>
#include <chrono>
#include <span>
#include <string>

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

[[nodiscard]] double Measured(std::span<const outshine::DiagnosticSample> measures,
                              const std::string &what) {
  for (const outshine::DiagnosticSample &one : measures) {
    if (one.Name == what) { return one.Value; }
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
  (void)engine.setRoots(
      outshine::Roots{"src/assets/drive", "src/assets", "/tmp/outshine-drive-cache", false});
  if (!engine.drawsInto(outshine::Extent{kWidePx, kHighPx})) {
    Unprepared("the device stood no canvas");
    return Report();
  }

  outshine::Scenario::Document stands;
  stands.Ground.Declared = true;
  stands.Ground.VegetationEnabled = false;
  stands.Ground.Origin.LatitudeDeg = kLatDeg;
  stands.Ground.Origin.LongitudeDeg = kLonDeg;
  stands.Ground.PatienceS = 3.0;
  stands.Ground.SightM = kSightM;
  stands.Render.Declared = true;
  stands.Render.Frame = outshine::Extent{kWidePx, kHighPx};
  stands.Render.CameraFill = 0.6;
  stands.Lit.Declared = true;
  outshine::Scenario::View watches;
  watches.Id = "street";
  watches.Person = "first";
  watches.Placement = outshine::Scenario::CameraPlacement::Geodetic;
  watches.Geographic.Geodetic.LatitudeDeg = kLatDeg;
  watches.Geographic.Geodetic.LongitudeDeg = kLonDeg;
  watches.Geographic.Geodetic.HeightM = kEyeAglM;
  watches.Geographic.SamplesHeight = true;
  watches.Geographic.BearingDeg = kBearingDeg;
  watches.Geographic.PitchDeg = kPitchDeg;
  watches.Sees.FovDeg = kFovDeg;
  stands.Views.push_back(watches);
  const auto timed = [](const char *phase, auto &&operation) {
    const auto began = std::chrono::steady_clock::now();
    const auto result = operation();
    std::printf(
        "PHASE %s %.3f ms %s\n",
        phase,
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count(),
        result ? "ready" : "failed");
    return result;
  };
  auto prepared = timed("declare", [&] { return engine.declare(stands); });
  double preloadMs = 0.0;
  if (prepared) {
    prepared = timed("assemble", [&] { return engine.assemble(); });
  }
  if (prepared) {
    outshine::Loading loading;
    double groundReadyS = -1.0;
    double vectorReadyS = -1.0;
    const auto preloadBegan = std::chrono::steady_clock::now();
    prepared = timed("preload", [&] {
      return engine.preload(
          kPatienceS, [&loading, &groundReadyS, &vectorReadyS](const outshine::Loading &current) {
            loading = current;
            if (groundReadyS < 0.0 && current.GroundWanted > 0 &&
                current.GroundArrived == current.GroundWanted) {
              groundReadyS = current.ElapsedS;
            }
            if (vectorReadyS < 0.0 && current.VectorWanted > 0 &&
                current.VectorArrived == current.VectorWanted) {
              vectorReadyS = current.ElapsedS;
            }
          });
      preloadMs =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - preloadBegan)
              .count();
    });
    if (!prepared) {
      prepared = std::unexpected(
          prepared.error() + "; streaming ground=" + std::to_string(loading.GroundArrived) + "/" +
          std::to_string(loading.GroundWanted) + ", vector=" +
          std::to_string(loading.VectorArrived) + "/" + std::to_string(loading.VectorWanted) +
          ", outstanding=" + std::to_string(loading.Outstanding) +
          ", fetchedMiB=" + std::to_string(loading.FetchedMB) + ", meanFetchMs=" +
          std::to_string(loading.MeanFetchMs) + ", groundReadyS=" + std::to_string(groundReadyS) +
          ", vectorReadyS=" + std::to_string(vectorReadyS));
    }
  }
  if (prepared) {
    prepared = timed("advance", [&] { return engine.advance(); });
  }
  if (!prepared) {
    std::printf("PRELOAD generator work %.3f ms\n",
                Measured(engine.measures(), "preload: generator work"));
    for (const char *what : {"rebuild: and handing it to the device took",
                             "rebuild: and the buildings, streets and water took",
                             "rebuild: of that, the surface the world stands on",
                             "streets: everything Paves did",
                             "streets: of that, finding the crossings",
                             "streets: of finding them, laying the lanes into a network",
                             "streets: of finding them, the sweep itself",
                             "streets: of finding them, filing each one",
                             "streets: of finding them, raising a deck over each",
                             "streets: crossings the plan found",
                             "streets: of that, raising the decks",
                             "streets: of raising decks, seeding bridge ends",
                             "streets: of raising decks, easing ramps",
                             "streets: of raising decks, grading approaches",
                             "streets: of that, designing every lane",
                             "streets: of that, shaping the junctions",
                             "streets: of that, paving every lane",
                             "streets: of designing, the fit",
                             "streets: of designing, the water",
                             "streets: of designing, the sweep",
                             "streets: of designing, the yields",
                             "streets: of paving, the fit",
                             "streets: of paving, the water",
                             "streets: of paving, the sweep",
                             "streets: of paving, the yields",
                             "ground: of that, pressing",
                             "rebuild: of that, the streets and the water",
                             "class field: it calls itself complete"}) {
      std::printf("PRELOAD %s %.3f\n", what, Measured(engine.measures(), what));
    }
    Unprepared(("place preparation failed: " + prepared.error()).c_str());
    return Report();
  }

  const std::span<const outshine::DiagnosticSample> told = engine.measures();
  for (const char *stage : {"ground candidate: corridors",
                            "ground candidate: earthworks",
                            "ground candidate: terrain mesh",
                            "ground candidate: water",
                            "ground candidate: final scene geometry slice",
                            "ground candidate: class upload",
                            "ground candidate: longest scene geometry slice",
                            "ground candidate: publication"}) {
    const double elapsedMs = Measured(told, stage);
    std::printf("STAGE %-31s %8.3f ms\n", stage, elapsedMs);
    CHECK(elapsedMs >= 0.0, "each listed ground stage or slice publishes its measured cost");
  }
  for (const char *operation : {"rebuild: cutting it into clusters",
                                "rebuild: of the streams, packing them",
                                "rebuild: and the device taking them",
                                "ground candidate: corridor drape field misses",
                                "streets: of that, finding the crossings",
                                "streets: of that, raising the decks",
                                "streets: of that, designing every lane",
                                "streets: of that, shaping the junctions",
                                "streets: of that, paving every lane",
                                "streets: of that, raising the junction bodies",
                                "streets: of that, handing the paving over",
                                "streets: everything Paves did",
                                "ground: of that, pressing",
                                "rebuild: of that, walking it into the proxy",
                                "rebuild: standing render plan",
                                "rebuild: standing and submitting INSIDE Build",
                                "rebuild: shaping what was built",
                                "rebuild: composing it",
                                "stand: the medium's own tables",
                                "rebuild: laying the surface",
                                "rebuild: settling placements and lights",
                                "rebuild: and the streams to the device"}) {
    std::printf("DETAIL %-47s %8.3f ms\n", operation, Measured(told, operation));
  }
  CHECK(Measured(told, "ground candidate: corridor drape field misses") == 0.0,
        "corridors resolve every terrain query through the adaptive DEM fields");
  CHECK(Measured(told, "ground candidate: direct CPU product peak") > 0.0,
        "the published candidate records its direct CPU product peak");
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

  CHECK(preloadMs <= kPatienceS * 1000.0,
        "**THE PLACE BECOMES RESIDENT WITHIN ITS DECLARED PRELOAD BUDGET**: final world assembly "
        "must not silently run after the 15-second budget has expired");

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
