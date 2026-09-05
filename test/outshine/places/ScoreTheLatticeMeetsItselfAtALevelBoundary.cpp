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
constexpr double kSightM = 240000.0;
constexpr double kLatDeg = 47.2537;
constexpr double kLonDeg = 7.4231;
constexpr double kBearingDeg = 120.57;
constexpr double kEyeAglM = 60.0;
constexpr double kPitchDeg = -6.0;
constexpr double kFovDeg = 55.0;
constexpr double kSeamToleranceM = 1.0e-3;

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
  watches.Id = "station";
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
    Unprepared(
        ("this place needs terrain tiles and this machine has none cached: " + engine.error())
            .c_str());
    return Report();
  }

  const std::vector<outshine::Measure> &told = engine.measures();
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
        "odd node takes the mean of its even neighbours in the vertex shader, and that mean IS "
        "the coarser chord at that point, so the two levels meet with no gap and no skirt shows. "
        "Measured here on the CPU with the shader's own formula over the pages the shader reads");

  Covers("board:2115 -- the lattice is gap-free where two of its levels meet");
  return Report();
}
