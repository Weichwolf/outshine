#include <cstdio>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

// HOW FAR THE AIR REACHES -- whether DISTANCE is visible on opaque geometry.
//
// Unreal's `SkyAtmosphere` writes an aerial perspective volume that every opaque material samples;
// RAGE's timecycle fogs all geometry by distance. They agree that a surface behind a lot of air is
// atmosphere-blended, so the matter was closed and only a proof was missing. Without one, a 4 000 m
// range at 145 km renders the same green as a 600 m hill at 20 km and the eye reads the nearer one.
//
// THE ORACLE IS RAYLEIGH'S, NOT THIS TREE'S: scattering goes as lambda^-4, so air adds BLUE and
// removes it from nothing. A band of ground seen through more air must therefore sit closer to the
// sky in blue share than a band seen through less. No absolute figure is asserted -- only the
// ORDER, which the physics fixes.
//
//   NEARER      the near ground's blue share is furthest from the sky's
//   FURTHER     the far ridge's is between the near ground's and the sky's
//   NOT A HAZE  the near ground is still clearly NOT sky-coloured. Without this the ordering could
//               hold on a frame that had been washed uniformly toward the sky, which is a defect
//               rather than a distance term
//   STEADY      an unchanged declaration reproduces to the bit
//
// THE HORIZON IS FOUND, NOT ASSUMED. Rows are scanned for the sharpest fall in blue, which is where
// sky meets ground; the bands are placed against that. A hardcoded row would be a number with no
// source and would stop being true the moment the pitch changed.
//
// WHAT THIS DOES NOT COVER, on its own page: it says nothing about whether the amount of blending is
// RIGHT at any distance -- there is no oracle in this tree for that and inventing one would be a
// number with no origin. It reads one place, one bearing and one sun. And it cannot see WHAT stands
// in the far band: at this camera it is the Alps, and the case knows only that it is far.

namespace {

constexpr int kWidePx = 320;
constexpr int kHighPx = 180;
constexpr double kPatienceS = 15.0;
constexpr double kSightM = 240000.0;
constexpr double kLatDeg = 47.132;
constexpr double kLonDeg = 7.059;
constexpr double kBearingDeg = 133.9;
constexpr double kEyeAglM = 60.0;
constexpr double kPitchDeg = -6.0;
constexpr double kFovDeg = 55.0;

struct Band {
  double Red, Green, Blue;

  [[nodiscard]] double BlueShare(void) const {
    const double all = Red + Green + Blue;
    return all > 0.0 ? Blue / all : 0.0;
  }
};

[[nodiscard]] Band Rows(const std::vector<uint8_t> &rgba, int wide, int from, int to) {
  double r = 0, g = 0, b = 0;
  size_t counted = 0;
  for (int row = from; row < to; ++row) {
    for (int column = 0; column < wide; ++column) {
      const size_t at = ((size_t)row * (size_t)wide + (size_t)column) * 4u;
      if (at + 2 >= rgba.size()) { continue; }
      r += rgba[at];
      g += rgba[at + 1];
      b += rgba[at + 2];
      ++counted;
    }
  }
  return counted > 0 ? Band{r / (double)counted, g / (double)counted, b / (double)counted}
                     : Band{0, 0, 0};
}

[[nodiscard]] int Horizon(const std::vector<uint8_t> &rgba, int wide, int high) {
  int found = high / 2;
  double steepest = 0.0;
  for (int row = 1; row < high - 1; ++row) {
    const double above = Rows(rgba, wide, row - 1, row).Blue;
    const double below = Rows(rgba, wide, row, row + 1).Blue;
    if (above - below > steepest) {
      steepest = above - below;
      found = row;
    }
  }
  return found;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so nothing can be drawn");
    return Report();
  }

  outshine::Engine engine;
  engine.setRoots(
      outshine::Roots{"apps/driver/src", "src/assets", "/tmp/outshine-drive-cache", false});
  if (!engine.drawsInto(outshine::Extent{kWidePx, kHighPx})) {
    Unprepared("the device stood no canvas");
    return Report();
  }

  const auto stood = [&](std::vector<uint8_t> &rgba) {
    outshine::Scenario stands;
    stands.Ground.Declared = true;
    stands.Ground.Origin.LatitudeDeg = kLatDeg;
    stands.Ground.Origin.LongitudeDeg = kLonDeg;
    stands.Ground.PatienceS = 3.0;
    stands.Ground.SightM = kSightM;
    stands.Render.Declared = true;
    stands.Render.Frame = outshine::Extent{kWidePx, kHighPx};
    stands.Render.Fill = 0.6;
    stands.Lit.Declared = true;
    stands.Lit.Key.Lux = 40000.0;
    stands.Lit.Key.ElevationDeg = 60.0;
    stands.Lit.Key.BearingDeg = 180.0;
    outshine::View watches;
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
    return engine.declare(stands) && engine.assemble() && engine.preload(kPatienceS) &&
           engine.advance() && engine.renderer().render(outshine::Extent{}) &&
           engine.renderer().readPixels(rgba);
  };

  std::vector<uint8_t> seen, seenAgain;
  if (!stood(seen) || !stood(seenAgain)) {
    Unprepared(("this place needs terrain tiles and this machine has none cached: " +
                engine.error())
                   .c_str());
    return Report();
  }

  const int horizon = Horizon(seen, kWidePx, kHighPx);
  const Band sky = Rows(seen, kWidePx, horizon - 12, horizon - 2);
  const Band far = Rows(seen, kWidePx, horizon + 1, horizon + 5);
  const Band near = Rows(seen, kWidePx, (kHighPx * 4) / 5, kHighPx);
  const Band farAgain = Rows(seenAgain, kWidePx, horizon + 1, horizon + 5);

  std::printf("horizon found at row %d of %d\n", horizon, kHighPx);
  std::printf("SKY   r %6.2f g %6.2f b %6.2f   blue share %.4f\n", sky.Red, sky.Green, sky.Blue,
              sky.BlueShare());
  std::printf("FAR   r %6.2f g %6.2f b %6.2f   blue share %.4f\n", far.Red, far.Green, far.Blue,
              far.BlueShare());
  std::printf("NEAR  r %6.2f g %6.2f b %6.2f   blue share %.4f\n", near.Red, near.Green, near.Blue,
              near.BlueShare());

  CHECK(far.Red + far.Green + far.Blue == farAgain.Red + farAgain.Green + farAgain.Blue,
        "**THE CONTROL: AN UNCHANGED DECLARATION REPRODUCES**. Every claim below is a comparison "
        "between bands of one frame, but the frame itself has to be the same frame twice or the "
        "numbers are streaming that had not finished");

  CHECK(sky.BlueShare() - near.BlueShare() > 0.05,
        "**THE CONTROL: THE NEAR GROUND IS NOT SKY-COLOURED**. The ordering below would also hold "
        "on a frame washed uniformly toward the sky, which is a defect and not a distance term. "
        "This is what tells an atmosphere from a wash");

  CHECK(far.BlueShare() > near.BlueShare(),
        "**DISTANCE IS VISIBLE ON OPAQUE GEOMETRY**: Rayleigh scattering goes as lambda^-4, so more "
        "air means more blue. Ground behind 145 km of it must sit closer to the sky in blue share "
        "than ground 2 km away. Without this term a 4 000 m range at 145 km renders the same green "
        "as a 600 m hill at 20 km, and the eye reads the nearer one -- which is what made the Alps "
        "look absent from a frame they were drawn in all along");

  CHECK(far.BlueShare() < sky.BlueShare(),
        "**AND THE GROUND HAS NOT BECOME THE SKY**: the far band must still be ground. A term that "
        "took it all the way to the sky's own blue share would be hiding the geometry rather than "
        "placing it in the air");

  Covers("board:2032 -- distance is visible on opaque geometry");
  return Report();
}
