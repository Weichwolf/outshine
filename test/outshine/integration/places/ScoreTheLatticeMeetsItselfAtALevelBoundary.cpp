#include <cstdio>
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
constexpr double kSightM = 240000.0;
constexpr double kLatDeg = 47.2537;
constexpr double kLonDeg = 7.4231;
constexpr double kBearingDeg = 120.57;
constexpr double kEyeAglM = 60.0;
constexpr double kPitchDeg = -6.0;
constexpr double kFovDeg = 55.0;
constexpr double kSeamToleranceM = 1.0e-3;

[[nodiscard]] double Measured(std::span<const outshine::DiagnosticSample> measures,
                              const std::string &what) {
  for (const outshine::DiagnosticSample &one : measures) {
    if (one.Name == what) { return one.Value; }
  }
  return -1.0;
}

[[nodiscard]] outshine::Result Prepare(outshine::Engine &engine,
                                       const outshine::Scenario::Document &scenario) {
  auto declared = engine.declare(scenario);
  if (!declared) { return declared; }
  auto assembled = engine.assemble();
  if (!assembled) { return assembled; }
  auto loaded = engine.preload(kPatienceS);
  if (!loaded) { return loaded; }
  return engine.advance();
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
  watches.Id = "station";
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
  const auto prepared = Prepare(engine, stands);
  if (!prepared) {
    Unprepared(("place preparation failed: " + prepared.error()).c_str());
    return Report();
  }

  const std::span<const outshine::DiagnosticSample> told = engine.measures();
  for (const char *stage : {"ground candidate: corridors",
                            "ground candidate: earthworks",
                            "ground candidate: terrain mesh",
                            "ground candidate: water",
                            "ground candidate: longest scene geometry slice",
                            "ground candidate: publication"}) {
    const double elapsedMs = Measured(told, stage);
    std::printf("STAGE %-31s %8.3f ms\n", stage, elapsedMs);
    CHECK(elapsedMs >= 0.0, "each listed ground stage or slice publishes its measured cost");
  }
  CHECK(Measured(told, "ground candidate: corridor drape field misses") == 0.0,
        "corridors resolve every terrain query through the adaptive DEM fields");
  CHECK(Measured(told, "ground candidate: direct CPU product peak") > 0.0,
        "the published candidate records its direct CPU product peak");
  const double virtualEdges = Measured(told, "ground: seam, virtual, edges stitched");
  const double virtualEven =
      Measured(told, "ground: seam, virtual, even nodes off the coarser node, worst");
  const double virtualOddBefore = Measured(
      told, "ground: seam, virtual, odd nodes off the coarser chord before the stitch, worst");
  const double virtualOddAfter =
      Measured(told, "ground: seam, virtual, odd nodes off the coarser chord after it, worst");
  const double realEdges = Measured(told, "ground: seam, real, edges stitched");
  const double realEven =
      Measured(told, "ground: seam, real, even nodes off the coarser node, worst");
  const double realOddAfter =
      Measured(told, "ground: seam, real, odd nodes off the coarser chord after it, worst");

  std::printf("VIRTUAL seams %6.0f edges   even %.6f m   odd before %.3f m   odd after %.6f m\n",
              virtualEdges,
              virtualEven,
              virtualOddBefore,
              virtualOddAfter);
  std::printf("REAL    seams %6.0f edges   even %.3f m   odd after %.3f m   (a coarser DEM, the "
              "skirt's)\n",
              realEdges,
              realEven,
              realOddAfter);

  CHECK(virtualEdges > 0.0,
        "**THE LATTICE HAS LEVEL BOUNDARIES INSIDE THE VIRTUAL LEVELS**: four virtual levels "
        "around the eye meet each other along edges, and the seam measure walked them. Zero "
        "edges means the measure saw no boundary and every claim below is vacuous");

  CHECK(virtualOddBefore > kSeamToleranceM,
        "**THE NEGATIVE CONTROL: THE CRACK IS REAL BEFORE THE STITCH**. The finer edge's odd node "
        "stands where the field says and the coarser chord runs under or over it by metres; if "
        "this reads zero there was nothing to close and the stitch proves nothing");

  CHECK(virtualEven <= kSeamToleranceM,
        "**THE EVEN NODES OF A FINER EDGE STAND ON THE COARSER LEVEL'S NODES**, to a millimetre: "
        "a 32-quad patch at 2:1 puts every second node of the finer tile on a node of the coarser "
        "one, and both sample one field at the same fraction. This is CDLOD's precondition and "
        "Unreal's Landscape's, and it is what a 33-quad patch could not give");

  CHECK(virtualOddAfter <= kSeamToleranceM,
        "**THE STITCH CLOSES THE CRACK**: on every edge whose same-level neighbour is absent, the "
        "edge node takes the coarser chord before the height page reaches the vertex shader, "
        "including boundaries wider than 2:1. The page and the CPU surface carry the same "
        "stitched height; the measured residual is against that coarser chord");

  Covers("board:2115 -- the lattice is gap-free where two of its levels meet");
  return Report();
}
