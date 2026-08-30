#ifndef OUTSHINE_AUDIO_MIXER_H
#define OUTSHINE_AUDIO_MIXER_H

#include <memory>
#include <span>
#include <string>
#include <vector>

#include <Scenario.h>

#include "BusGraph.h"

namespace outshine::Audio {

struct Heard {
  std::string Id;
  double AtM[3] = {0.0, 0.0, 0.0};
  double VelocityMs[3] = {0.0, 0.0, 0.0};
  double Blocked = 0.0;
  bool Standing = false;
};

struct Listening {
  double AtM[3] = {0.0, 0.0, 0.0};
  double VelocityMs[3] = {0.0, 0.0, 0.0};
  double ForwardXyz[3] = {0.0, 0.0, -1.0};
  double RightXyz[3] = {1.0, 0.0, 0.0};
};

class Mixer {
public:
  Mixer();
  ~Mixer();
  Mixer(Mixer &&) noexcept;
  Mixer &operator=(Mixer &&) noexcept;
  Mixer(const Mixer &) = delete;
  Mixer &operator=(const Mixer &) = delete;

  [[nodiscard]] bool
  Stands(std::span<const Bus> buses, std::span<const Sound> declared, int rate, std::string &error);

  [[nodiscard]] bool Fills(std::span<float> stereo,
                           std::span<const Heard> sources,
                           const Listening &ear,
                           std::string &error);

  [[nodiscard]] size_t Voices() const;
  [[nodiscard]] const BusGraph &Routing() const;

  [[nodiscard]] double SpeedOfSoundMs() const { return SpeedOfSoundMs_; }

private:
  struct Held;
  std::unique_ptr<Held> Held_;
  double SpeedOfSoundMs_ = 343.0;
  int Rate_ = 48000;
};

} // namespace outshine::Audio
#endif
