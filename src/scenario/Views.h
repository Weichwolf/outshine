#ifndef OUTSHINE_SCENARIO_VIEWS_H
#define OUTSHINE_SCENARIO_VIEWS_H

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <span>
#include <vector>

#include <Scenario.h>

namespace outshine {

class ViewBook {
public:
  [[nodiscard]] static std::expected<ViewBook, std::string> Stand(std::span<const View> declared,
                                                                  std::string_view starting);

  [[nodiscard]] bool Take(std::string_view id);

  [[nodiscard]] const View &Active() const noexcept { return Held_[Active_]; }

  [[nodiscard]] std::string_view ActiveId() const noexcept { return Held_[Active_].Id; }

  [[nodiscard]] size_t Count() const noexcept { return Held_.size(); }

  [[nodiscard]] const View &AtIndex(size_t at) const noexcept { return Held_[at]; }

  [[nodiscard]] double ClockScale() const noexcept { return Held_[Active_].TimeScale; }

  [[nodiscard]] std::string_view ListensFrom() const noexcept { return Held_[Active_].Follows; }

private:
  ViewBook() = default;

  std::vector<View> Held_;
  size_t Active_ = 0;
};

} // namespace outshine
#endif
