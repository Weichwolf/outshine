#include "Mixer.h"
#include "Check.h"
#include <array>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::array<Scenario::Bus, 1> buses{};
  buses[0].Id = "master";
  std::array<Scenario::Sound, 1> sounds{};
  sounds[0].Id = "tone";
  Scenario::Voice oscillator;
  oscillator.Id = "oscillator";
  oscillator.Does = Scenario::Makes::Oscillator;
  sounds[0].Graph.push_back(oscillator);
  Audio::Mixer mixer;
  Audio::Mixer control;
  std::string error;
  CHECK(mixer.Stands(buses, sounds, 48000) && control.Stands(buses, sounds, 48000),
        "matching mixers initialized");
  std::array<Audio::Heard, 1> sources{};
  sources[0].Id = "tone";
  sources[0].Standing = true;
  std::array<float, 128> actual{};
  std::array<float, 128> expected{};
  const auto compare = [&] {
    CHECK(mixer.Fills(actual, sources, {}, error) && control.Fills(expected, sources, {}, error),
          "both mixers render the next block");
    CHECK(actual == expected, "rejected setup preserves sample rate and oscillator phase");
  };
  compare();
  auto invalidBuses = buses;
  invalidBuses[0].Into = "absent";
  CHECK(!mixer.Stands(invalidBuses, sounds, 96000), "invalid routing rejected");
  compare();
  auto invalidSounds = sounds;
  invalidSounds[0].Id = "broken";
  invalidSounds[0].Graph.clear();
  CHECK(!mixer.Stands(buses, invalidSounds, 96000), "missing source rejected");
  CHECK(mixer.Voices() == 1 && mixer.Routing().GainOf("tone") == 1,
        "late validation failure preserves published sources and voices");
  return Report();
}
