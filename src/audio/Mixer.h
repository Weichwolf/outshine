#ifndef OUTSHINE_AUDIO_MIXER_H
#define OUTSHINE_AUDIO_MIXER_H

#include <memory>
#include <expected>
#include <span>
#include <string>
#include <vector>

#include <audio/AudioScene.h>

#include "math/Vec3.h"
#include "BusGraph.h"

namespace outshine::Audio {

constexpr double kSpeedOfSoundUnsaidMs = 343.0;
constexpr int kSampleRateUnsaidHz = 48000;

struct Heard {
  std::string Id;
  Vec3 AtM;
  Vec3 VelocityMs;
  double Blocked = 0.0;
  bool Standing = false;
};

struct Listening {
  Vec3 AtM;
  Vec3 VelocityMs;
  Vec3 ForwardXyz = {{0.0, 0.0, -1.0}};
  Vec3 RightXyz = {{1.0, 0.0, 0.0}};
};

class Mixer {
public:
  Mixer();
  ~Mixer();
  Mixer(Mixer &&) noexcept;
  Mixer &operator=(Mixer &&) noexcept;
  Mixer(const Mixer &) = delete;
  Mixer &operator=(const Mixer &) = delete;

  [[nodiscard]] std::expected<void, std::string>
  Configure(std::span<const MixBus> buses, std::span<const SoundSource> declared, int rate);

  [[nodiscard]] bool Mix(std::span<float> stereo,
                         std::span<const Heard> sources,
                         const Listening &ear,
                         std::string &error);

  [[nodiscard]] size_t VoiceCount() const;
  [[nodiscard]] const BusGraph &Buses() const;

  [[nodiscard]] double SpeedOfSoundMs() const { return SpeedOfSoundMs_; }

private:
  struct Held;
  std::unique_ptr<Held> Held_;
  double SpeedOfSoundMs_ = kSpeedOfSoundUnsaidMs;
  int Rate_ = kSampleRateUnsaidHz;
};

}
#endif
