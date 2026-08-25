#ifndef OUTSHINE_AUDIO_BUSGRAPH_H
#define OUTSHINE_AUDIO_BUSGRAPH_H

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <Scenario.h>

namespace outshine::Audio {

class BusGraph {
public:
  [[nodiscard]] bool Build(std::span<const Bus> buses, std::span<const Sound> sounds,
                           std::string &error);

  [[nodiscard]] size_t BusCount() const { return Buses_.size(); }
  [[nodiscard]] size_t SoundCount() const { return Sounds_.size(); }
  [[nodiscard]] std::string_view Master() const;

  [[nodiscard]] bool Play(std::string_view id, std::string &error);
  [[nodiscard]] size_t Playing() const { return Voices_; }

  [[nodiscard]] double GainOf(std::string_view id) const;
  [[nodiscard]] double GainAt(std::string_view id, const double sourceM[3],
                              const double listenerM[3]) const;

private:
  struct Row {
    std::string Id;
    int Into = -1;
    double Gain = 1.0;
  };
  struct Source {
    std::string Id;
    int Into = -1;
    double Gain = 1.0;
    double FalloffM = 0.0;
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
