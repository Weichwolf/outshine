#include "Mixer.h"
#include "Check.h"
#include <array>
#include <limits>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::array<Audio::MixBus, 2> buses{};
  buses[0].Id = "master";
  buses[1].Id = "effects";
  buses[1].Output = "master";
  buses[0].Reverberation = {.Enabled = true, .DecayTimeS = 0.3, .Damping = 0.4, .WetGain = 0.5};
  std::array<Audio::SoundSource, 1> sounds{};
  sounds[0].Id = "tone";
  sounds[0].ReverbSend = 0.4;
  sounds[0].Graph.push_back({.Id = "osc"});
  Audio::Mixer mixer;
  Audio::Mixer control;
  const bool initialized =
      mixer.Configure(buses, sounds, 48000) && control.Configure(buses, sounds, 48000);
  CHECK(initialized, "matching mixers with reverberation prepared");
  if (!initialized) { return Report(); }
  std::array<Audio::Heard, 1> sources{};
  sources[0].Id = "tone";
  sources[0].Standing = true;
  std::array<float, 4096> actual{};
  std::array<float, 4096> expected{};
  std::string error;
  const auto compare = [&] {
    CHECK(mixer.Mix(actual, sources, {}, error) && control.Mix(expected, sources, {}, error),
          "both mixers render");
    CHECK(actual == expected, "rejected room preserves rate, phase and reverberation rings");
  };
  compare();
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double infinity = std::numeric_limits<double>::infinity();
  for (const double value : {-1.0, nan, infinity}) {
    auto invalid = buses;
    invalid[0].Reverberation.DecayTimeS = value;
    CHECK(!mixer.Configure(invalid, sounds, 96000), "invalid decay time rejected");
    compare();
  }
  for (auto field : {&Audio::Reverb::Damping, &Audio::Reverb::WetGain}) {
    for (const double value : {-0.1, 1.1, nan, infinity}) {
      auto invalid = buses;
      invalid[0].Reverberation.*field = value;
      CHECK(!mixer.Configure(invalid, sounds, 96000), "invalid normalized room parameter rejected");
      compare();
    }
  }
  auto invalid = buses;
  invalid[1].Reverberation = buses[0].Reverberation;
  invalid[1].Reverberation.Damping = nan;
  CHECK(!mixer.Configure(invalid, sounds, 96000), "all declared rooms are validated");
  compare();
  CHECK(!mixer.Configure(buses, sounds, std::numeric_limits<int>::max()),
        "sample rate cannot exceed the ring storage budget");
  compare();
  CHECK(!mixer.Configure(buses, sounds, 64000000),
        "combined rings cannot exceed the storage budget");
  compare();
  buses[0].Reverberation = {.Enabled = true, .DecayTimeS = 0, .Damping = 0, .WetGain = 1};
  CHECK(mixer.Configure(buses, sounds, 48000).has_value(), "zero decay disables reverberation");
  return Report();
}
