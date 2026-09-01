#ifndef OUTSHINE_SCENARIO_INPUTMAP_H
#define OUTSHINE_SCENARIO_INPUTMAP_H

#include <cstdint>
#include <string>
#include <string_view>
#include <span>
#include <vector>

#include <scenario/Scenario.h>

namespace outshine {

class InputMap {
public:
  enum class Kind { Button, Axis };
  static constexpr uint16_t kUnbound = 0xFFFF;

  [[nodiscard]] bool Build(std::span<const Scenario::Binding> declared, std::string &error);

  [[nodiscard]] bool Requires(std::string_view action, std::string &error) const;

  [[nodiscard]] static size_t Events();
  [[nodiscard]] static ptrdiff_t EventIndexOf(std::string_view event);

  [[nodiscard]] uint16_t ActionAt(size_t eventIndex) const;
  [[nodiscard]] const std::string *ActionNamed(uint16_t action) const;

  [[nodiscard]] const std::string *ActionOf(std::string_view event) const;
  [[nodiscard]] static bool KindOf(std::string_view event, Kind &out);
  [[nodiscard]] size_t BoundTo(std::string_view action) const;

private:
  std::vector<uint16_t> ActionAt_;
  std::vector<std::string> Actions_;
};

} // namespace outshine
#endif
