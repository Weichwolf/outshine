#ifndef OUTSHINE_TEST_RENDEREDPLACE_H
#define OUTSHINE_TEST_RENDEREDPLACE_H

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"
#include "LogSinks.h"
#include "TextTarget.h"

// ONE PROGRAM, SIX PLACES. The six cases were six copies of the same 263 lines differing in a name
// and three numbers, so every change to the shared part was made six times or -- worse -- five.
// THE ONLY THING A PLACE MAY DECLARE IS WHERE THE CAMERA STANDS AND WHERE IT LOOKS. Anything else
// varying between two of them would make their pictures incomparable, which is the whole reason the
// directory exists.
//
// `places/` exists so the engine's visual state is VISIBLE: every gate drops the pictures into
// build/places/ and an EYE decides. The only thing a case here refuses on is not getting a picture
// out at all -- a declaration that will not stand, a world that will not compose, a frame that
// cannot be written, or a tile left bare on the ellipsoid. Whether the picture is any GOOD is the
// owner's judgement and never a number invented here.
//
// THEY ARE COMPARABLE BY CONSTRUCTION. Same eye height above the GROUND, same sun, same lens, same
// frame. Above the ground rather than above the sea, and the reason is arithmetic: at 720 px over
// 55 deg a pixel is 0.076 deg, so a 12 m building needs to be within about 3 km to cover three of
// them -- and from 4 000 m ASL at -15 deg of pitch the nearest ground is 14.9 km away, so a town is
// sub-pixel by construction. One height above sea level cannot show both a canyon and a street. The
// sun is DECLARED at 60 deg of elevation bearing 180 rather than taken from the hour, because a
// real-time sun makes two pictures incomparable the moment they are rendered minutes apart.

namespace outshine::Test {

struct Place {
  const char *Name = "";
  double LatDeg = 0.0;
  double LonDeg = 0.0;
  double BearingDeg = 0.0;
};

namespace Placed {

constexpr int kWidePx = 1280;
constexpr int kHighPx = 720;

constexpr double kPatienceS = 15.0;
constexpr double kSightM = 240000.0;
constexpr double kEyeAglM = 60.0;
constexpr double kPitchDeg = -6.0;
constexpr double kFovDeg = 55.0;
constexpr double kSunElevationDeg = 60.0;
constexpr double kSunBearingDeg = 180.0;

}

inline int RenderPlace(const Place &place) {
  using namespace outshine::Test::Placed;

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
  engine.setRoots(outshine::Roots{"src/assets/drive", "src/assets", "/tmp/outshine-drive-cache", false});
  if (!engine.drawsInto(outshine::Extent{kWidePx, kHighPx})) {
    Unprepared("the device stood no canvas");
    return Report();
  }

  outshine::Scenario stands;
  stands.Ground.Declared = true;
  stands.Ground.Origin.LatitudeDeg = place.LatDeg;
  stands.Ground.Origin.LongitudeDeg = place.LonDeg;
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
  watches.Sees.Stands.Geodetic.LatitudeDeg = place.LatDeg;
  watches.Sees.Stands.Geodetic.LongitudeDeg = place.LonDeg;
  watches.Sees.Stands.Geodetic.HeightM = kEyeAglM;
  watches.Sees.Stands.SamplesHeight = true;
  watches.Sees.Stands.BearingDeg = place.BearingDeg;
  watches.Sees.Stands.PitchDeg = kPitchDeg;
  watches.Sees.FovDeg = kFovDeg;
  stands.Views.push_back(watches);

  const auto began = std::chrono::steady_clock::now();
  if (!engine.declare(stands) || !engine.assemble()) {
    Unprepared((std::string(place.Name) + " needs terrain and OSM tiles and this machine has none "
                                      "cached: " + engine.error()).c_str());
    return Report();
  }

  // PRELOAD, THEN ONE PICTURE. The engine never blocks on IO, so a client that wants a finished
  // frame rather than a progressively refining one says so ONCE, bounded in seconds, before it
  // draws. That is Filament's `flushAndWait` distinction: the wait belongs to the client's call,
  // never to the frame path.
  const auto asked = std::chrono::steady_clock::now();
  // A WAIT THE OPERATOR CAN SEE. The load throttled itself for months behind a green PRELOADED and
  // nobody could tell, because one share is a number without a rate beside it. The bar to hold: at
  // least 50 Mbit/s off the wire, and a ring already in the cache costs no wait at all.
  outshine::Loading last;
  const bool ready = engine
                         .preload(kPatienceS,
                                  [&](const outshine::Loading &how) {
                                    if (how.ElapsedS - last.ElapsedS < 0.25 && how.share() < 1.0) {
                                      return;
                                    }
                                    last = how;
                                    std::printf(
                                        "\r    loading  terrain %zu/%zu  osm %zu/%zu  %zu in flight"
                                        "  %.1f MB  %.0f Mbit/s  %.0f ms/fetch  %.1f s   ",
                                        how.GroundArrived, how.GroundWanted, how.VectorArrived,
                                        how.VectorWanted, how.Outstanding, how.FetchedMB,
                                        how.Megabits, how.MeanFetchMs, how.ElapsedS);
                                    std::fflush(stdout);
                                  })
                         .has_value();
  std::printf("\n");
  // THE WAIT AND THE BUILD ARE TWO DIFFERENT COSTS AND ONE STOPWATCH HID IT. `preload` reports its
  // progress on every wake and then, on the wake that settles, BUILDS the world inside the same
  // return statement -- so the reporter's last elapsed is the streaming alone and the clock around
  // the call is streaming plus building. Measured on Shibuya: 3.5 s of stream and 48.7 s of build,
  // at a wire that was doing 90 to 786 Mbit/s. The load is not IO bound and reading one number
  // would have said it was.
  const double streamedS = last.ElapsedS;
  const auto stood = std::chrono::steady_clock::now();
  int frames = 0;
  double advancingMs = 0.0, renderingMs = 0.0;
  // AS MANY FRAMES AS THE PLAN SAYS IT NEEDS. Two frames of a temporal resolve is two samples of a
  // sequence eight long, so every edge in every picture this directory has ever kept was as aliased
  // as no antialiasing at all -- and the number that fixes it was computed by the plan and read by
  // nobody.
  // AND NEVER FEWER THAN TWO, which is not a rounder number but a property of the instruments: the
  // measures are published at advance time and report the frame BEFORE the one being published, so
  // a case that draws once reports `subjects, triangles 0` for a picture full of triangles. Six of
  // them read exactly that for one measurement.
  const int settle = engine.renderer().settleFrames();
  const int wanted = settle > 2 ? settle : 2;
  for (; frames < wanted; ++frames) {
    const auto beforeStep = std::chrono::steady_clock::now();
    if (!engine.advance()) {
      Unprepared((std::string(place.Name) + " did not advance: " + engine.error()).c_str());
      return Report();
    }
    const auto stepped = std::chrono::steady_clock::now();
    if (!engine.renderer().render(outshine::Extent{})) {
      Unprepared((std::string(place.Name) + " did not render: " + engine.error()).c_str());
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
  const std::string kept = std::string("build/places/") + place.Name + ".png";
  // AND THEN A DISTRIBUTION, because two frames is not one. The loop above draws what the PICTURE
  // needs -- a temporal resolve wants its whole sequence -- and two samples of a frame time say
  // nothing about whether a place holds 60 fps: the same Shibuya read 22.65 ms and 15.21 ms on two
  // runs of it. These frames draw the same standing view again and are timed, so the number below
  // is a p50/p95/p99 over a population rather than a coin toss.
  //
  // WHAT IT DOES NOT COVER: the camera STANDS. A moving eye pays for streaming, for re-laying the
  // ring and for cache misses this never sees, so this is the floor of what a place costs and not
  // its worst case (board:1457).
  std::vector<double> heldMs;
  {
    constexpr int kTimedFrames = 120;
    heldMs.reserve((size_t)kTimedFrames);
    for (int at = 0; at < kTimedFrames; ++at) {
      const auto before = std::chrono::steady_clock::now();
      if (!engine.advance() || !engine.renderer().render(outshine::Extent{})) { break; }
      heldMs.push_back(
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - before)
              .count());
    }
    std::sort(heldMs.begin(), heldMs.end());
  }
  const auto quantile = [&heldMs](double share) {
    if (heldMs.empty()) { return 0.0; }
    size_t which = (size_t)(share * (double)(heldMs.size() - 1) + 0.5);
    return heldMs[which < heldMs.size() ? which : heldMs.size() - 1];
  };

  // EVERY NUMBER, WHEN ASKED. The lines below are what this reads by default; a run that is
  // chasing where a frame went needs the rest, and guessing which name to add to a filter is how
  // a measurement goes missing.
  if (std::getenv("OUTSHINE_ALL_MEASURES") != nullptr) {
    for (const outshine::Measure &one : engine.measures()) {
      std::printf("MEASURE %-56s %14.3f %s\n", one.What.c_str(), one.How, one.Unit.c_str());
    }
  }
  const bool wrote = engine.renderer().saveScreenshot(kept).has_value();

  std::printf("%s  %.0f tile(s) over %.0f levels, %.0f triangle(s), %.0f m relief, reach %.1f km\n",
              place.Name, measured("tiles the ring laid"), measured("levels the cascade laid"),
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
  std::printf("    THE FRAME, over %zu drawn from a STANDING eye: p50 %.2f  p95 %.2f  p99 %.2f ms"
              "  (60 fps wants 16.67)\n",
              heldMs.size(), quantile(0.50), quantile(0.95), quantile(0.99));
  std::printf("    %.0f tile(s) stood BARE on the ellipsoid because the ground had not arrived\n",
              measured("tiles laid bare on the ellipsoid"));
  std::printf("    of that wait, %.1f s STREAMED and %.1f s BUILT the world from what arrived\n",
              streamedS, loadingMs / 1000.0 - streamedS);
  std::printf("    %s -- loaded %.0f%% of what the view wants, %d frame(s) drawn\n",
              ready ? "PRELOADED" : "PATIENCE RAN OUT", 100.0 * engine.loadProgress(),
              frames);

  // A PICTURE OF NOTHING IS NOT A PICTURE. This case scores nothing and refuses only when it cannot
  // get a frame out -- and that let a flat green plane through as a PASS while 112 tiles stood bare
  // on the ellipsoid because the ground never arrived. The place then renders at sea level, every
  // building with it, and the frame says "outshine gets everything wrong" when what happened is that
  // the world did not come. A bare tile is the loudest thing this case can see and it now says so.
  const double bare = measured("tiles laid bare on the ellipsoid");
  if (bare > 0.0) {
    char why[256];
    std::snprintf(why, sizeof why,
                  "%s stood %.0f tile(s) BARE on the ellipsoid -- the elevation never arrived, so "
                  "the ground and everything on it is drawn at sea level. The picture is of the "
                  "streaming, not of the place",
                  place.Name, bare);
    Unprepared(why);
    return Report();
  }

  for (const outshine::Measure &held : engine.measures()) {
    if (held.What.rfind("zoom ", 0) == 0 || held.What.rfind("mesh jobs", 0) == 0 ||
        held.What.rfind("fetches", 0) == 0 || held.What.rfind("jobs it", 0) == 0 ||
        held.What.rfind("asks that", 0) == 0 || held.What.rfind("megabytes", 0) == 0 ||
        held.What.rfind("jobs still", 0) == 0 || held.What.rfind("keys with", 0) == 0 ||
        held.What.rfind("jobs parked", 0) == 0 || held.What.rfind("results it", 0) == 0 ||
        held.What.rfind("jobs waiting", 0) == 0 || held.What.rfind("mesh jobs it dropped", 0) == 0 || held.What.rfind("cull:", 0) == 0 || held.What.rfind("solid:", 0) == 0 || held.What.rfind("relief:", 0) == 0 || held.What.rfind("lighting:", 0) == 0 || held.What.rfind("generators:", 0) == 0 || held.What.rfind("buildings:", 0) == 0 || held.What.rfind("streets:", 0) == 0 || held.What.rfind("water:", 0) == 0 || held.What.rfind("the ring's vertices a land", 0) == 0 ||
        held.What.rfind("out of, for a class", 0) == 0 ||
        held.What.rfind("restand:", 0) == 0 ||
        held.What.rfind("rebuild:", 0) == 0) {
      std::printf("    %s: %.0f %s\n", held.What.c_str(), held.How, held.Unit.c_str());
    }
  }

  // A PICTURE OF NOTHING PASSED THIS CASE FOR MONTHS, and the guard beside this one -- the bare-tile
  // count -- did not catch it: Shibuya meshed 6.1 M building triangles, reported 0 bare tiles, and
  // kept a 49 KB frame of flat green under a gradient sky. The case only ever asked whether a FILE
  // was written.
  //
  // THE ORACLE NEEDS NO TASTE AND NO INVENTED NUMBER. A bare ellipsoid under a sky is a VERTICAL
  // gradient, and a vertical gradient has exactly ZERO horizontal variation by construction. Every
  // edge a building, a street or a hillside puts in the frame has some. So the mean absolute
  // difference between horizontally adjacent pixels over the WHOLE picture separates the two, and
  // the separation is structural rather than a taste. Over the LOWER HALF it does not: Central Park
  // reads 0.719 across a real Manhattan skyline, because the camera looks over a park and every
  // building in the frame stands ABOVE the horizon. Both halves of a bare frame -- the sky's
  // gradient and the flat ground -- carry zero horizontal variation, so taking the whole frame
  // dilutes nothing and misses nothing. MEASURED at the bar of ONE unit:
  // the bare frame reads 0.719 and the five that hold their place read 2.343 to 3.629, so the gap is
  // a factor of 3.3 -- not the order of magnitude a first draft of this comment claimed. Wide enough
  // to separate them and narrow enough that the number is quoted here rather than assumed.
  //
  // It only bites when geometry was actually meshed, which is what makes it a CONTRADICTION between
  // two measurements rather than a judgement about how the picture looks: triangles were built and
  // the frame does not contain them.
  // AND WHAT IT MUST NEVER BE USED FOR. This number separates a PICTURE from NO PICTURE and nothing
  // else. Between two frames that both show their place it decides nothing, and reading a shift in
  // it as a regression is how a real repair gets reverted: the axis swap that ran over the NORMALS
  // was removed, this statistic moved by hundredths, and the walls came out correctly lit -- the
  // owner saw it, the number could not. While architectural defects are being corrected a changed
  // picture is as likely to be a REPAIR as a fault, so the eye decides and nothing is reverted on a
  // drift in this figure.
  //
  // THE MEASURE'S OWN NEGATIVE CONTROL, and without it the bar of 1.0 is a number nobody checked.
  // A bare ellipsoid under a sky IS a vertical gradient, so the statistic is run first over a
  // gradient built here -- if THAT does not come in far under the bar, the bar separates nothing and
  // every green below it is worthless. Measured on the real thing for comparison: a Shibuya frame
  // that meshed 9.7 M triangles and drew none of them read 0.719, and the five places that do show
  // their place read 1.789 to 2.883.
  const auto varyingAcross = [](const std::vector<uint8_t> &rgba, int wide, int high) {
    double sum = 0.0;
    size_t steps = 0;
    for (int y = 0; y < high; ++y) {
      for (int x = 1; x < wide; ++x) {
        const size_t at = ((size_t)y * (size_t)wide + (size_t)x) * 4u;
        for (int c = 0; c < 3; ++c) {
          const int here = rgba[at + (size_t)c], left = rgba[at - 4u + (size_t)c];
          sum += here > left ? here - left : left - here;
          ++steps;
        }
      }
    }
    return steps > 0 ? sum / (double)steps : 0.0;
  };
  {
    std::vector<uint8_t> gradient((size_t)kWidePx * (size_t)kHighPx * 4u, 255u);
    for (int y = 0; y < kHighPx; ++y) {
      const uint8_t shade = (uint8_t)(255 * y / (kHighPx - 1));
      for (int x = 0; x < kWidePx; ++x) {
        const size_t at = ((size_t)y * (size_t)kWidePx + (size_t)x) * 4u;
        gradient[at] = shade;
        gradient[at + 1] = shade;
        gradient[at + 2] = (uint8_t)(255 - shade);
      }
    }
    const double flatGradient = varyingAcross(gradient, kWidePx, kHighPx);
    std::printf("    control: a bare vertical gradient varies by %.3f of 255 along its rows\n",
                flatGradient);
    CHECK(flatGradient < 0.5,
          "**THE BLANK-FRAME BAR SEPARATES SOMETHING**: the negative control for the measure beside "
          "it. A picture of nothing is a vertical gradient and a vertical gradient has no horizontal "
          "variation at all, so this has to read far below the bar of 1.0 that a real frame is held "
          "to -- and if a future measure lets a gradient through, this goes red before a place does");
  }

  double alongRows = 0.0;
  size_t steps = 0;
  std::vector<uint8_t> pixels;
  const bool read = engine.renderer().readPixels(pixels).has_value();
  if (read && pixels.size() >= (size_t)kWidePx * (size_t)kHighPx * 4u) {
    for (int y = 0; y < kHighPx; ++y) {
      for (int x = 1; x < kWidePx; ++x) {
        const size_t at = ((size_t)y * (size_t)kWidePx + (size_t)x) * 4u;
        for (int c = 0; c < 3; ++c) {
          const int here = pixels[at + (size_t)c], left = pixels[at - 4u + (size_t)c];
          alongRows += here > left ? here - left : left - here;
          ++steps;
        }
      }
    }
  }
  const double flatness = steps > 0 ? alongRows / (double)steps : 0.0;
  const double meshed = measured("building triangles the world meshed");
  std::printf("    the picture varies by %.3f of 255 along its rows, over %.0f meshed building "
              "triangle(s)\n", flatness, meshed);
  if (meshed > 0.0 && read && flatness < 1.0) {
    char why[320];
    std::snprintf(why, sizeof why,
                  "%s meshed %.0f building triangle(s) and its picture varies by %.3f of 255 along "
                  "its rows -- a vertical gradient varies by zero, so the frame holds the sky and "
                  "the ground and NONE of the geometry that was built for it",
                  place.Name, meshed, flatness);
    Unprepared(why);
    return Report();
  }

  CHECK(wrote,
        "**THERE IS A PICTURE**: the only thing this case refuses on. A place that declares, "
        "composes, advances and writes its frame has done its whole job here -- what the frame "
        "SHOWS is the owner's to judge, and no number invented in this file may stand in for that");

  Covers("a declared place on Earth stands, advances and leaves a picture in build/places for an "
         "eye -- never a judgement about how that picture looks");
  return Report();
}

}

#endif
