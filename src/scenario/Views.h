#ifndef OUTSHINE_SCENARIO_VIEWS_H
#define OUTSHINE_SCENARIO_VIEWS_H

#include <cstdint>
#include <string>
#include <string_view>
#include <span>
#include <vector>

#include <outshine/Scenario.h>

namespace outshine {

// the declared views, stood up once: a first-person view, an aimed view that slows time,
// a chase view and a map view are ONE mechanism -- follow something, sit at an offset,
// carry a field and a time scale. Exactly one is active, switching is one integer, and
// the active view's timeScale scales the CLOCK, never the frame
class ViewBook {
public:
  [[nodiscard]] bool Build(std::span<const View> declared, std::string_view starting,
                           std::string &error);

  // switching costs no stand-up: the id resolves once, the active seat is one index
  [[nodiscard]] bool Take(std::string_view id);

  [[nodiscard]] const View &Active() const { return Held_[Active_]; }
  [[nodiscard]] std::string_view ActiveId() const { return Held_[Active_].Id; }
  [[nodiscard]] size_t Count() const { return Held_.size(); }

  // the clock's own rate: a slowed view still lands its frames, the world advances less
  [[nodiscard]] double ClockScale() const { return Held_[Active_].TimeScale; }

  // the active view is the audio listener -- one seat, no second declaration
  [[nodiscard]] std::string_view ListensFrom() const { return Held_[Active_].Follows; }

private:
  std::vector<View> Held_;
  size_t Active_ = 0;
};

}
#endif
