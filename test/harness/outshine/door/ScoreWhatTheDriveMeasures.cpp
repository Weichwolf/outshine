#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>

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
// and it says so. Whether the picture is any good is the stakeholder's judgement and not a number
// this case may invent.
constexpr int kFrames = 24;

[[nodiscard]] bool Ran(const std::string &command, std::string &said) {
  std::FILE *const pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) { return false; }
  std::array<char, 512> buffer{};
  while (std::fgets(buffer.data(), (int)buffer.size(), pipe) != nullptr) { said += buffer.data(); }
  return pclose(pipe) == 0;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::string said;
  const bool ran = Ran("./build/outshine-driver --headless --offline --frames " +
                           std::to_string(kFrames) + " 2>&1",
                       said);
  const bool drove = said.find("DROVE") != std::string::npos;
  const bool fetching = said.find("offline") != std::string::npos && !drove;

  if (!ran && fetching) {
    Unprepared("the drive needs terrain and OSM tiles and this machine has no cache -- pinning "
               "them by URL and hash is board:1964's remaining half");
    return Report();
  }

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
