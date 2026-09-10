#include "Mixer.h"
#include "Check.h"
#include <array>
#include <limits>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::array<Scenario::Bus, 2> buses{};
  buses[0].Id = "master";
  buses[1].Id = "effects";
  buses[1].Into = "master";
  buses[0].Reverberates = {.Declared = true, .SecondsRt60 = 0.3, .Damping = 0.4, .WetShare = 0.5};
  std::array<Scenario::Sound, 1> sounds{};
  sounds[0].Id = "tone";
  sounds[0].SendShare = 0.4;
  sounds[0].Graph.push_back({.Id = "osc"});
  Audio::Mixer mixer;
  Audio::Mixer control;
  const bool initialized =
      mixer.Stands(buses, sounds, 48000) && control.Stands(buses, sounds, 48000);
  CHECK(initialized, "matching mixers with reverberation prepared");
  if (!initialized) { return Report(); }
  std::array<Audio::Heard, 1> sources{};
  sources[0].Id = "tone";
  sources[0].Standing = true;
  std::array<float, 4096> actual{};
  std::array<float, 4096> expected{};
  std::string error;
  const auto compare = [&] {
    CHECK(mixer.Fills(actual, sources, {}, error) && control.Fills(expected, sources, {}, error),
          "both mixers render");
    CHECK(actual == expected, "rejected room preserves rate, phase and reverberation rings");
  };
  compare();
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double infinity = std::numeric_limits<double>::infinity();
  for (const double value : {-1.0, nan, infinity}) {
    auto invalid = buses;
    invalid[0].Reverberates.SecondsRt60 = value;
    CHECK(!mixer.Stands(invalid, sounds, 96000), "invalid decay time rejected");
    compare();
  }
  for (auto field : {&Scenario::Room::Damping, &Scenario::Room::WetShare}) {
    for (const double value : {-0.1, 1.1, nan, infinity}) {
      auto invalid = buses;
      invalid[0].Reverberates.*field = value;
      CHECK(!mixer.Stands(invalid, sounds, 96000), "invalid normalized room parameter rejected");
      compare();
    }
  }
  auto invalid = buses;
  invalid[1].Reverberates = buses[0].Reverberates;
  invalid[1].Reverberates.Damping = nan;
  CHECK(!mixer.Stands(invalid, sounds, 96000), "all declared rooms are validated");
  compare();
  CHECK(!mixer.Stands(buses, sounds, std::numeric_limits<int>::max()),
        "sample rate cannot exceed the ring storage budget");
  compare();
  CHECK(!mixer.Stands(buses, sounds, 64000000), "combined rings cannot exceed the storage budget");
  compare();
  buses[0].Reverberates = {.Declared = true, .SecondsRt60 = 0, .Damping = 0, .WetShare = 1};
  CHECK(mixer.Stands(buses, sounds, 48000).has_value(), "zero decay disables reverberation");
  return Report();
}
