#include <cstdio>
#include <cstdlib>
#include <string>

#include <SDL3/SDL.h>

#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

// THE GROUND AT A PLACE, ASKED THROUGH THE DOOR.
//
// The goal this door is measured against names the case in words: put a player at GPS coordinates
// on the ground, and a client that has to SEARCH for how is a door finding rather than a learning
// problem. Cesium answers `sampleHeightMostDetailed`; Unreal traces down onto the landscape. In
// both, a client ASKS and never computes.
//
// Before this verb a client had to declare a view carrying `SamplesHeight`, advance the engine and
// read a published measure back -- which is a client reaching into an engine's instrumentation for
// a fact the engine already knows.
//
// WHAT THIS CASE DOES NOT DECIDE: nothing about how the terrain LOOKS, and nothing about the
// height's accuracy against a survey. It decides that the door answers, that the answer is the
// place's own order of magnitude, and that a refusal is a refusal.

namespace {

constexpr double kRimLatDeg = 36.0616;
constexpr double kRimLonDeg = -112.1076;

// Mather Point stands at about 2 100 m and the Colorado at about 760 m, so a rim answer belongs
// between them and well above either shoulder. These are the place's own bounds and not a tolerance
// chosen to make the case pass.
constexpr double kLeastM = 1500.0;
constexpr double kMostM = 2600.0;

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start");
    return Report();
  }

  outshine::Engine engine;
  engine.setRoots(outshine::Roots{"src/assets/drive", "src/assets", "/tmp/outshine-drive-cache", false});

  double before = 0.0;
  const bool answeredEarly = engine.sampleHeight(kRimLatDeg, kRimLonDeg, before);
  std::printf("BEFORE A WORLD STANDS  %s -- %s\n", answeredEarly ? "answered" : "refused",
              answeredEarly ? "" : engine.error().c_str());

  CHECK(!answeredEarly,
        "**A HEIGHT NOBODY HAS IS NOT A NUMBER TO INVENT**: asked before a world stands, the door "
        "REFUSES and says why. This is the negative control for the check below -- a door that "
        "answered 0.0 here would pass that one by accident, and a client would build on a sea-level "
        "lie");

  if (!engine.drawsInto(outshine::Extent{320, 180})) {
    Unprepared("the device stood no canvas");
    return Report();
  }

  outshine::Scenario stands;
  stands.Ground.Declared = true;
  stands.Ground.Origin.LatitudeDeg = kRimLatDeg;
  stands.Ground.Origin.LongitudeDeg = kRimLonDeg;
  stands.Ground.PatienceS = 3.0;
  stands.Render.Declared = true;
  stands.Render.Frame = outshine::Extent{320, 180};

  if (!engine.declare(stands) || !engine.assemble()) {
    Unprepared((std::string("the canyon needs terrain and this machine has none cached: ") +
                engine.error())
                   .c_str());
    return Report();
  }
  (void)engine.preload(15.0);

  double rimM = 0.0;
  const bool answered = engine.sampleHeight(kRimLatDeg, kRimLonDeg, rimM);
  std::printf("MATHER POINT           %s %.1f m\n", answered ? "reads" : "refused", rimM);

  CHECK(answered,
        "and once a world stands the door ANSWERS: a client says a longitude and a latitude and "
        "gets the ground, without declaring a view, advancing a frame or reading a measure back");

  CHECK(answered && rimM > kLeastM && rimM < kMostM,
        "**AND IT IS THE PLACE'S OWN GROUND**: Mather Point stands at about 2 100 m and the "
        "Colorado at about 760 m, so a rim answer belongs between 1 500 and 2 600. A door that "
        "answered any number would satisfy the check above; this is what makes that answer the "
        "CANYON's rather than an arithmetic accident");

  Covers("the door answers the height of the ground at a place, and refuses when the terrain "
         "there is not resident -- Cesium's sampleHeight, which is how a client puts a player on "
         "the ground without computing it");
  return Report();
}
