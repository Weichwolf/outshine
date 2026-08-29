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

// OldTown -- A PLACE, RENDERED. This is not a proof and it scores nothing.
//
// `places/` exists so the engine's visual state is VISIBLE: every gate drops six pictures into
// build/places/ and an eye decides. The only thing a case here refuses on is not getting a picture
// out at all -- a declaration that will not stand, a world that will not compose, a frame that
// cannot be written. Whether the picture is any GOOD is the owner's judgement and never a number
// invented here.
//
// THE SIX ARE COMPARABLE BY CONSTRUCTION. Same eye height above the GROUND, same sun, same lens,
// same frame. Above the ground rather than above the sea, and the reason is arithmetic: at 720 px
// over 55 deg a pixel is 0.076 deg, so a 12 m building needs to be within about 3 km to cover three
// of them -- and from 4 000 m ASL at -15 deg of pitch the nearest ground is 14.9 km away, so a town
// is sub-pixel by construction. One height above sea level cannot show both a canyon and a street. Only the place and the bearing change, so a difference between two pictures is a
// difference between two places or a defect -- never the clock. The sun is declared at 60 deg of
// elevation bearing 180 deg rather than taken from the hour, because a real-time sun makes two
// pictures incomparable the moment they are rendered a few minutes apart.
//
// WHAT I EXPECT TO SEE, written before looking.
//   A walled town on a plateau above the Tauber. Relief is about 150 m -- small enough that the
//   town, not the ground, has to carry the picture.

namespace {

constexpr const char *kPlace = "OldTown";
constexpr int kWidePx = 1280;
constexpr int kHighPx = 720;
constexpr int kSteps = 2;
constexpr double kPatienceS = 15.0;
constexpr double kSightM = 240000.0;
constexpr double kLatDeg = 49.3777;
constexpr double kLonDeg = 10.179;
constexpr double kBearingDeg = 70.0;

constexpr double kEyeAglM = 60.0;
constexpr double kPitchDeg = -6.0;
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
  stands.Ground.Origin.LatitudeDeg = kLatDeg;
  stands.Ground.Origin.LongitudeDeg = kLonDeg;
  stands.Ground.PatienceS = 3.0;
  stands.Ground.SightM = kSightM;
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
  watches.Sees.Stands.Geodetic.LatitudeDeg = kLatDeg;
  watches.Sees.Stands.Geodetic.LongitudeDeg = kLonDeg;
  watches.Sees.Stands.Geodetic.HeightM = kEyeAglM;
  watches.Sees.Stands.SamplesHeight = true;
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

  // PRELOAD, THEN ONE PICTURE. The engine never blocks on IO, so a client that wants a finished
  // frame rather than a progressively refining one says so ONCE, bounded in seconds, before it
  // draws. That is Filament's `flushAndWait` distinction: the wait belongs to the client's call,
  // never to the frame path.
  const auto asked = std::chrono::steady_clock::now();
  const bool ready = engine.preload(kPatienceS).has_value();
  const auto stood = std::chrono::steady_clock::now();
  int frames = 0;
  double advancingMs = 0.0, renderingMs = 0.0;
  for (; frames < kSteps; ++frames) {
    const auto beforeStep = std::chrono::steady_clock::now();
    if (!engine.advance()) {
      Unprepared((std::string(kPlace) + " did not advance: " + engine.error()).c_str());
      return Report();
    }
    const auto stepped = std::chrono::steady_clock::now();
    if (!engine.renderer().render(outshine::Extent{})) {
      Unprepared((std::string(kPlace) + " did not render: " + engine.error()).c_str());
      return Report();
    }
    const auto drawn = std::chrono::steady_clock::now();
    advancingMs += std::chrono::duration<double, std::milli>(stepped - beforeStep).count();
    renderingMs += std::chrono::duration<double, std::milli>(drawn - stepped).count();
  }
  const auto drew = std::chrono::steady_clock::now();
  const double standingMs = std::chrono::duration<double, std::milli>(asked - began).count();
  const double loadingMs = std::chrono::duration<double, std::milli>(stood - asked).count();
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
  const bool wrote = engine.renderer().saveScreenshot(kept).has_value();

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
  std::printf("    %.0f rebuild(s) of the terrain over %.0f walk(s) that asked\n",
              measured("times the terrain was rebuilt"), measured("and how often it was asked about"));
  std::printf("    the last rebuild took %.0f ms; of %d frame(s), advance %.0f ms and render %.0f ms\n",
              measured("and what the last rebuild took"), frames, advancingMs, renderingMs);
  std::printf("    %.0f building triangle(s) meshed from OSM footprints\n",
              measured("building triangles the world meshed"));
  std::printf("    the ring's nearest vertex is %.0f m out at %.0f m up; the eye is %.0f m up\n",
              measured("the ring's nearest vertex to the frame origin"), measured("and its up"),
              measured("the eye, up"));
  std::printf("    buildings from %.0f to %.0f m up, %.0f to %.0f m out\n",
              measured("buildings stand between"), measured("and"),
              measured("their nearest vertex lies"), measured("their farthest"));
  std::printf("    cascade: %.0f level(s), %.0f tile(s) skipped as covered, %.0f OVERLAPPING a finer level\n",
              measured("levels the cascade laid"), measured("tiles it skipped as already covered"),
              measured("tiles that overlap a finer level"));
  std::printf("    seam: %.0f shared vertices, %.2f m apart in height, %.2f deg apart in normal; "
              "%.0f street(s), %.0f footprint(s), %.0f instanced; kept at %s\n",
              measured("vertices two tiles put in the same place"),
              measured("and the widest they disagree on height"),
              measured("the widest their NORMALS disagree"), measured("streets the world holds"),
              measured("building footprints it holds"), measured("instances its draw sources made"),
              wrote ? kept.c_str() : engine.error().c_str());

  // THE THREE ARE SEPARATE AND WERE NOT. This clock used to start BEFORE `preload`, so the
  // client's own wait for the world to arrive was divided by the frame count and reported as a
  // frame time: 3 138 ms a frame, on a frame that costs 12. Standing, loading and drawing are
  // three different questions and a single stopwatch across all three answers none of them.
  std::printf("    THE TIME IS NOT THE PICTURE: %.0f ms to stand, %.0f ms waiting for the world, "
              "%.1f ms to draw %d frame(s) (%.2f ms each). %.0f pending, %.0f absent, %.0f refused\n",
              standingMs, loadingMs, drawingMs, frames, drawingMs / (double)(frames > 0 ? frames : 1),
              measured("tiles it is still waiting for"), measured("tiles the stack does not hold"),
              measured("tiles it refused"));
  std::printf("    %.0f tile(s) stood BARE on the ellipsoid because the ground had not arrived\n",
              measured("tiles laid bare on the ellipsoid"));
  std::printf("    %s -- loaded %.0f%% of what the view wants, %d frame(s) drawn\n",
              ready ? "PRELOADED" : "PATIENCE RAN OUT", 100.0 * engine.loadProgress(),
              frames);

  for (const outshine::Measure &held : engine.measures()) {
    if (held.What.rfind("zoom ", 0) == 0 || held.What.rfind("mesh jobs", 0) == 0 ||
        held.What.rfind("fetches", 0) == 0 || held.What.rfind("jobs it", 0) == 0 ||
        held.What.rfind("asks that", 0) == 0 || held.What.rfind("megabytes", 0) == 0 ||
        held.What.rfind("jobs still", 0) == 0 || held.What.rfind("keys with", 0) == 0 ||
        held.What.rfind("jobs parked", 0) == 0 || held.What.rfind("results it", 0) == 0 ||
        held.What.rfind("jobs waiting", 0) == 0 || held.What.rfind("mesh jobs it dropped", 0) == 0 || held.What.rfind("lighting:", 0) == 0 || held.What.rfind("generators:", 0) == 0 || held.What.rfind("buildings:", 0) == 0 || held.What.rfind("streets:", 0) == 0 || held.What.rfind("water:", 0) == 0 || held.What.rfind("the ring's vertices a land", 0) == 0 ||
        held.What.rfind("out of, for a class", 0) == 0 ||
        held.What.rfind("restand:", 0) == 0) {
      std::printf("    %s: %.0f %s\n", held.What.c_str(), held.How, held.Unit.c_str());
    }
  }

  CHECK(wrote,
        "**THERE IS A PICTURE**: the only thing this case refuses on. A place that declares, "
        "composes, advances and writes its frame has done its whole job here -- what the frame "
        "SHOWS is the owner's to judge, and no number invented in this file may stand in for that");

  Covers("a declared place on Earth stands, advances and leaves a picture in build/places for an "
         "eye -- never a judgement about how that picture looks");
  return Report();
}
