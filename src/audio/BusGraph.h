#ifndef OUTSHINE_AUDIO_BUSGRAPH_H
#define OUTSHINE_AUDIO_BUSGRAPH_H

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <outshine/Scenario.h>

namespace outshine::Audio {

// the declared bus graph, stood up once: sources route into buses, buses into buses, ONE
// master -- so a scenario ducks music under dialogue without the engine knowing what music
// or dialogue is. A cycle is a refusal naming it; the mix that walks this takes nothing
// from the allocator, because the audio callback's deadline is the buffer's, not the frame's
class BusGraph {
public:
  [[nodiscard]] bool Build(std::span<const Bus> buses, std::span<const Sound> sounds,
                           std::string &error);

  [[nodiscard]] size_t BusCount() const { return Buses_.size(); }
  [[nodiscard]] size_t SoundCount() const { return Sounds_.size(); }
  [[nodiscard]] std::string_view Master() const;

  // a sound is played BY ID and the engine answers whether it started -- a missing sound
  // is a named refusal, never a silence (a silence is what a working sound also sounds
  // like when nothing is happening)
  [[nodiscard]] bool Play(std::string_view id, std::string &error);
  [[nodiscard]] size_t Playing() const { return Voices_; }

  // the gain a source arrives at the master with: its own, every bus on its route, and
  // (for a positional source) the declared falloff against the one listener
  [[nodiscard]] double GainOf(std::string_view id) const;
  [[nodiscard]] double GainAt(std::string_view id, const double sourceM[3],
                              const double listenerM[3]) const;

private:
  struct Row {
    std::string Id;
    int Into = -1; // index into Buses_, -1 = the master itself
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
