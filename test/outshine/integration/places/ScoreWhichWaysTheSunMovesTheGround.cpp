#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include <Outshine.h>
#include <scenario/Scenario.h>

#include "Check.h"

// This place-level check observes terrain, buildings and sky together under fixed exposure.
// Its streamed world remains playable but not refined during capture. Geometry may publish again
// after a declaration; cross-history pixel equality is therefore tested on an isolated native
// receiver in SunElevationLightsAnalyticPlane instead of on this changing place.

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
constexpr double kFixedExposure = 2.5 / (1.2 * 40000.0);

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
  (void)engine.setRoots(
      outshine::Roots{"src/assets/drive", "src/assets", "/tmp/outshine-drive-cache", false});
  if (!engine.setRenderTarget(outshine::Extent{kWidePx, kHighPx})) {
    Unprepared("the device stood no canvas");
    return Report();
  }

  const auto stoodAt = [&](double elevationDeg,
                           std::vector<uint8_t> &rgba,
                           double exposure = kFixedExposure) -> outshine::Result {
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
    stands.Render.Exposure = exposure;
    stands.Lit.Declared = true;
    stands.Lit.Key.Lux = 40000.0;
    stands.Lit.Key.ElevationDeg = elevationDeg;
    stands.Lit.Key.BearingDeg = kSunBearingDeg;
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
    auto prepared = engine.declare(stands);
    if (prepared) { prepared = engine.assemble(); }
    if (prepared) { prepared = engine.preload(kPatienceS); }
    if (prepared) { prepared = engine.advance(); }
    const int settle = std::max(2, engine.renderer().settleFrames());
    for (int frame = 0; prepared && frame < settle + 1; ++frame) {
      prepared = engine.renderer().render(outshine::Extent{});
    }
    if (prepared) { prepared = engine.renderer().readPixels(rgba); }
    return prepared;
  };

  std::vector<uint8_t> low, lowTwice, middling, high;
  auto prepared = stoodAt(5.0, low);
  if (prepared) { prepared = stoodAt(5.0, lowTwice); }
  if (prepared) { prepared = stoodAt(30.0, middling); }
  if (prepared) { prepared = stoodAt(75.0, high); }
  if (!prepared) {
    Unprepared(("place preparation failed: " + prepared.error()).c_str());
    return Report();
  }

  const int groundFrom = (kHighPx * 3) / 4;
  const int skyTo = kHighPx / 8;
  const double atFive = Luminance(low, kWidePx, groundFrom, kHighPx);
  const double atThirty = Luminance(middling, kWidePx, groundFrom, kHighPx);
  const double atSeventyFive = Luminance(high, kWidePx, groundFrom, kHighPx);
  const double atFiveTwice = Luminance(lowTwice, kWidePx, groundFrom, kHighPx);

  std::printf("GROUND, bottom quarter    5 deg %7.3f   30 deg %7.3f   75 deg %7.3f\n",
              atFive,
              atThirty,
              atSeventyFive);
  std::printf(
      "5 deg AGAIN, straight after         %7.3f   (%+.4f)\n", atFiveTwice, atFiveTwice - atFive);

  CHECK(lowTwice == low, "repeating the same scene reproduces every pixel");

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
  prepared = stoodAt(30.0, dim, derived * 0.5);
  if (prepared) { prepared = stoodAt(30.0, bright, derived * 2.0); }
  if (!prepared) {
    Unprepared(("a declared exposure would not stand: " + prepared.error()).c_str());
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
        "declaration does not reproduce, the difference could come from another world update "
        "rather than the exposure declaration, and the claim would prove nothing");

  Covers("board:2020 -- the ground's luminance rises with the sun's declared elevation");
  return Report();
}
