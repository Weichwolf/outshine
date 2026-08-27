#include <cstdio>
#include <string>

#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"
#include "Live.h"

namespace {

// The oracle is a COST BOUND, and it does not depend on our design: the work a declaration
// causes must be proportional to what the declaration CHANGED, not to how big it is. Both
// benchmarks state it the same way and neither rebuilds a world to change part of one --
// Unreal streams levels in and out of a persistent world with AddToWorld/RemoveFromWorld and
// spawns and destroys actors against it, RAGE swaps one IMAP group for another through a map
// change against a streamed map data store. Neither re-declares.
//
// So: declaring what already stands must build nothing a second time. The instrument is
// outshine::Core::Live::PlanInits(), which counts every time a stand hands a fresh render
// plan to the device -- pipelines, passes and every resource behind them. That count is the
// whole measurement: a rebuild that re-initialises the plan has thrown away everything the
// device held, and one that does not has thrown away nothing.
constexpr int kFramePx = 64;

[[nodiscard]] outshine::Scenario Showing(void) {
  outshine::Scenario made;
  made.Render.Declared = true;
  made.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  made.Lit.Declared = true;
  made.Lit.Key.Lux = 40000.0;
  made.Lit.Key.ElevationDeg = 42.0;
  made.Lit.Key.BearingDeg = 150.0;
  return made;
}

}

int main(int argc, char **argv) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::string under = argc > 1 ? argv[1] : std::string(".");

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so no stand can be judged");
    return Report();
  }

  outshine::Engine engine;
  engine.Under(outshine::Roots{under, "src/assets", "/tmp/outshine-door-cache", true});
  if (!engine.DrawsInto(outshine::Extent{kFramePx, kFramePx})) {
    Unprepared("the device stood no canvas");
    return Report();
  }

  const outshine::Scenario stands = Showing();

  const size_t before = outshine::Core::Live::PlanInits();
  if (!engine.Declare(stands)) {
    Unprepared(("nothing stood: " + engine.Error()).c_str());
    return Report();
  }
  const size_t afterFirst = outshine::Core::Live::PlanInits();
  std::printf("FIRST DECLARATION initialised the plan %zu time(s)\n", afterFirst - before);
  CHECK(afterFirst > before, "the first declaration builds a plan, so there is a stand");

  CHECK(engine.Declare(stands), "declaring what already stands is accepted, not refused");
  const size_t afterSecond = outshine::Core::Live::PlanInits();
  std::printf("SECOND DECLARATION of the SAME scenario initialised the plan %zu further time(s)\n",
              afterSecond - afterFirst);
  CHECK(afterSecond == afterFirst,
        "**DECLARING WHAT ALREADY STANDS REBUILDS NOTHING**: a scenario is a stream and a "
        "declaration is one message on it, so the work it causes is proportional to what it "
        "CHANGED -- an engine that tears its stand down to be handed the same stand again "
        "cannot stream anything, because every part that entered would cost the whole world");

  outshine::Scenario overlaid = stands;
  outshine::Surface page;
  page.Document = "<p>over</p>";
  page.Where = outshine::Patch{};
  overlaid.Surfaces.push_back(page);
  CHECK(engine.Declare(overlaid), "a scenario that adds only a surface is accepted");
  const size_t afterOverlay = outshine::Core::Live::PlanInits();
  std::printf("A SURFACE ADDED over the same picture initialised the plan %zu further time(s)\n",
              afterOverlay - afterSecond);
  CHECK(afterOverlay == afterSecond,
        "and adding a SURFACE over an unchanged picture rebuilds no plan either -- the overlay "
        "is a different part of the stream from the thing it lies over");

  Note("plans initialised over three declarations of one picture",
       (double)(afterOverlay - before), "inits");

  Covers("the door: the work a declaration causes is proportional to what it changed, so a "
         "scenario can be streamed rather than rebuilt");
  return Report();
}
