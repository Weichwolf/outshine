#include <cstdio>
#include <cstring>
#include <string>

#include "Check.h"

#include "InputMap.h"
#include "ScenarioRead.h"

using outshine::InputMap;
using outshine::ReadScenario;
using outshine::Scenario;

namespace {

[[nodiscard]] bool Stood(const char *text, InputMap &map, std::string &error) {
  Scenario declared;
  if (!ReadScenario(text, std::strlen(text), declared, error)) { return false; }
  return map.Build(declared.Input, error);
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::string error;
  InputMap map;
  const bool up = Stood("<scenario name=\"drives\"><input>"
                        "<bind event=\"KeyW\" action=\"forward\"/>"
                        "<bind event=\"AxisLeftY\" action=\"forward\"/>"
                        "<bind event=\"Escape\" action=\"pause\"/>"
                        "</input></scenario>",
                        map, error);
  if (!up) { std::printf("REFUSED %s\n", error.c_str()); }
  CHECK(up, "declared bindings stand up once");
  if (!up) { return Report(); }

  const std::string *forward = map.ActionOf("KeyW");
  CHECK(forward != nullptr && *forward == "forward",
        "**A DEVICE EVENT MAPS TO A NAMED ACTION** and the client is handed the name -- "
        "never a keycode, so remapping is a scenario edit");
  CHECK(map.BoundTo("forward") == 2,
        "**A BINDING IS 1..N PER ACTION**: forward answers to the key and to the stick");
  CHECK(map.Requires("forward", error) && map.Requires("pause", error),
        "the actions the client asks for are checked at stand-up");
  CHECK(!map.Requires("jump", error) && error.find("jump") != std::string::npos,
        "**AN ACTION NOTHING BINDS REFUSES AT STAND-UP**, naming it -- never a key that "
        "does nothing at run time");

  InputMap::Kind kind = InputMap::Kind::Button;
  CHECK(map.KindOf("TriggerLeft", kind) && kind == InputMap::Kind::Axis,
        "**AN AXIS IS NOT A BUTTON**: the trigger's 0..1 is declared an axis, never a press");
  CHECK(map.KindOf("KeyW", kind) && kind == InputMap::Kind::Button,
        "and a key is a button, from the same one catalogue");

  {
    InputMap bad;
    CHECK(!Stood("<scenario name=\"t\"><input>"
                 "<bind event=\"KeyQ\" action=\"quit\"/></input></scenario>",
                 bad, error) &&
              error.find("KeyQ") != std::string::npos &&
              error.find("KeyW") != std::string::npos,
          "an event the catalogue does not offer refuses naming it AND the catalogue");
  }
  {
    InputMap bad;
    CHECK(!Stood("<scenario name=\"t\"><input>"
                 "<bind event=\"KeyW\" action=\"forward\"/>"
                 "<bind event=\"KeyW\" action=\"backward\"/></input></scenario>",
                 bad, error) &&
              error.find("one press has one meaning") != std::string::npos,
          "one event bound to two actions refuses -- one press has one meaning");
  }

  Covers("III.10 a device event reaches the client as a declared action: named actions from "
         "one constexpr event catalogue, 1..N bindings per action, axis distinct from "
         "button, unbound asks refuse at stand-up; the input-to-photon measurement is the "
         "frame suite's later slice (board:1491)");
  return Report();
}
