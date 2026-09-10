#ifndef OUTSHINE_SCENARIO_TRIGGERS_H
#define OUTSHINE_SCENARIO_TRIGGERS_H

#include "math/Vec3.h"
#include <world/Entity.h>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <span>
#include <vector>

#include <scenario/Scenario.h>

namespace outshine {

class TriggerField {
public:
  enum class When : uint8_t { Enter, Exit, Dwell };

  struct Fired {
    uint16_t Event = 0;
    Entity Body = kNoEntity;
  };

  [[nodiscard]] static std::expected<TriggerField, std::string>
  Stand(std::span<const Scenario::Volume> volumes, std::span<const Scenario::Event> events);

  [[nodiscard]] bool
  Listen(std::string_view event, std::span<const std::string_view> reads, std::string &error);

  enum class ProbeError : uint8_t { InvalidEntity, InvalidPosition, InvalidTime };
  [[nodiscard]] std::expected<void, ProbeError> Probe(Entity body, const Vec3 &atM, double nowS);

  [[nodiscard]] std::span<const Fired> Drain();

  [[nodiscard]] size_t EventCount() const { return Events_.size(); }

  [[nodiscard]] const std::string *EventNamed(uint16_t event) const;
  [[nodiscard]] size_t Unheard(std::string_view event) const;

  [[nodiscard]] size_t Overflowed() const { return Overflowed_; }

  [[nodiscard]] size_t Unseated() const { return Unseated_; }

private:
  TriggerField() = default;

  struct PreparedVolume {
    uint16_t Event = 0;
    When Opens = When::Enter;
    uint8_t Sphere = 0;
    Vec3 AtM;
    Vec3 ExtentM;
    double DwellS = 0.0;
  };

  [[nodiscard]] static std::expected<PreparedVolume, std::string>
  PrepareVolume(const Scenario::Volume &volume, uint16_t event);

  struct Occupant {
    Entity Body = kNoEntity;
    double SinceS = 0.0;
    bool Dwelt = false;
  };

  void Emit(uint16_t event, Entity body);
  void UpdateOccupant(size_t volume, Entity body, const Vec3 &atM, double nowS);
  double LastProbeS_ = 0.0;
  std::vector<std::vector<Occupant>> Occupants_;

  [[nodiscard]] static bool Inside(const PreparedVolume &door, const Vec3 &atM);

  std::vector<PreparedVolume> Volumes_;
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
