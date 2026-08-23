#include "InputMap.h"

namespace outshine {

namespace {

struct KnownEvent {
  const char *Name;
  InputMap::Kind What;
};

// the event catalogue is the engine's, constexpr: a scenario selects from it and cannot
// add to it, and AN AXIS IS NOT A BUTTON -- a trigger's 0..1 is never a press
constexpr KnownEvent kEvents[] = {
    {"KeyW", InputMap::Kind::Button},        {"KeyA", InputMap::Kind::Button},
    {"KeyS", InputMap::Kind::Button},        {"KeyD", InputMap::Kind::Button},
    {"Space", InputMap::Kind::Button},       {"Escape", InputMap::Kind::Button},
    {"MouseLeft", InputMap::Kind::Button},   {"MouseRight", InputMap::Kind::Button},
    {"GamepadSouth", InputMap::Kind::Button},{"GamepadEast", InputMap::Kind::Button},
    {"MouseX", InputMap::Kind::Axis},        {"MouseY", InputMap::Kind::Axis},
    {"AxisLeftX", InputMap::Kind::Axis},     {"AxisLeftY", InputMap::Kind::Axis},
    {"AxisRightX", InputMap::Kind::Axis},    {"AxisRightY", InputMap::Kind::Axis},
    {"TriggerLeft", InputMap::Kind::Axis},   {"TriggerRight", InputMap::Kind::Axis},
};
constexpr size_t kEventCount = sizeof kEvents / sizeof kEvents[0];
static_assert(kEventCount < InputMap::kUnbound, "the unbound sentinel must stay outside");

[[nodiscard]] std::string Catalogue(void) {
  std::string all;
  for (const KnownEvent &event : kEvents) {
    if (!all.empty()) { all += ' '; }
    all += event.Name;
  }
  return all;
}

} // namespace

size_t InputMap::Events(void) { return kEventCount; }

ptrdiff_t InputMap::EventIndexOf(std::string_view event) {
  for (size_t at = 0; at < kEventCount; ++at) {
    if (event == kEvents[at].Name) { return (ptrdiff_t)at; }
  }
  return -1;
}

bool InputMap::Build(std::span<const Binding> declared, std::string &error) {
  ActionAt_.assign(kEventCount, kUnbound);
  Actions_.clear();
  for (const Binding &binding : declared) {
    const ptrdiff_t at = EventIndexOf(binding.Event);
    if (at < 0) {
      error = "the binding names the event '" + binding.Event +
              "', and the catalogue offers: " + Catalogue();
      return false;
    }
    if (ActionAt_[(size_t)at] != kUnbound) {
      error = "the event '" + binding.Event + "' is bound twice -- to '" +
              Actions_[ActionAt_[(size_t)at]] + "' and to '" + binding.Action +
              "' -- and one press has one meaning";
      return false;
    }
    uint16_t action = kUnbound;
    for (size_t held = 0; held < Actions_.size(); ++held) {
      if (Actions_[held] == binding.Action) { action = (uint16_t)held; }
    }
    if (action == kUnbound) {
      action = (uint16_t)Actions_.size();
      Actions_.push_back(binding.Action);
    }
    ActionAt_[(size_t)at] = action;
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
  return at < 0 ? nullptr : ActionNamed(ActionAt((size_t)at));
}

bool InputMap::KindOf(std::string_view event, Kind &out) const {
  const ptrdiff_t at = EventIndexOf(event);
  if (at < 0) { return false; }
  out = kEvents[at].What;
  return true;
}

size_t InputMap::BoundTo(std::string_view action) const {
  uint16_t wanted = kUnbound;
  for (size_t held = 0; held < Actions_.size(); ++held) {
    if (Actions_[held] == action) { wanted = (uint16_t)held; }
  }
  if (wanted == kUnbound) { return 0; }
  size_t bound = 0;
  for (const uint16_t at : ActionAt_) {
    if (at == wanted) { ++bound; }
  }
  return bound;
}

} // namespace outshine
