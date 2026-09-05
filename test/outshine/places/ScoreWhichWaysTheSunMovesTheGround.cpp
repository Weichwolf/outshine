#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include <Outshine.h>
#include <scenario/Scenario.h>

#include "Check.h"

// WHICH WAY THE SUN MOVES THE GROUND -- board:2020's own second control, and it had never been run.
//
// The item claimed the terrain took no directional light and named the measurement that would
// settle it: render one place at several sun elevations and watch the ground's luminance. If the
// renders match, nothing about the sun reaches the ground. It was never run, the cause was later
// re-measured and struck, and the control still stood unrun -- which is exactly how a struck cause
// can leave a real question unanswered behind it.
//
// The oracle is not this tree's. A Lambertian surface takes irradiance as `E * sin(elevation)`, so
// between 5 and 75 degrees the DIRECT term alone changes by sin75/sin5 = 11.1x. Ambient does not
// scale that way and compresses it hard, so no exact figure is asserted here -- only the ORDER,
// which is what the physics fixes and what a broken directional term cannot produce.
//
// Three claims and two controls, and the controls are the reason the claims mean anything:
//
//   MOVES        the ground is brighter at 75 deg than at 5 deg
//   MONOTONE     30 deg lies between them. A term that merely reacted to the declaration without
//                carrying its geometry could pass MOVES and would have to work to pass this
//   STEADY       two renders at the same elevation FROM THE SAME HISTORY agree to the bit. The
//                qualifier is not a hedge, it is measured: an immediate repeat is identical, and a
//                repeat after the 75 deg frame comes back 12x darker because the exposure adapted.
//                The renderer carries temporal state, so the arms are read in ASCENDING order and
//                the adaptation gets a check of its own rather than being quietly worked around
//   THE SKY TOO  the frame's top rows brighten with the sun as well. This is the control that
//                separates "the sun reaches the ground" from "the sun reaches the frame": if the
//                ground moved and the sky did not, the ground is reading something else
//
// WHAT THIS CASE DOES NOT COVER, on its own page: nothing here says the ground's brightness is
// CORRECT at any elevation. There is no oracle in this tree for an absolute luminance and inventing
// one would be a number with no source. It reads the bottom quarter of the frame as "ground", which
// at -6 degrees of pitch from 60 m up is ground and buildings together -- it cannot separate them,
// and does not claim to. And it stands on the same cached tiles the rest of `places/` needs.

namespace {

constexpr int kWidePx = 320;
constexpr int kHighPx = 180;
constexpr double kPatienceS = 15.0;
constexpr double kSightM = 60000.0;
constexpr double kLatDeg = 49.3777;
constexpr double kLonDeg = 10.179;
constexpr double kBearingDeg = 70.0;
constexpr double kEyeAglM = 60.0;
constexpr double kPitchDeg = -6.0;
constexpr double kFovDeg = 55.0;
constexpr double kSunBearingDeg = 180.0;

[[nodiscard]] double Luminance(const std::vector<uint8_t> &rgba, int wide, int fromRow, int toRow) {
  double summed = 0.0;
  size_t counted = 0;
  for (int row = fromRow; row < toRow; ++row) {
    for (int column = 0; column < wide; ++column) {
      const size_t at = ((size_t)row * (size_t)wide + (size_t)column) * 4u;
      if (at + 2 >= rgba.size()) { continue; }
      summed += 0.2126 * rgba[at] + 0.7152 * rgba[at + 1] + 0.0722 * rgba[at + 2];
      ++counted;
    }
  }
  return counted > 0 ? summed / (double)counted : 0.0;
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

  const auto stoodAt = [&](double elevationDeg, std::vector<uint8_t> &rgba, double exposure = 0.0) {
    outshine::Scenario::Document stands;
    stands.Ground.Declared = true;
    stands.Ground.Origin.LatitudeDeg = kLatDeg;
    stands.Ground.Origin.LongitudeDeg = kLonDeg;
    stands.Ground.PatienceS = 3.0;
    stands.Ground.SightM = kSightM;
    stands.Render.Declared = true;
    stands.Render.Frame = outshine::Extent{kWidePx, kHighPx};
    stands.Render.Fill = 0.6;
    stands.Render.Exposure = exposure;
    stands.Lit.Declared = true;
    stands.Lit.Key.Lux = 40000.0;
    stands.Lit.Key.ElevationDeg = elevationDeg;
    stands.Lit.Key.BearingDeg = kSunBearingDeg;
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
      return false;
    }
    const int settle = std::max(2, engine.renderer().settleFrames());
    for (int frame = 0; frame < settle; ++frame) {
      if (!engine.renderer().render(outshine::Extent{})) { return false; }
    }
    return static_cast<bool>(engine.renderer().readPixels(rgba));
  };

  std::vector<uint8_t> low, lowTwice, middling, high, lowAgain;
  if (!stoodAt(5.0, low) || !stoodAt(5.0, lowTwice) || !stoodAt(30.0, middling) ||
      !stoodAt(75.0, high) || !stoodAt(5.0, lowAgain)) {
    Unprepared(("this place needs terrain and OSM tiles and this machine has none cached: " +
                engine.error())
                   .c_str());
    return Report();
  }

  const int groundFrom = (kHighPx * 3) / 4;
  const int skyTo = kHighPx / 8;
  const double atFive = Luminance(low, kWidePx, groundFrom, kHighPx);
  const double atThirty = Luminance(middling, kWidePx, groundFrom, kHighPx);
  const double atSeventyFive = Luminance(high, kWidePx, groundFrom, kHighPx);
  const double atFiveTwice = Luminance(lowTwice, kWidePx, groundFrom, kHighPx);
  const double atFiveAgain = Luminance(lowAgain, kWidePx, groundFrom, kHighPx);

  std::printf("GROUND, bottom quarter    5 deg %7.3f   30 deg %7.3f   75 deg %7.3f\n",
              atFive,
              atThirty,
              atSeventyFive);
  std::printf(
      "5 deg AGAIN, straight after         %7.3f   (%+.4f)\n", atFiveTwice, atFiveTwice - atFive);
  std::printf(
      "5 deg AGAIN, after the 75 deg frame %7.3f   (%+.4f)\n", atFiveAgain, atFiveAgain - atFive);

  CHECK(
      atFiveTwice == atFive,
      "**THE CONTROL: THE SAME DECLARATION FROM THE SAME HISTORY GIVES THE SAME PICTURE, TO THE "
      "BIT**. Every claim below is a difference between two renders, and a difference is evidence "
      "only if an unchanged input reproduces. If this goes red the case is reading streaming that "
      "had not finished, or noise, and has been calling it sunlight");

  CHECK(
      atFiveAgain != atFive,
      "**THE SECOND CONTROL: THE RENDERER CARRIES EXPOSURE STATE, AND THIS CASE SAYS SO**. The "
      "same 5 deg declaration rendered after the 75 deg frame comes back an order of magnitude "
      "darker, because the exposure has adapted to a bright scene and one frame does not undo it. "
      "That is not a defect and this case does not treat it as one -- it is why the control above "
      "is `from the same history` rather than `the same declaration`, and why the arms below are "
      "read in ASCENDING order. A first draft of this case demanded statelessness from a renderer "
      "that legitimately adapts and went red on the engine being right. If THIS check goes red "
      "the adaptation has gone, and the control above has quietly become vacuous");

  CHECK(atSeventyFive > atFive,
        "**THE SUN REACHES THE GROUND**: board:2020's own second control, unrun until now. A "
        "Lambertian surface takes `E * sin(elevation)`, so a sun at 75 degrees must light the "
        "ground harder than one at 5. If these match, the only illuminant reaching the terrain is "
        "the sky and the directional term is not arriving");

  CHECK(atThirty > atFive && atSeventyFive > atThirty,
        "**AND IT MOVES WITH THE GEOMETRY, NOT MERELY WITH THE DECLARATION**: 30 degrees lies "
        "between 5 and 75. A term that reacted to the number without carrying `sin(elevation)` "
        "could pass the claim above by accident and has to work to pass this one");

  const double skyAtFive = Luminance(low, kWidePx, 0, skyTo);
  const double skyAtSeventyFive = Luminance(high, kWidePx, 0, skyTo);
  std::printf(
      "SKY, top eighth           5 deg %7.3f   75 deg %7.3f\n", skyAtFive, skyAtSeventyFive);
  CHECK(
      skyAtSeventyFive > skyAtFive,
      "**THE SECOND CONTROL: THE SKY MOVES WITH THE SUN TOO**. One sun drives the atmosphere and "
      "the ground through the same declaration, which is what Unreal's `SkyAtmosphere` and RAGE's "
      "timecycle both do. A ground that brightened while the sky sat still would mean the ground "
      "is reading something that is not the sun");

  // A DECLARED EXPOSURE REACHES THE FRAME, which is the other half of board:2020's second box.
  // `Scenario::Render.Exposure` was read by the scenario parser and then by NOTHING: `Declaring`
  // never copied it, so `Live` saw 0 and always took the branch that derives an exposure from the
  // key light. A declaration the engine accepts and then ignores is worse than one it refuses,
  // because the client has no way to see that it did not land.
  //
  // The arms are one stop either side of the exposure the ENGINE derives for this key light, so
  // both frames sit where the tone curve can still tell them apart. The derivation is the engine's
  // own and is written out rather than a number picked to work: EV100 = log2(lux / 2.5) = 13.97 at
  // 40 000 lx, and exposure = 1 / (1.2 * 2^EV100) = 5.2e-5. A first draft used 0.5 and 1.0 and both
  // arms came back at 255 -- saturated, equal, and proving nothing.
  const double ev100 = std::log2(40000.0 / 2.5);
  const double derived = 1.0 / (1.2 * std::pow(2.0, ev100));
  std::vector<uint8_t> dim, bright;
  if (!stoodAt(30.0, dim, derived * 0.5) || !stoodAt(30.0, bright, derived * 2.0)) {
    Unprepared(("a declared exposure would not stand: " + engine.error()).c_str());
    return Report();
  }
  const double atHalf = Luminance(dim, kWidePx, groundFrom, kHighPx);
  const double atOne = Luminance(bright, kWidePx, groundFrom, kHighPx);
  std::printf("DECLARED EXPOSURE  half %.2e %7.3f   twice %.2e %7.3f\n",
              derived * 0.5,
              atHalf,
              derived * 2.0,
              atOne);
  CHECK(
      atOne > atHalf,
      "**A DECLARED EXPOSURE REACHES THE FRAME**: `Scenario::Render.Exposure` was parsed and then "
      "read by nothing at all, so a client could state one and watch the engine derive its own "
      "from the key light instead. Accepting a declaration and doing nothing with it is worse "
      "than refusing it, because nothing tells the client it did not land");

  std::vector<uint8_t> againAtOne;
  if (!stoodAt(30.0, againAtOne, derived * 2.0)) {
    Unprepared("the exposure control would not stand");
    return Report();
  }
  const double atOneAgain = Luminance(againAtOne, kWidePx, groundFrom, kHighPx);
  std::printf("THE SAME twice-derived again %7.3f   (%+.4f)\n", atOneAgain, atOneAgain - atOne);
  CHECK(atOneAgain == atOne,
        "**THE CONTROL FOR IT: THE SAME DECLARED EXPOSURE REPRODUCES**. The claim above is a "
        "difference between two frames whose only stated difference is one number. If an unchanged "
        "declaration does not reproduce, the difference could be the adaptation this case already "
        "measured rather than the declaration, and the claim would prove nothing");

  Covers("board:2020 -- the ground's luminance rises with the sun's declared elevation");
  return Report();
}
