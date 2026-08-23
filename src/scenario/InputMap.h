#ifndef INPUTMAP_H
#define INPUTMAP_H

#include <cstdint>
#include <string>
#include <string_view>
#include <span>
#include <vector>

#include <outshine/Scenario.h>

namespace outshine {

// the declared bindings, stood up once: a device event maps to a NAMED action and the
// client is handed the name -- it never sees a keycode, so remapping is a scenario edit.
// Build interns the event to its catalogue index and the action to a small id, so the
// per-event lookup is one integer index and the name resolves OFF the event path
class InputMap {
public:
  enum class Kind { Button, Axis };
  static constexpr uint16_t kUnbound = 0xFFFF;

  [[nodiscard]] bool Build(std::span<const Binding> declared, std::string &error);

  // every action the client will ask for is checked at stand-up -- a key that does nothing
  // at run time is the defect this refusal replaces
  [[nodiscard]] bool Requires(std::string_view action, std::string &error) const;

  // the catalogue side, static: the pump resolves its device event to an index ONCE
  [[nodiscard]] static size_t Events();
  [[nodiscard]] static ptrdiff_t EventIndexOf(std::string_view event);

  // the event path: index in, action id out, no string touched
  [[nodiscard]] uint16_t ActionAt(size_t eventIndex) const;
  [[nodiscard]] const std::string *ActionNamed(uint16_t action) const;

  [[nodiscard]] const std::string *ActionOf(std::string_view event) const;
  [[nodiscard]] bool KindOf(std::string_view event, Kind &out) const;
  [[nodiscard]] size_t BoundTo(std::string_view action) const;

private:
  std::vector<uint16_t> ActionAt_; // catalogue-index -> action id, kUnbound where nothing binds
  std::vector<std::string> Actions_;
};

}
#endif
