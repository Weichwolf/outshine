#include "Mixer.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <string>

int main() {
  using namespace outshine::Audio;
  using namespace outshine::Test;
  std::array<MixBus, 2> buses{};
  buses[0].Id = "master";
  buses[0].GainDb = -6;
  buses[1].Id = "effects";
  buses[1].Output = "master";
  buses[1].GainDb = -12;
  std::array<SoundSource, 2> sounds{};
  SignalNode oscillator;
  oscillator.Id = "osc";
  oscillator.Parameters = {{.Name = "frequency", .Value = "1000"}};
  for (size_t row = 0; row < sounds.size(); ++row) {
    sounds[row].Id = row == 0 ? "left" : "right";
    sounds[row].Bus = "effects";
    sounds[row].Spatial.Positional = true;
    sounds[row].Graph = {oscillator};
  }
  std::array<Heard, 2> canonical{};
  canonical[0].Id = "left";
  canonical[0].AtM[0] = -1;
  canonical[0].Standing = true;
  canonical[1].Id = "right";
  canonical[1].AtM[0] = 1;
  canonical[1].Standing = true;
  std::array<Heard, 5> unordered{canonical[1], canonical[1], canonical[0], {}, {}};
  unordered[0].Id = "foreign";
  unordered[3].Id = "left";
  unordered[4].Id = "right";
  Mixer actual;
  Mixer reference;
  CHECK(actual.Configure(buses, sounds, 48000) && reference.Configure(buses, sounds, 48000),
        "both mixers are prepared");
  constexpr size_t kSamplesAcrossBlocks = 2050;
  constexpr double kSampleTolerance = 1e-6;
  std::array<float, kSamplesAcrossBlocks> samples{};
  std::array<float, kSamplesAcrossBlocks> expected{};
  std::string error;
  CHECK(actual.Mix(samples, unordered, {}, error) && reference.Mix(expected, canonical, {}, error),
        "mix spans several internal blocks");
  CHECK(samples == expected, "unordered duplicate and unknown identities preserve every sample");
  const double gain = std::pow(10.0, -18.0 / 20.0);
  for (size_t frame = 0; frame < samples.size() / 2; ++frame) {
    const double tone = gain * std::sin(2 * std::numbers::pi * static_cast<double>(frame) / 48);
    CHECK(std::abs(samples[frame * 2] - tone) < kSampleTolerance &&
              std::abs(samples[frame * 2 + 1] - tone) < kSampleTolerance,
          "independent oscillator, bus gain and full-left/full-right oracle agree");
  }
  CHECK(actual.Mix(samples, {}, {}, error), "missing sources mix successfully");
  for (const float sample : samples) {
    CHECK(sample == 0, "no previous source leaks into silence");
  }
  return Report();
}
