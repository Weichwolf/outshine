#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "Check.h"

namespace {

// THE ONE INTEGRATION TEST WAS COMPILED AND NEVER RUN. `test/run.sh` builds `apps/driver`, reserves
// it under `NAMED_ONLY="apps"`, and naming it answers *no declared suite under apps/driver/src* --
// because a program is not a suite. So the client CLAUDE.md calls the proof of the door was only
// ever type-checked, and board:1963 could stand for as long as it did, with the drive unable to
// open its own subject, without any gate noticing.
//
// A build that compiles the demo and never starts it is testing the compiler. Unreal runs its
// templates in automation and RAGE ran its map on every build; neither settled for a clean compile.
//
// WHAT THIS CASE IS HONEST ABOUT. A drive needs terrain and OSM tiles. They are fetched and cached,
// and a machine that has never driven has no cache -- so this runs the drive OFFLINE and reports
// UNPREPARED rather than red when the cache is absent. That is the corpora's own bargain stated
// small: a case that cannot run says so and does not pretend. Pinning the tiles by URL and hash,
// which is what would make this deterministic anywhere, is board:1964's remaining half.
//
// The bar is deliberately low and it is the bar that was missing: the drive STARTS, it advances,
// and it says so. Whether the picture is any good is the owner's judgement and not a number
// this case may invent.
constexpr int kFrames = 24;

// A CHILD IS SPAWNED WITH A BOUND OR IT IS LEAKED (board:2006). `popen` hands back no pid, so
// when the runner cuts this case off at its own bound the driver survives as an orphan -- one was
// measured at 01:19:55, holding a core, and the three door runs after it each timed out on a
// DIFFERENT innocent case. SDL_CreateProcess gives a handle that can be waited on without
// blocking and killed, which is what Unreal's CreateProc/WaitForProc pairing is for.
constexpr int kBoundSeconds = 90;

[[nodiscard]] bool Ran(const std::vector<const char *> &argv, const std::string &into,
                       std::string &said, bool &bounded) {
  bounded = false;
  SDL_IOStream *const sink = SDL_IOFromFile(into.c_str(), "w+b");
  if (sink == nullptr) { return false; }
  SDL_PropertiesID how = SDL_CreateProperties();
  if (how == 0) {
    (void)SDL_CloseIO(sink);
    return false;
  }
  SDL_SetPointerProperty(how, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)argv.data());
  SDL_SetNumberProperty(how, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_REDIRECT);
  SDL_SetPointerProperty(how, SDL_PROP_PROCESS_CREATE_STDOUT_POINTER, sink);
  SDL_SetBooleanProperty(how, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
  SDL_Process *const child = SDL_CreateProcessWithProperties(how);
  SDL_DestroyProperties(how);
  if (child == nullptr) {
    (void)SDL_CloseIO(sink);
    return false;
  }

  // NOTHING IS READ INSIDE THE LOOP, and that is the whole point. A read on the child's pipe
  // blocks when the child says nothing, which is exactly the case the bound exists for -- so the
  // child writes to a file and this loop only ever asks whether it has exited.
  const Uint64 until = SDL_GetTicks() + (Uint64)kBoundSeconds * 1000u;
  int status = 0;
  bool exited = false;
  while (!(exited = SDL_WaitProcess(child, false, &status))) {
    if (SDL_GetTicks() > until) {
      bounded = true;
      SDL_KillProcess(child, true);
      (void)SDL_WaitProcess(child, true, &status);
      break;
    }
    SDL_Delay(10);
  }
  SDL_DestroyProcess(child);
  (void)SDL_CloseIO(sink);

  size_t bytes = 0;
  void *const held = SDL_LoadFile(into.c_str(), &bytes);
  if (held != nullptr) {
    said.assign((const char *)held, bytes);
    SDL_free(held);
  }
  return exited && status == 0;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::string said;
  const std::string frames = std::to_string(kFrames);
  const std::vector<const char *> argv = {"./build/outshine-driver", "--headless", "--offline",
                                          "--frames", frames.c_str(), nullptr};
  const char *const nest = std::getenv("OUTSHINE_NEST");
  bool bounded = false;
  const bool ran = Ran(argv, std::string(nest != nullptr ? nest : ".") + "/drive-said.txt", said,
                       bounded);
  const bool drove = said.find("DROVE") != std::string::npos;
  const bool fetching = said.find("offline") != std::string::npos && !drove;

  if (!ran && fetching) {
    Unprepared("the drive needs terrain and OSM tiles and this machine has no cache -- pinning "
               "them by URL and hash is board:1964's remaining half");
    return Report();
  }

  CHECK(!bounded,
        "**THE DRIVE FINISHES INSIDE ITS OWN BOUND, AND THE BOUND IS THIS CASE'S TO ENFORCE**: a "
        "child spawned without one is a child nobody can kill, and the runner cutting this case "
        "off leaves it running -- one was measured holding a core for 01:19:55, after which three "
        "door runs each timed out on a DIFFERENT innocent case. A hang reported here names the "
        "drive; a hang reported by the runner names whoever was unlucky");

  const size_t at = said.rfind("DROVE");
  std::printf("THE DRIVE SAID  %s",
              at == std::string::npos ? "nothing about driving\n" : said.c_str() + at);

  CHECK(ran,
        "the one integration test EXITS CLEANLY. It is compiled by every gate and was run by "
        "none, so a client that could not open its own subject stood as long as it did with "
        "nothing to notice");
  CHECK(drove,
        "**AND IT DRIVES**: the client reads a declaration from the tree, composes a world, "
        "stands a vehicle, advances the simulation and renders -- which is more of the door than "
        "any suite here reaches, and the reason CLAUDE.md calls it the integration test");

  Covers("the client: the gate RUNS the drive it builds, offline, and refuses to pass when the "
         "one integration test cannot start");
  return Report();
}
