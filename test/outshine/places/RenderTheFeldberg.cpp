#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <chrono>

#include <SDL3/SDL.h>

#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"
#include "LogSinks.h"
#include "TextTarget.h"

// BlackForest -- A PLACE, RENDERED. This is not a proof and it scores nothing.
//
// `places/` exists so the engine's visual state is VISIBLE: every gate drops six pictures into
// build/places/ and an eye decides. The only thing a case here refuses on is not getting a picture
// out at all -- a declaration that will not stand, a world that will not compose, a frame that
// cannot be written. Whether the picture is any GOOD is the owner's judgement and never a number
// invented here.
//
// THE SIX ARE COMPARABLE BY CONSTRUCTION. Same eye height above SEA LEVEL, same sun, same lens,
// same frame. Only the place and the bearing change, so a difference between two pictures is a
// difference between two places or a defect -- never the clock. The sun is declared at 60 deg of
// elevation bearing 180 deg rather than taken from the hour, because a real-time sun makes two
// pictures incomparable the moment they are rendered a few minutes apart.
//
// WHAT I EXPECT TO SEE, written before looking.
//   Rounded hills, 846 m of relief, and only 185 footprints. Whatever this place shows is TERRAIN,
//   because there is almost nothing else here to show.

namespace {

constexpr const char *kPlace = "BlackForest";
constexpr int kWidePx = 1280;
constexpr int kHighPx = 720;
constexpr int kSteps = 60;
constexpr double kLatDeg = 47.8736;
constexpr double kLonDeg = 8.0044;
constexpr double kBearingDeg = 90.0;

constexpr double kEyeAslM = 4000.0;
constexpr double kPitchDeg = -15.0;
constexpr double kFovDeg = 55.0;
constexpr double kSunElevationDeg = 60.0;
constexpr double kSunBearingDeg = 180.0;

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so nothing can be drawn");
    return Report();
  }

  // THE ENGINE SAYS WHAT IT IS DOING. `Log::Sink_` is null by default, so every Log::Warn in the
  // tree writes to nowhere unless a sink is attached -- which is why a 122 s stall in this suite
  // was silent and read as a slow fetch. One place in the whole tree attached one before this.
  outshine::TextTarget saying(outshine::TextStream::Stdout);
  outshine::TextLogSink telling(saying);
  outshine::LogSinkScope listening(&telling);

  outshine::Engine engine;
  engine.setRoots(outshine::Roots{"apps/driver/src", "src/assets", "/tmp/outshine-drive-cache", false});
  if (!engine.drawsInto(outshine::Extent{kWidePx, kHighPx})) {
    Unprepared("the device stood no canvas");
    return Report();
  }

  outshine::Scenario stands;
  stands.Ground.Declared = true;
  stands.Ground.Lat = kLatDeg;
  stands.Ground.Lon = kLonDeg;
  stands.Ground.PatienceS = 3.0;
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
  watches.Sees.Stands.GlobeAnchor = true;
  watches.Sees.Stands.LatitudeDeg = kLatDeg;
  watches.Sees.Stands.LongitudeDeg = kLonDeg;
  watches.Sees.Stands.HeightM = kEyeAslM;
  watches.Sees.Stands.SamplesHeight = false;
  watches.Sees.Stands.BearingDeg = kBearingDeg;
  watches.Sees.Stands.PitchDeg = kPitchDeg;
  watches.Sees.FovDeg = kFovDeg;
  stands.Views.push_back(watches);

  const auto began = std::chrono::steady_clock::now();
  if (!engine.declare(stands) || !engine.assemble()) {
    Unprepared((std::string(kPlace) + " needs terrain and OSM tiles and this machine has none "
                                      "cached: " + engine.error()).c_str());
    return Report();
  }

  const auto stood = std::chrono::steady_clock::now();
  for (int step = 0; step < kSteps; ++step) {
    if (!engine.advance() || !engine.render(outshine::Extent{})) {
      Unprepared((std::string(kPlace) + " did not advance: " + engine.error()).c_str());
      return Report();
    }
  }

  const auto drew = std::chrono::steady_clock::now();
  const double standingMs =
      std::chrono::duration<double, std::milli>(stood - began).count();
  const double drawingMs = std::chrono::duration<double, std::milli>(drew - stood).count();

  const auto measured = [&engine](const char *what) {
    for (const outshine::Measure &held : engine.measures()) {
      if (held.What == what) { return held.How; }
    }
    return 0.0;
  };

  std::error_code failed;
  std::filesystem::create_directories("build/places", failed);
  const std::string kept = std::string("build/places/") + kPlace + ".png";
  const bool wrote = engine.saveScreenshot(kept);

  std::printf("%s  %.0f tile(s) over %.0f levels, %.0f triangle(s), %.0f m relief, reach %.1f km\n",
              kPlace, measured("tiles the ring laid"), measured("levels the cascade laid"),
              measured("subjects, triangles"), measured("so the relief it carries"),
              measured("its farthest vertex") / 1000.0);
  std::printf("    sun %.0f deg up bearing %.0f, eye %.0f m ASL, curvature %.2f m at %.0f m where a "
              "sphere says %.2f m\n",
              measured("the sun stands this high"), measured("and bears"),
              measured("the eye, up"),
              measured("the ring's vertex that sinks furthest below its own altitude"),
              measured("and how far out it lies"), measured("a sphere would sink it by"));
  std::printf("    seam: %.0f shared vertices, %.2f m apart in height, %.2f deg apart in normal; "
              "%.0f street(s), %.0f footprint(s), %.0f instanced; kept at %s\n",
              measured("vertices two tiles put in the same place"),
              measured("and the widest they disagree on height"),
              measured("the widest their NORMALS disagree"), measured("streets the world holds"),
              measured("building footprints it holds"), measured("instances its draw sources made"),
              wrote ? kept.c_str() : engine.error().c_str());

  std::printf("    THE TIME IS NOT THE PICTURE: %.0f ms to stand the world, %.1f ms to draw %d "
              "frames (%.2f ms each). %.0f tile(s) still pending, %.0f absent, %.0f refused\n",
              standingMs, drawingMs, kSteps, drawingMs / (double)kSteps,
              measured("tiles it is still waiting for"), measured("tiles the stack does not hold"),
              measured("tiles it refused"));
  std::printf("    %.0f tile(s) stood BARE on the ellipsoid because the ground had not arrived\n",
              measured("tiles laid bare on the ellipsoid"));

  CHECK(wrote,
        "**THERE IS A PICTURE**: the only thing this case refuses on. A place that declares, "
        "composes, advances and writes its frame has done its whole job here -- what the frame "
        "SHOWS is the owner's to judge, and no number invented in this file may stand in for that");

  Covers("a declared place on Earth stands, advances and leaves a picture in build/places for an "
         "eye -- never a judgement about how that picture looks");
  return Report();
}
