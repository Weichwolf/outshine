#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

// ROTHENBURG OB DER TAUBER AT MIDDAY -- a narrow alley whose shaded wall is lit by sky alone.
//
// THIS IS NOT A PROOF AND SAYS SO. What it asserts is that the declaration STANDS, that it
// advances, and that a picture comes out of it carrying more than one colour. Whether the picture
// is any GOOD is the owner's judgement and not a number this case may invent -- the same line
// ScoreWhatTheDriveMeasures draws. The screenshot lands in `build/places/` for an eye to look at,
// and `build/` is a build artefact directory rather than the tree.
//
// WHAT THIS PLACE IS HERE TO EXPOSE, measured rather than imagined:
// the sky DOES light it (board:2013) and the term is FLAT -- one radiance for every normal, so
// the wall facing the bright half of the sky and the wall facing away receive the same
//
// WHAT I EXPECT TO SEE, written before looking.
//   Dense footprints, narrow gaps, a high sun. The sunlit walls should be fine. The prediction that
//   matters is the SHADED wall: it is lit by the sky's irradiance and that term is one radiance for
//   every normal, so a wall facing the bright half of the sky and a wall facing away should look
//   THE SAME. If they differ, my reading of SubjectEnvironment is wrong and board:2013 is wrong
//   with it.

namespace {

constexpr const char *kPlace = "OldTown";
constexpr int kWidePx = 1280;
constexpr int kHighPx = 720;
constexpr int kSteps = 60;
constexpr double kLatDeg = 49.3777;
constexpr double kLonDeg = 10.179;

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
  engine.setRoots(outshine::Roots{"apps/driver/src", "src/assets", "/tmp/outshine-drive-cache", false});
  if (!engine.drawsInto(outshine::Extent{kWidePx, kHighPx})) {
    Unprepared("the device stood no canvas");
    return Report();
  }

  outshine::Scenario stands;
  stands.Ground.Declared = true;
  stands.Ground.Lat = kLatDeg;
  // A COLD FETCH IS BOUNDED BY THE DECLARATION, not by the runner's axe. WorldSettings carries
  // PatienceS and nothing here had used it: a place whose tiles are not cached must SAY so in
  // seconds rather than be killed at 120 and report nothing (board:1778, board:2009).
  stands.Ground.PatienceS = 3.0;
  stands.Ground.Lon = kLonDeg;
  stands.Render.Declared = true;
  stands.Render.Frame = outshine::Extent{kWidePx, kHighPx};
  stands.Render.Fill = 0.6;
  stands.Lit.Declared = true;
  stands.Lit.Key.Lux = 40000.0;

  outshine::View watches;
  watches.Id = "station";
  watches.Person = "first";
  watches.Sees.Stands.GlobeAnchor = true;
  watches.Sees.Stands.LatitudeDeg = 49.3777;
  watches.Sees.Stands.LongitudeDeg = 10.179;
  watches.Sees.Stands.HeightM = 2000.0;
  watches.Sees.Stands.SamplesHeight = true;
  watches.Sees.Stands.BearingDeg = 70.0;
  watches.Sees.Stands.PitchDeg = -35.0;
  watches.Sees.FovDeg = 70.0;
  stands.Views.push_back(watches);

  if (!engine.declare(stands) || !engine.assemble()) {
    Unprepared((std::string("OldTown needs terrain and OSM tiles and this machine has none "
                            "cached: ") +
                engine.error())
                   .c_str());
    return Report();
  }

  for (int step = 0; step < kSteps; ++step) {
    if (!engine.advance() || !engine.render(outshine::Extent{})) {
      Unprepared((std::string("OldTown did not advance: ") + engine.error()).c_str());
      return Report();
    }
  }

  const auto measured = [&engine](const char *what) {
    for (const outshine::Measure &held : engine.measures()) {
      if (held.What == what) { return held.How; }
    }
    return 0.0;
  };
  const double tiles = measured("tiles the ring laid");
  const double triangles = measured("subjects, triangles");
  const double relief = measured("so the relief it carries");
  const double ways = measured("streets the world holds");
  const double placed = measured("bodies the world's generators placed");
  const double instanced = measured("instances its draw sources made");
  const double prints = measured("building footprints it holds");
  const double spans = measured("and the ground it spans, east to west");
  const double adrift = measured("vertices more than 500 m from that average");
  const double vertices = measured("out of");
  std::printf("    %s  mean %.1f m, %.0f of %.0f vertices more than 500 m from it\n", kPlace,
              measured("the height its vertices average"), adrift, vertices);

  std::vector<uint8_t> rgba;
  const bool read = engine.readPixels(rgba);
  const size_t apart = read ? Colours(rgba) : 0;

  std::error_code failed;
  std::filesystem::create_directories("build/places", failed);
  const std::string kept = "build/places/OldTown.png";
  const bool wrote = engine.saveScreenshot(kept);
  std::printf("%s  %.0f tile(s), %.0f triangle(s), %.0f m relief over %.0f m, %.0f street(s), %.0f footprint(s), %.0f placed, %.0f instanced, %zu of %zu pixel(s) differ, kept at %s\n",
              kPlace, tiles, triangles, relief, spans, ways, prints, placed, instanced, apart, rgba.size() / 4u,
              wrote ? kept.c_str() : engine.error().c_str());

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
