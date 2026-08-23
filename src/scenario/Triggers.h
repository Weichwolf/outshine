#ifndef OUTSHINE_SCENARIO_TRIGGERS_H
#define OUTSHINE_SCENARIO_TRIGGERS_H

#include <cstdint>
#include <string>
#include <string_view>
#include <span>
#include <vector>

#include <outshine/Scenario.h>

namespace outshine {

// the declared trigger volumes, stood up once: a volume fires on enter, exit or dwell --
// the engine spells no fourth -- into a declared event, and firing takes nothing from the
// allocator because a mind walks through doors on the frame path
class TriggerField {
public:
  enum class When : uint8_t { Enter, Exit, Dwell };

  struct Fired {
    uint16_t Event = 0;
    uint32_t Body = 0;
  };

  [[nodiscard]] bool Build(std::span<const Volume> volumes, std::span<const Event> events,
                           std::string &error);

  // a listener declares at stand-up which fields it will read; a field the event does not
  // carry is a refusal HERE, never a null at run time
  [[nodiscard]] bool Listen(std::string_view event, std::span<const std::string_view> reads,
                            std::string &error);

  // one probe per MOVING body per tick against the declared few doors -- a still body
  // probes nothing, state flips fire into the bounded ring, and nothing allocates
  void Probe(uint32_t body, const double atM[3], double nowS);

  // the fired ring since the last drain, oldest first; draining clears it
  [[nodiscard]] std::span<const Fired> Drain();

  [[nodiscard]] size_t EventCount() const { return Events_.size(); }
  [[nodiscard]] const std::string *EventNamed(uint16_t event) const;
  [[nodiscard]] size_t Unheard(std::string_view event) const;
  [[nodiscard]] size_t Overflowed() const { return Overflowed_; }
  // a standing the pool could not seat is a body that would fire Enter EVERY tick and
  // never Exit -- counted, and the count is what a scenario reads to see it
  [[nodiscard]] size_t Unseated() const { return Unseated_; }

private:
  struct Door {
    uint16_t Event = 0;
    When Opens = When::Enter;
    uint8_t Sphere = 0;
    double AtM[3] = {0.0, 0.0, 0.0};
    double ExtentM[3] = {0.0, 0.0, 0.0};
    double DwellS = 0.0;
  };
  struct Standing {
    uint32_t Body = 0;
    uint32_t Door = 0;
    double SinceS = 0.0;
    bool Dwelt = false;
  };
  // the standings are indexed BY DOOR: a probe once scanned every (body, door) pair inside
  // the door loop, so one tick cost doors x standings -- 66 ms at this pool's own declared
  // bounds, four frames for one tick (board:1759). Each door's list is the few bodies
  // standing in THAT door, and the walk is over those alone
  std::vector<std::vector<Standing>> InsideDoor_;

  [[nodiscard]] bool Inside(const Door &door, const double atM[3]) const;

  std::vector<Door> Doors_;
  std::vector<std::string> Events_;
  std::vector<std::vector<std::string>> Carries_;
  std::vector<uint8_t> Heard_;
  std::vector<size_t> Unheard_;
  std::vector<Fired> Ring_;
  std::vector<Fired> Drained_;
  size_t Overflowed_ = 0;
  size_t Unseated_ = 0;
};

}
#endif
