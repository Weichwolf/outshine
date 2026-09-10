#include <Outshine.h>
#include "Check.h"
#include <vector>
#include <string>

namespace {
class Receiver final : public outshine::Host {
public:
  std::vector<std::string> Names;
  std::vector<double> Values;
  bool Accept = true;

  bool calls(std::string_view name, std::span<const outshine::Argument> args) override {
    Names.emplace_back(name);
    if (args.size() != 1 || args[0].Is != outshine::Argument::Kind::Number) { return false; }
    Values.push_back(args[0].Number);
    return Accept;
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Receiver receiver;
  Engine engine;
  engine.offers(&receiver);
  Scenario::Document scene;
  scene.Input = {{.Event = "KeyW", .Action = "key"},
                 {.Event = "MouseLeft", .Action = "mouse"},
                 {.Event = "MouseX", .Action = "x"},
                 {.Event = "MouseY", .Action = "y"},
                 {.Event = "GamepadSouth", .Action = "pad"},
                 {.Event = "AxisLeftX", .Action = "stick"},
                 {.Event = "TriggerRight", .Action = "trigger"}};
  CHECK(engine.declare(scene).has_value(), "input declaration needs no SDL or render target");
  SDL_Event event{};
  for (const auto type : {SDL_EVENT_KEY_DOWN,
                          SDL_EVENT_KEY_UP,
                          SDL_EVENT_MOUSE_BUTTON_DOWN,
                          SDL_EVENT_MOUSE_BUTTON_UP,
                          SDL_EVENT_GAMEPAD_BUTTON_DOWN,
                          SDL_EVENT_GAMEPAD_BUTTON_UP}) {
    event = {};
    event.type = type;
    if (type == SDL_EVENT_KEY_DOWN || type == SDL_EVENT_KEY_UP) {
      event.key.key = SDLK_W;
    } else if (type == SDL_EVENT_MOUSE_BUTTON_DOWN || type == SDL_EVENT_MOUSE_BUTTON_UP) {
      event.button.button = SDL_BUTTON_LEFT;
    } else {
      event.gbutton.button = SDL_GAMEPAD_BUTTON_SOUTH;
    }
    const auto result = engine.handleEvent(event);
    CHECK(result && *result, "bound button reaches host");
  }
  event = {};
  event.type = SDL_EVENT_MOUSE_MOTION;
  event.motion.xrel = 2.0f;
  event.motion.yrel = -3.0f;
  CHECK(engine.handleEvent(event).value_or(false), "both mouse axes reach host");
  event = {};
  event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
  event.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTX;
  event.gaxis.value = SDL_JOYSTICK_AXIS_MIN;
  CHECK(engine.handleEvent(event).value_or(false), "stick reaches host");
  event.gaxis.axis = SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
  event.gaxis.value = SDL_JOYSTICK_AXIS_MAX;
  CHECK(engine.handleEvent(event).value_or(false), "trigger reaches host");
  CHECK(receiver.Names ==
            std::vector<std::string>(
                {"key", "key", "mouse", "mouse", "pad", "pad", "x", "y", "stick", "trigger"}),
        "action identities and dispatch order preserved");
  CHECK(receiver.Values == std::vector<double>({1, 0, 1, 0, 1, 0, 2, -3, -1, 1}),
        "button states, pixel deltas and normalized axes preserved");
  receiver.Accept = false;
  const auto refused = engine.handleEvent(event);
  CHECK(refused && !*refused, "host refusal remains an unhandled result");
  engine.offers(static_cast<Host *>(nullptr));
  CHECK(!engine.handleEvent(event), "bound action without a host produces an error");
  engine.offers(&receiver);
  CHECK(engine.declare({}).has_value(), "empty declaration removes bindings");
  const auto unbound = engine.handleEvent(event);
  CHECK(unbound && !*unbound, "removed binding is unhandled without reusing old error");
  return Report();
}
