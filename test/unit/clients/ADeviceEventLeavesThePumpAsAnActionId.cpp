#include <cstdio>
#include <cstring>
#include <string>

#include "Check.h"

#include "InputMap.h"
#include "InputPump.h"
#include "ScenarioRead.h"

using outshine::InputMap;
using outshine::ReadScenario;
using outshine::Scenario;
using outshine::Clients::InputPump;

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const char *text = "<scenario name=\"drives\"><input>"
                     "<bind event=\"KeyW\" action=\"forward\"/>"
                     "<bind event=\"AxisLeftY\" action=\"forward\"/>"
                     "<bind event=\"MouseX\" action=\"look\"/>"
                     "</input></scenario>";
  Scenario declared;
  std::string error;
  InputMap map;
  CHECK(ReadScenario(text, std::strlen(text), declared, error) &&
            map.Build(declared.Input, error),
        "the declared bindings stand up");

  InputPump pump;
  CHECK(pump.Open(map), "the pump opens -- every SDL row resolved its catalogue index once");

  const uint16_t forward = map.ActionAt((size_t)InputMap::EventIndexOf("KeyW"));

  InputPump::Fired fired[2];
  {
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = SDLK_W;
    CHECK(pump.Translate(event, fired) == 1 && fired[0].Action == forward &&
              fired[0].What == InputMap::Kind::Button && fired[0].Value == 1.0f,
          "**A KEY LEAVES THE PUMP AS THE DECLARED ACTION'S ID** -- the client never sees "
          "the keycode (board:1491)");
    event.type = SDL_EVENT_KEY_UP;
    CHECK(pump.Translate(event, fired) == 1 && fired[0].Value == 0.0f,
          "and the release is the same action at zero");
  }
  {
    SDL_Event event{};
    event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    event.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTY;
    event.gaxis.value = 16384;
    CHECK(pump.Translate(event, fired) == 1 && fired[0].Action == forward &&
              fired[0].What == InputMap::Kind::Axis && fired[0].Value > 0.49f &&
              fired[0].Value < 0.51f,
          "**THE STICK LANDS ON THE SAME ACTION ID AS THE KEY**, as an axis in -1..1 -- one "
          "declared meaning, two devices");
  }
  {
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = SDLK_ESCAPE;
    CHECK(pump.Translate(event, fired) == 0,
          "**AN EVENT THE SCENARIO LEFT UNBOUND LEAVES AS NOTHING** -- the pump fires no "
          "action a declaration did not name");
    event.key.key = SDLK_W;
    event.key.repeat = true;
    CHECK(pump.Translate(event, fired) == 0,
          "and a key repeat is the OS's convenience, not a second press");
  }
  {
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.xrel = 3.0f;
    event.motion.yrel = -2.0f;
    const size_t count = pump.Translate(event, fired);
    CHECK(count == 1 && fired[0].Value == 3.0f,
          "one motion event carries two axes and only the BOUND one fires -- MouseX is "
          "declared, MouseY is not");
  }

  Covers("III.10 a device event leaves the pump as an action id: SDL rows resolve their "
         "catalogue indices once at open, a pumped event is (id, kind, value), unbound "
         "events and key repeats leave as nothing (board:1491)");
  return Report();
}
