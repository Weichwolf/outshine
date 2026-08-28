#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

// MATHER POINT AT SUNSET -- kilometres of terrain, a shadow thrown the length of it, aerial
// perspective doing most of the work.
//
// THIS IS NOT A PROOF AND SAYS SO. What it asserts is that the declaration STANDS, that it
// advances, and that a picture comes out of it carrying more than one colour. Whether the picture
// is any GOOD is the owner's judgement and not a number this case may invent -- the same line
// ScoreWhatTheDriveMeasures draws. The screenshot lands in `build/places/` for an eye to look at,
// and `build/` is a build artefact directory rather than the tree.
//
// WHAT THIS PLACE IS HERE TO EXPOSE, measured rather than imagined:
// the atmosphere is the strongest system in this tree and the shadow has ONE cascade
// (board:1943), so a shadow over kilometres is one map's worth of resolution
//
// WHAT I EXPECT TO SEE, written before looking.
//   The best of the five. Terrain from DEM tiles at a low sun, and the atmosphere is the strongest
//   system in this tree -- aerial perspective and a warm horizon should do most of the work and
//   should look genuinely right. What I expect to be WRONG is the shadow: one cascade over
//   kilometres means the canyon's own shadow is either absent or blocky. And the rock is a flat
//   ground material, so the strata that make the place will not be there.

namespace {

constexpr const char *kPlace = "GrandCanyon";
constexpr int kWidePx = 1280;
constexpr int kHighPx = 720;
constexpr int kSteps = 8;
constexpr double kLatDeg = 36.0616;
constexpr double kLonDeg = -112.1076;
constexpr double kSunElevationDeg = 4.0;
constexpr double kSunBearingDeg = 250.0;

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

  if (!engine.Declare(stands) || !engine.Assemble()) {
    Unprepared((std::string("GrandCanyon needs terrain and OSM tiles and this machine has none "
                            "cached: ") +
                engine.Error())
                   .c_str());
    return Report();
  }

  for (int step = 0; step < kSteps; ++step) {
    if (!engine.Advance() || !engine.RenderTo(outshine::Extent{})) {
      Unprepared((std::string("GrandCanyon did not advance: ") + engine.Error()).c_str());
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
  const std::string kept = "build/places/GrandCanyon.png";
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
