#ifndef OUTSHINE_SCENARIO_TRIGGERS_H
#define OUTSHINE_SCENARIO_TRIGGERS_H

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <span>
#include <vector>

#include <Scenario.h>

namespace outshine {

class TriggerField {
public:
  enum class When : uint8_t { Enter, Exit, Dwell };

  struct Fired {
    uint16_t Event = 0;
    uint32_t Body = 0;
  };

  [[nodiscard]] static std::expected<TriggerField, std::string>
  Stand(std::span<const Volume> volumes, std::span<const Event> events);

  [[nodiscard]] bool
  Listen(std::string_view event, std::span<const std::string_view> reads, std::string &error);

  void Probe(uint32_t body, const double atM[3], double nowS);

  [[nodiscard]] std::span<const Fired> Drain();

  [[nodiscard]] size_t EventCount() const { return Events_.size(); }

  [[nodiscard]] const std::string *EventNamed(uint16_t event) const;
  [[nodiscard]] size_t Unheard(std::string_view event) const;

  [[nodiscard]] size_t Overflowed() const { return Overflowed_; }

  [[nodiscard]] size_t Unseated() const { return Unseated_; }

private:
  TriggerField() = default;

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
