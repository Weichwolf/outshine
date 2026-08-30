#include "InputPump.h"

namespace outshine::Core {

namespace {

struct KeyRow {
  SDL_Keycode Key;
  ptrdiff_t Event;
};

struct PadRow {
  uint8_t Button;
  ptrdiff_t Event;
};

struct AxisRow {
  uint8_t Axis;
  ptrdiff_t Event;
};

struct Resolved {
  KeyRow Keys[12];
  PadRow Buttons[2];
  AxisRow Axes[6];
  ptrdiff_t MouseLeft, MouseRight, MouseX, MouseY;
  bool Whole;
};

[[nodiscard]] const Resolved &Table() {
  static const Resolved held = [] {
    Resolved out;
    out.Keys[0] = {SDLK_W, InputMap::EventIndexOf("KeyW")};
    out.Keys[1] = {SDLK_A, InputMap::EventIndexOf("KeyA")};
    out.Keys[2] = {SDLK_S, InputMap::EventIndexOf("KeyS")};
    out.Keys[3] = {SDLK_D, InputMap::EventIndexOf("KeyD")};
    out.Keys[4] = {SDLK_SPACE, InputMap::EventIndexOf("Space")};
    out.Keys[5] = {SDLK_ESCAPE, InputMap::EventIndexOf("Escape")};
    out.Keys[6] = {SDLK_UP, InputMap::EventIndexOf("ArrowUp")};
    out.Keys[7] = {SDLK_DOWN, InputMap::EventIndexOf("ArrowDown")};
    out.Keys[8] = {SDLK_LEFT, InputMap::EventIndexOf("ArrowLeft")};
    out.Keys[9] = {SDLK_RIGHT, InputMap::EventIndexOf("ArrowRight")};
    out.Keys[10] = {SDLK_PAGEUP, InputMap::EventIndexOf("PageUp")};
    out.Keys[11] = {SDLK_PAGEDOWN, InputMap::EventIndexOf("PageDown")};
    out.Buttons[0] = {SDL_GAMEPAD_BUTTON_SOUTH, InputMap::EventIndexOf("GamepadSouth")};
    out.Buttons[1] = {SDL_GAMEPAD_BUTTON_EAST, InputMap::EventIndexOf("GamepadEast")};
    out.Axes[0] = {SDL_GAMEPAD_AXIS_LEFTX, InputMap::EventIndexOf("AxisLeftX")};
    out.Axes[1] = {SDL_GAMEPAD_AXIS_LEFTY, InputMap::EventIndexOf("AxisLeftY")};
    out.Axes[2] = {SDL_GAMEPAD_AXIS_RIGHTX, InputMap::EventIndexOf("AxisRightX")};
    out.Axes[3] = {SDL_GAMEPAD_AXIS_RIGHTY, InputMap::EventIndexOf("AxisRightY")};
    out.Axes[4] = {SDL_GAMEPAD_AXIS_LEFT_TRIGGER, InputMap::EventIndexOf("TriggerLeft")};
    out.Axes[5] = {SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, InputMap::EventIndexOf("TriggerRight")};
    out.MouseLeft = InputMap::EventIndexOf("MouseLeft");
    out.MouseRight = InputMap::EventIndexOf("MouseRight");
    out.MouseX = InputMap::EventIndexOf("MouseX");
    out.MouseY = InputMap::EventIndexOf("MouseY");
    out.Whole = out.MouseLeft >= 0 && out.MouseRight >= 0 && out.MouseX >= 0 && out.MouseY >= 0;
    for (const KeyRow &row : out.Keys) { out.Whole = out.Whole && row.Event >= 0; }
    for (const PadRow &row : out.Buttons) { out.Whole = out.Whole && row.Event >= 0; }
    for (const AxisRow &row : out.Axes) { out.Whole = out.Whole && row.Event >= 0; }
    return out;
  }();
  return held;
}

constexpr float kAxisScale = 1.0f / 32767.0f;

} // namespace

bool InputPump::Open(const InputMap &declared) {
  if (!Table().Whole) { return false; }
  Map_ = &declared;
  return true;
}

size_t InputPump::Translate(const SDL_Event &event, Fired out[2]) const {
  if (Map_ == nullptr) { return 0; }
  const auto fire = [&](ptrdiff_t at, InputMap::Kind what, float value, size_t held) {
    const uint16_t action = Map_->ActionAt((size_t)at);
    if (action == InputMap::kUnbound) { return held; }
    out[held] = {action, what, value};
    return held + 1;
  };
  switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
      if (event.key.repeat) { return 0; }
      for (const KeyRow &row : Table().Keys) {
        if (row.Key == event.key.key) {
          return fire(
              row.Event, InputMap::Kind::Button, event.type == SDL_EVENT_KEY_DOWN ? 1.0f : 0.0f, 0);
        }
      }
      return 0;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
      const float value = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? 1.0f : 0.0f;
      if (event.button.button == SDL_BUTTON_LEFT) {
        return fire(Table().MouseLeft, InputMap::Kind::Button, value, 0);
      }
      if (event.button.button == SDL_BUTTON_RIGHT) {
        return fire(Table().MouseRight, InputMap::Kind::Button, value, 0);
      }
      return 0;
    }
    case SDL_EVENT_MOUSE_MOTION: {
      size_t held = 0;
      if (event.motion.xrel != 0.0f) {
        held = fire(Table().MouseX, InputMap::Kind::Axis, event.motion.xrel, held);
      }
      if (event.motion.yrel != 0.0f) {
        held = fire(Table().MouseY, InputMap::Kind::Axis, event.motion.yrel, held);
      }
      return held;
    }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP: {
      for (const PadRow &row : Table().Buttons) {
        if (row.Button == event.gbutton.button) {
          return fire(row.Event,
                      InputMap::Kind::Button,
                      event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ? 1.0f : 0.0f,
                      0);
        }
      }
      return 0;
    }
    case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
      for (const AxisRow &row : Table().Axes) {
        if (row.Axis == event.gaxis.axis) {
          return fire(row.Event, InputMap::Kind::Axis, (float)event.gaxis.value * kAxisScale, 0);
        }
      }
      return 0;
    }
    default: return 0;
  }
}

} // namespace outshine::Core
