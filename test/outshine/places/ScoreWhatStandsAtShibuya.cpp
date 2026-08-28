#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

// SHIBUYA CROSSING AT NIGHT -- thousands of small emissive sources, wet asphalt, signage.
//
// THIS IS NOT A PROOF AND SAYS SO. What it asserts is that the declaration STANDS, that it
// advances, and that a picture comes out of it carrying more than one colour. Whether the picture
// is any GOOD is the owner's judgement and not a number this case may invent -- the same line
// ScoreWhatTheDriveMeasures draws. The screenshot lands in `build/places/` for an eye to look at,
// and `build/` is a build artefact directory rather than the tree.
//
// WHAT THIS PLACE IS HERE TO EXPOSE, measured rather than imagined:
// the light cap is sixteen (board:1943), nothing reflects (board:2012), an emissive material is
// not a light source, and no surface is wet
//
// WHAT I EXPECT TO SEE, written before looking so that being wrong is visible. The suggestion was
// mine, so the prediction is mine to be judged on.
//   Streets and building footprints from OSM, unlit or nearly so, under a night sky the Bruneton
//   chain draws correctly. The one key light is the sun below the horizon, so the ground should be
//   dark and FLAT -- lit only by the sky's ambient, which is one radiance for every normal.
//   I expect NO signage, NO emissive glow, NO wet reflection, and the crossing to read as grey
//   geometry at night rather than as Shibuya. If it looks like anything else, the surprise is the
//   finding.

namespace {

constexpr const char *kPlace = "Shibuya";
constexpr int kWidePx = 1280;
constexpr int kHighPx = 720;
constexpr int kSteps = 8;
constexpr double kLatDeg = 35.6595;
constexpr double kLonDeg = 139.7005;
constexpr double kSunElevationDeg = 6.0;
constexpr double kSunBearingDeg = 210.0;

[[nodiscard]] size_t Colours(const std::vector<uint8_t> &rgba) {
  size_t apart = 0;
  for (size_t at = 4; at + 3 < rgba.size(); at += 4) {
    if (rgba[at] != rgba[0] || rgba[at + 1] != rgba[1] || rgba[at + 2] != rgba[2]) { ++apart; }
  }
  return apart;
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
  engine.Under(outshine::Roots{"apps/driver/src", "src/assets", "/tmp/outshine-drive-cache", false});
  if (!engine.DrawsInto(outshine::Extent{kWidePx, kHighPx})) {
    Unprepared("the device stood no canvas");
    return Report();
  }

  outshine::Scenario stands;
  stands.Ground.Declared = true;
  stands.Ground.Lat = kLatDeg;
  stands.Ground.Lon = kLonDeg;
  stands.Render.Declared = true;
  stands.Render.Frame = outshine::Extent{kWidePx, kHighPx};
  stands.Render.Fill = 0.6;
  stands.Lit.Declared = true;
  stands.Lit.Key.Lux = 40000.0;
  stands.Lit.Key.ElevationDeg = kSunElevationDeg;
  stands.Lit.Key.BearingDeg = kSunBearingDeg;

  outshine::View watches;
  watches.Id = "station";
  watches.Person = "first";
  watches.Stands.GlobeAnchor = true;
  watches.Stands.LatitudeDeg = 35.6595;
  watches.Stands.LongitudeDeg = 139.7005;
  watches.Stands.HeightM = 25.0;
  watches.Stands.BearingDeg = 40.0;
  watches.Stands.PitchDeg = -8.0;
  watches.FovDeg = 70.0;
  stands.Views.push_back(watches);

  if (!engine.Declare(stands) || !engine.Assemble()) {
    Unprepared((std::string("Shibuya needs terrain and OSM tiles and this machine has none "
                            "cached: ") +
                engine.Error())
                   .c_str());
    return Report();
  }

  for (int step = 0; step < kSteps; ++step) {
    if (!engine.Advance() || !engine.RenderTo(outshine::Extent{})) {
      Unprepared((std::string("Shibuya did not advance: ") + engine.Error()).c_str());
      return Report();
    }
  }

  const auto measured = [&engine](const char *what) {
    for (const outshine::Measure &held : engine.Numbers()) {
      if (held.What == what) { return held.How; }
    }
    return 0.0;
  };
  const double tiles = measured("tiles the ring laid");
  const double triangles = measured("subjects, triangles");

  std::vector<uint8_t> rgba;
  const bool read = engine.Pixels(rgba);
  const size_t apart = read ? Colours(rgba) : 0;

  std::error_code failed;
  std::filesystem::create_directories("build/places", failed);
  const std::string kept = "build/places/Shibuya.png";
  const bool wrote = engine.Capture(kept);
  std::printf("%s  %.0f tile(s), %.0f triangle(s), %zu of %zu pixel(s) differ, kept at %s\n",
              kPlace, tiles, triangles, apart, rgba.size() / 4u,
              wrote ? kept.c_str() : engine.Error().c_str());

  CHECK(read && apart > 0,
        "**SOMETHING IS DRAWN**: a declaration the engine accepts and renders as one flat colour "
        "is the quietest kind of wrong. This is the weakest of the three checks and on its own it "
        "is a FALSE FLOOR -- the sky gradient alone satisfies it, which is what the first version "
        "of this case measured and called a picture of a place");

  CHECK(tiles > 0.0,
        "**AND THE PLACE IS THERE**: the ring laid at least one terrain tile. Without this the "
        "engine composes an empty world, draws the sky over it, and every pixel check above still "
        "passes -- a green that means the opposite of what it looks like");

  CHECK(triangles > 0.0,
        "and GEOMETRY reached the picture: tiles laid but no triangle drawn is the other half of "
        "the same silence, and the two together are the floor beneath any judgement about how the "
        "place LOOKS");

  CHECK(wrote,
        "and the picture is KEPT, in build/ where build artefacts go and never in the tree, so "
        "there is something to look at when the answer to `is it any good yet` is wanted");

  Covers("a declared place on Earth stands, advances and renders a picture of more than one "
         "colour -- the floor beneath a judgement about how it looks, and never that judgement");
  return Report();
}
