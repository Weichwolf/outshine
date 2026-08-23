#ifndef INPUTMAP_H
#define INPUTMAP_H

#include <string>
#include <string_view>
#include <span>
#include <vector>

#include <outshine/Scenario.h>

namespace outshine {

// the declared bindings, stood up once: a device event maps to a NAMED action and the
// client is handed the name -- it never sees a keycode, so remapping is a scenario edit
class InputMap {
public:
  enum class Kind { Button, Axis };

  [[nodiscard]] bool Build(std::span<const Binding> declared, std::string &error);

  // every action the client will ask for is checked at stand-up -- a key that does nothing
  // at run time is the defect this refusal replaces
  [[nodiscard]] bool Requires(std::string_view action, std::string &error) const;

  [[nodiscard]] const std::string *ActionOf(std::string_view event) const;
  [[nodiscard]] bool KindOf(std::string_view event, Kind &out) const;
  [[nodiscard]] size_t BoundTo(std::string_view action) const;

private:
  struct Row {
    std::string Event;
    std::string Action;
    Kind What = Kind::Button;
  };
  std::vector<Row> Rows_;
};

}
#endif
