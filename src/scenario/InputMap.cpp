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

[[nodiscard]] const KnownEvent *Known(std::string_view name) {
  for (const KnownEvent &event : kEvents) {
    if (name == event.Name) { return &event; }
  }
  return nullptr;
}

[[nodiscard]] std::string Catalogue(void) {
  std::string all;
  for (const KnownEvent &event : kEvents) {
    if (!all.empty()) { all += ' '; }
    all += event.Name;
  }
  return all;
}

} // namespace

bool InputMap::Build(const std::vector<Binding> &declared, std::string &error) {
  Rows_.clear();
  for (const Binding &binding : declared) {
    const KnownEvent *known = Known(binding.Event);
    if (known == nullptr) {
      error = "the binding names the event '" + binding.Event +
              "', and the catalogue offers: " + Catalogue();
      return false;
    }
    for (const Row &row : Rows_) {
      if (row.Event == binding.Event) {
        error = "the event '" + binding.Event + "' is bound twice -- to '" + row.Action +
                "' and to '" + binding.Action + "' -- and one press has one meaning";
        return false;
      }
    }
    Rows_.push_back({binding.Event, binding.Action, known->What});
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

const std::string *InputMap::ActionOf(std::string_view event) const {
  for (const Row &row : Rows_) {
    if (row.Event == event) { return &row.Action; }
  }
  return nullptr;
}

bool InputMap::KindOf(std::string_view event, Kind &out) const {
  const KnownEvent *known = Known(event);
  if (known == nullptr) { return false; }
  out = known->What;
  return true;
}

size_t InputMap::BoundTo(std::string_view action) const {
  size_t bound = 0;
  for (const Row &row : Rows_) {
    if (row.Action == action) { ++bound; }
  }
  return bound;
}

} // namespace outshine
