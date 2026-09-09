#include "InputPump.h"
#include "Check.h"
#include <array>
#include <limits>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array bindings{Scenario::Binding{.Event = "AxisLeftX", .Action = "steer"},
                            Scenario::Binding{.Event = "TriggerRight", .Action = "accelerate"}};
  InputMap map;
  std::string error;
  CHECK(map.Build(bindings, error), "input bindings are valid");
  Core::InputPump pump;
  CHECK(pump.Open(map), "input pump opens");
  std::array<Core::InputPump::Fired, 2> output{};
  SDL_Event event{};
  event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
  for (const auto axis : {SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER}) {
    event.gaxis.axis = static_cast<Uint8>(axis);
    const int lowest = axis == SDL_GAMEPAD_AXIS_LEFTX ? std::numeric_limits<Sint16>::min() : 0;
    float previous = -2.0f;
    bool valid = true;
    for (int value = lowest; value <= std::numeric_limits<Sint16>::max(); ++value) {
      event.gaxis.value = static_cast<Sint16>(value);
      const auto count = pump.Translate(event, output);
      const float normalized = output[0].Value;
      valid = valid && count == 1 && output[0].What == InputMap::Kind::Axis &&
              normalized >= (lowest < 0 ? -1.0f : 0.0f) && normalized <= 1.0f &&
              normalized > previous;
      if (value == 0) { valid = valid && normalized == 0.0f; }
      if (value == lowest) { valid = valid && normalized == (lowest < 0 ? -1.0f : 0.0f); }
      if (value == std::numeric_limits<Sint16>::max()) { valid = valid && normalized == 1.0f; }
      previous = normalized;
    }
    CHECK(valid, "every axis value preserves range, endpoints, zero and strict monotonicity");
  }
  return Report();
}
