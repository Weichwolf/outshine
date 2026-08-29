#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <Event.h>
#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

namespace {

// The oracle is what a BUTTON is, and it does not depend on our design: a button has two edges.
// A control command activates a force and the force is active for as long as the command is; a
// door that reports only the press has told the engine to accelerate and never to stop. Both
// benchmarks carry both edges -- Unreal's input actions fire Started and Completed, RAGE's
// control mapping reads a held state each frame -- and neither could hold a throttle without it.
//
// So: a bound key pressed and released reports TWICE, and the second report carries zero.
constexpr int kFramePx = 64;

class Counting : public outshine::Host {
public:
  [[nodiscard]] bool calls(std::string_view name, std::span<const outshine::Argument> args) override {
    Named.emplace_back(name);
    Values.push_back(args.empty() ? -1.0 : args[0].Number);
    return true;
  }
  std::vector<std::string> Named;
  std::vector<double> Values;
};

[[nodiscard]] outshine::Scenario Bound(void) {
  outshine::Scenario made;
  made.Render.Declared = true;
  made.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  outshine::Binding press;
  press.Event = "KeyW";
  press.Action = "throttle";
  made.Input.push_back(press);
  return made;
}

[[nodiscard]] SDL_Event Keyed(bool down) {
  SDL_Event event{};
  event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
  event.key.key = SDLK_W;
  event.key.repeat = 0;
  return event;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so no door can be handed an event");
    return Report();
  }

  outshine::Engine engine;
  if (!engine.drawsInto(outshine::Extent{kFramePx, kFramePx})) {
    Unprepared("the device stood no canvas");
    return Report();
  }
  Counting host;
  engine.offers(&host);
  if (!engine.declare(Bound())) {
    Unprepared(("the bound scenario did not stand: " + engine.error()).c_str());
    return Report();
  }

  const SDL_Event down = Keyed(true);
  const SDL_Event up = Keyed(false);
  const bool tookDown = engine.handleEvent(down).has_value();
  const bool tookUp = engine.handleEvent(up).has_value();

  std::printf("THE PRESS was %s, THE RELEASE was %s\n", tookDown ? "taken" : "DROPPED",
              tookUp ? "taken" : "DROPPED");
  for (size_t at = 0; at < host.Named.size(); ++at) {
    std::printf("  REPORTED %s = %.1f\n", host.Named[at].c_str(), host.Values[at]);
  }

  CHECK(host.Named.size() == 2,
        "**A BOUND KEY REPORTS ITS PRESS AND ITS RELEASE**: a button has two edges, and a door "
        "that carries only the press has told the engine to accelerate and never to stop -- a "
        "throttle pressed once would be a throttle held forever");
  if (host.Named.size() != 2) { return Report(); }

  CHECK(host.Named[0] == "throttle" && host.Named[1] == "throttle",
        "and both edges name the action the scenario bound, not the key that carried it");
  CHECK(host.Values[0] == 1.0,
        "the press reports one -- the command is fully active");
  CHECK(host.Values[1] == 0.0,
        "and the release reports ZERO, which is the value that makes it a release rather than a "
        "second press: the client cannot tell the edges apart by their count");

  Covers("the door: a declared binding reports both edges of its button, so a control command "
         "can be held and can end");
  return Report();
}
