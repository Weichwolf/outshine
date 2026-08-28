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
constexpr int kSteps = 60;
constexpr double kLatDeg = 36.0616;
constexpr double kLonDeg = -112.1076;

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
  watches.Sees.Stands.LatitudeDeg = 36.0616;
  watches.Sees.Stands.LongitudeDeg = -112.1076;
  watches.Sees.Stands.HeightM = 2000.0;
  watches.Sees.Stands.SamplesHeight = true;
  watches.Sees.Stands.BearingDeg = 330.0;
  watches.Sees.Stands.PitchDeg = -35.0;
  watches.Sees.FovDeg = 70.0;
  stands.Views.push_back(watches);

  if (!engine.declare(stands) || !engine.assemble()) {
    Unprepared((std::string("GrandCanyon needs terrain and OSM tiles and this machine has none "
                            "cached: ") +
                engine.error())
                   .c_str());
    return Report();
  }

  for (int step = 0; step < kSteps; ++step) {
    if (!engine.advance() || !engine.render(outshine::Extent{})) {
      Unprepared((std::string("GrandCanyon did not advance: ") + engine.error()).c_str());
      return Report();
    }
  }

  const auto measured = [&engine](const char *what) {
    for (const outshine::Measure &held : engine.measures()) {
      if (held.What == what) { return held.How; }
    }
    return 0.0;
  };
  for (const outshine::Measure &held : engine.measures()) {
    if (held.What.find(", took") != std::string::npos || held.What.find("stages") != std::string::npos ||
        held.What.find("passes") != std::string::npos) {
      std::printf("    ROW  %-40s %.3f\n", held.What.c_str(), held.How);
    }
  }
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
  std::printf("    %s  spans %.0f m east, %.0f m north -- mean %.1f m, %.0f of %.0f vertices more than 500 m from it\n", kPlace,
              measured("and the ground it spans, east to west"), measured("north to south"),
              measured("the height its vertices average"), adrift, vertices);

  std::printf("    %s  eye at %.1f east / %.1f up / %.1f south -- ring nearest %.1f m at %.1f m up, farthest %.1f m at %.1f m up\n",
              kPlace, measured("the eye, east"), measured("the eye, up"), measured("the eye, south"),
              measured("the ring's nearest vertex to the frame origin"), measured("and its up"),
              measured("its farthest vertex"), measured("and THAT one's up"));

  std::printf("    %s  CURVATURE: farthest sink %.2f m at %.0f m out, a sphere says %.2f m\n",
              kPlace, measured("the ring's vertex that sinks furthest below its own altitude"),
              measured("and how far out it lies"), measured("a sphere would sink it by"));

  std::printf("    %s  clusters %.0f held / %.0f drawn, worst error %.1f m -- skirt is twice that\n",
              kPlace, measured("clusters the ring holds"), measured("clusters it drew"),
              measured("the worst error any of them carries"));

  std::vector<uint8_t> rgba;
  const bool read = engine.readPixels(rgba);
  const size_t apart = read ? Colours(rgba) : 0;

  std::error_code failed;
  std::filesystem::create_directories("build/places", failed);
  const std::string kept = "build/places/GrandCanyon.png";
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
