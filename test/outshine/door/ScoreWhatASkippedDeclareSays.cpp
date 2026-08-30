#include <cstdio>
#include <cstdlib>
#include <string>

#include <SDL3/SDL.h>

#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

// A STAGE SKIPPED IS REFUSED WHERE IT IS SKIPPED, NEVER ABSORBED.
//
// Unreal refuses a world that was loaded and never initialised by name; RAGE asserts on a map that
// is streamed and not activated. Both agree, and the rule is what makes a door teachable: the verb
// that was missed is the verb the refusal names.
//
// `Read` fills the declaration. `Declare` hands it over and stands the picture. `Assemble` builds
// what `Declare` stood. A client that reads and assembles has skipped the middle one, and this
// door used to ACCEPT that: `apps/bench` did exactly it, ran ONE step of a drive, and failed with
//
//     nothing joined this picture from a file, so there is no body to carry --
//     every part stands where the world put it
//
// which is a true sentence about a state four verbs downstream. A reader debugging it inspects the
// asset, the scenario's body and the joining, all of which are correct.
//
// A CLIENT'S LINE COUNT MEASURES THE DOOR (CLAUDE.md). Here the client's line count was one line
// SHORT and the door said nothing until the shortfall had propagated out of sight of its cause.

namespace {

constexpr const char *kScenario = "src/assets/drive/f31.scenario";

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so no canvas can stand");
    return Report();
  }

  outshine::Engine engine;
  engine.setRoots(
      outshine::Roots{"src/assets/drive", "src/assets", "/tmp/outshine-door-cache", true});
  if (!engine.drawsInto(outshine::Extent{64, 36})) {
    Unprepared("the device stood no canvas");
    return Report();
  }
  if (!engine.readScenario(kScenario)) {
    Unprepared(("the declaration would not read: " + engine.error()).c_str());
    return Report();
  }

  const bool refusedTheSkip = !engine.assemble();
  const std::string said = engine.error();
  std::printf("READ THEN ASSEMBLED  %s\n", refusedTheSkip ? said.c_str() : "and it was accepted");

  outshine::Scenario declared = engine.declaration();
  declared.Render.Frame = outshine::Extent{64, 36};
  const bool stoodAfterDeclare = engine.declare(declared).has_value();
  std::printf("THEN DECLARED        %s\n",
              stoodAfterDeclare ? "and the declaration stands" : engine.error().c_str());

  CHECK(refusedTheSkip,
        "**ASSEMBLE REFUSES A DECLARATION THAT WAS READ AND NEVER DECLARED**: accepting a "
        "declaration and doing nothing with it is worse than refusing it, and this one was "
        "accepted far enough to run a step before it failed on something else entirely");

  CHECK(refusedTheSkip && said.find("DECLARED") != std::string::npos,
        "**AND IT NAMES THE VERB THAT WAS MISSED**: a refusal that says what is broken four verbs "
        "downstream sends the reader to the asset, the body and the joining -- all of which are "
        "correct. Unreal names the initialisation a world skipped; RAGE asserts on the activation. "
        "The cost of not naming it is measured: it is the hour somebody spends on the wrong file");

  CHECK(stoodAfterDeclare,
        "and the refusal is about the ORDER rather than the declaration: the same scenario stands "
        "the moment Declare is called, so this case cannot pass by holding a scenario that was "
        "broken all along");

  Covers("the door: Assemble refuses a declaration that was read and never declared, and the "
         "refusal names Declare rather than a symptom four verbs downstream");
  return Report();
}
