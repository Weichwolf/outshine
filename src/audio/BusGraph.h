#ifndef OUTSHINE_AUDIO_BUSGRAPH_H
#define OUTSHINE_AUDIO_BUSGRAPH_H

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <scenario/Scenario.h>

namespace outshine::Audio {

class BusGraph {
public:
  [[nodiscard]] std::expected<void, std::string> Build(std::span<const Scenario::Bus> buses,
                                                       std::span<const Scenario::Sound> sounds);

  [[nodiscard]] size_t BusCount() const { return Buses_.size(); }

  [[nodiscard]] size_t SoundCount() const { return Sounds_.size(); }

  [[nodiscard]] std::string_view Master() const;

  [[nodiscard]] bool Play(std::string_view id, std::string &error);

  [[nodiscard]] size_t Playing() const { return Voices_; }

  [[nodiscard]] double GainOf(std::string_view id) const;

private:
  [[nodiscard]] bool DefineBuses(std::span<const Scenario::Bus> buses, std::string &error);
  [[nodiscard]] bool RouteBuses(std::span<const Scenario::Bus> buses, std::string &error);
  [[nodiscard]] bool DefineSounds(std::span<const Scenario::Sound> sounds, std::string &error);

  struct Row {
    std::string Id;
    int Into = -1;
    double Gain = 1.0;
  };

  struct Source {
    std::string Id;
    int Into = -1;
    double Gain = 1.0;
    bool Positional = false;
  };

  [[nodiscard]] int BusNamed(std::string_view id) const;

  std::vector<Row> Buses_;
  std::vector<Source> Sounds_;
  int Master_ = -1;
  size_t Voices_ = 0;
};

}
#endif
