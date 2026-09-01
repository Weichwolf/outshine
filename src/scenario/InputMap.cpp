#include "InputMap.h"
#include <cstddef>
#include <string>
#include <string_view>
#include <span>
#include <cstdint>

namespace outshine {

namespace {

struct KnownEvent {
  const char *Name;
  InputMap::Kind What;
};

constexpr KnownEvent kEvents[] = {
    {.Name = "KeyW", .What = InputMap::Kind::Button},
    {.Name = "KeyA", .What = InputMap::Kind::Button},
    {.Name = "KeyS", .What = InputMap::Kind::Button},
    {.Name = "KeyD", .What = InputMap::Kind::Button},
    {.Name = "Space", .What = InputMap::Kind::Button},
    {.Name = "Escape", .What = InputMap::Kind::Button},
    {.Name = "ArrowUp", .What = InputMap::Kind::Button},
    {.Name = "ArrowDown", .What = InputMap::Kind::Button},
    {.Name = "ArrowLeft", .What = InputMap::Kind::Button},
    {.Name = "ArrowRight", .What = InputMap::Kind::Button},
    {.Name = "PageUp", .What = InputMap::Kind::Button},
    {.Name = "PageDown", .What = InputMap::Kind::Button},
    {.Name = "MouseLeft", .What = InputMap::Kind::Button},
    {.Name = "MouseRight", .What = InputMap::Kind::Button},
    {.Name = "GamepadSouth", .What = InputMap::Kind::Button},
    {.Name = "GamepadEast", .What = InputMap::Kind::Button},
    {.Name = "MouseX", .What = InputMap::Kind::Axis},
    {.Name = "MouseY", .What = InputMap::Kind::Axis},
    {.Name = "AxisLeftX", .What = InputMap::Kind::Axis},
    {.Name = "AxisLeftY", .What = InputMap::Kind::Axis},
    {.Name = "AxisRightX", .What = InputMap::Kind::Axis},
    {.Name = "AxisRightY", .What = InputMap::Kind::Axis},
    {.Name = "TriggerLeft", .What = InputMap::Kind::Axis},
    {.Name = "TriggerRight", .What = InputMap::Kind::Axis},
};
constexpr size_t kEventCount = sizeof kEvents / sizeof kEvents[0];
static_assert(kEventCount < InputMap::kUnbound, "the unbound sentinel must stay outside");

[[nodiscard]] std::string Catalogue() {
  std::string all;
  for (const KnownEvent &event : kEvents) {
    if (!all.empty()) { all += ' '; }
    all += event.Name;
  }
  return all;
}

} // namespace

size_t InputMap::Events() {
  return kEventCount;
}

ptrdiff_t InputMap::EventIndexOf(std::string_view event) {
  for (size_t at = 0; at < kEventCount; ++at) {
    if (event == kEvents[at].Name) { return static_cast<ptrdiff_t>(at); }
  }
  return -1;
}

bool InputMap::Build(std::span<const Scenario::Binding> declared, std::string &error) {
  ActionAt_.assign(kEventCount, kUnbound);
  Actions_.clear();
  for (const Scenario::Binding &binding : declared) {
    const ptrdiff_t at = EventIndexOf(binding.Event);
    if (at < 0) {
      error = "the binding names the event '" + binding.Event +
              "', and the catalogue offers: " + Catalogue();
      return false;
    }
    if (ActionAt_[static_cast<size_t>(at)] != kUnbound) {
      error = "the event '" + binding.Event + "' is bound twice -- to '" +
              Actions_[ActionAt_[static_cast<size_t>(at)]] + "' and to '" + binding.Action +
              "' -- and one press has one meaning";
      return false;
    }
    uint16_t action = kUnbound;
    for (size_t held = 0; held < Actions_.size(); ++held) {
      if (Actions_[held] == binding.Action) { action = static_cast<uint16_t>(held); }
    }
    if (action == kUnbound) {
      action = static_cast<uint16_t>(Actions_.size());
      Actions_.push_back(binding.Action);
    }
    ActionAt_[static_cast<size_t>(at)] = action;
  }
  return true;
}

bool InputMap::Requires(std::string_view action, std::string &error) const {
  if (BoundTo(action) > 0) { return true; }
  error = "the client asks for the action '" + std::string(action) +
          "', which no binding declares -- a key that does nothing at run time is the "
          "defect this refusal replaces";
  return false;
}

uint16_t InputMap::ActionAt(size_t eventIndex) const {
  return eventIndex < ActionAt_.size() ? ActionAt_[eventIndex] : kUnbound;
}

const std::string *InputMap::ActionNamed(uint16_t action) const {
  return action < Actions_.size() ? &Actions_[action] : nullptr;
}

const std::string *InputMap::ActionOf(std::string_view event) const {
  const ptrdiff_t at = EventIndexOf(event);
  return at < 0 ? nullptr : ActionNamed(ActionAt(static_cast<size_t>(at)));
}

bool InputMap::KindOf(std::string_view event, Kind &out) {
  const ptrdiff_t at = EventIndexOf(event);
  if (at < 0) { return false; }
  out = kEvents[at].What;
  return true;
}

size_t InputMap::BoundTo(std::string_view action) const {
  uint16_t wanted = kUnbound;
  for (size_t held = 0; held < Actions_.size(); ++held) {
    if (Actions_[held] == action) { wanted = static_cast<uint16_t>(held); }
  }
  if (wanted == kUnbound) { return 0; }
  size_t bound = 0;
  for (const uint16_t at : ActionAt_) {
    if (at == wanted) { ++bound; }
  }
  return bound;
}

} // namespace outshine
