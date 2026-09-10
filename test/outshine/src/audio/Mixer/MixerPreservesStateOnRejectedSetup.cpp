#include "Mixer.h"
#include "Check.h"
#include <array>
#include <string>
#include <limits>

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
  for (const char *value : {"440garbage", "nan", "inf", "", "1e9999"}) {
    auto invalidParameter = sounds;
    invalidParameter[0].Graph[0].Parameters.push_back({"frequency", value});
    CHECK(!mixer.Stands(buses, invalidParameter, 48000), "malformed numeric parameter rejected");
  }
  auto invalidDelay = sounds;
  invalidDelay[0].Graph[0].Does = Scenario::Makes::Delay;
  for (const char *value : {"-1", "1e30"}) {
    invalidDelay[0].Graph[0].Parameters = {{"delayS", value}};
    CHECK(!mixer.Stands(buses, invalidDelay, 48000), "invalid or over-budget delay rejected");
  }
  for (const bool positional : {false, true}) {
    for (const double value : {-1.0,
                               std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()}) {
      for (const auto member : {&Scenario::Emitter::MostM,
                                &Scenario::Emitter::Rolloff,
                                &Scenario::Emitter::BlockedHz,
                                &Scenario::Emitter::BlockedGain}) {
        auto invalid = sounds;
        invalid[0].Heard.Positional = positional;
        invalid[0].Heard.*member = value;
        CHECK(!mixer.Stands(buses, invalid, 96000), "invalid spatial parameter rejected");
        compare();
      }
      auto invalid = sounds;
      invalid[0].Heard.Positional = positional;
      invalid[0].SendShare = value;
      CHECK(!mixer.Stands(buses, invalid, 96000), "invalid reverb send rejected");
      compare();
    }
    auto invalid = sounds;
    invalid[0].Heard.Positional = positional;
    invalid[0].Heard.BlockedGain = 1.01;
    CHECK(!mixer.Stands(buses, invalid, 96000), "obstruction cannot amplify the source");
    compare();
    invalid[0].Heard.BlockedGain = 1;
    invalid[0].Heard.By = static_cast<Scenario::Falls>(255);
    CHECK(!mixer.Stands(buses, invalid, 96000), "unknown distance model rejected");
    compare();
  }
  for (const auto law :
       {Scenario::Falls::Linear, Scenario::Falls::Inverse, Scenario::Falls::Exponential}) {
    auto boundary = sounds;
    boundary[0].Heard.Positional = true;
    boundary[0].Heard.By = law;
    boundary[0].Heard.MostM = 0;
    boundary[0].Heard.Rolloff = 0;
    boundary[0].Heard.BlockedGain = 0;
    boundary[0].Heard.BlockedHz = 0;
    boundary[0].SendShare = 0;
    CHECK(mixer.Stands(buses, boundary, 48000).has_value(), "valid zero boundaries accepted");
    boundary[0].Heard.BlockedGain = 1;
    boundary[0].SendShare = 2;
    CHECK(mixer.Stands(buses, boundary, 48000).has_value(),
          "unity obstruction and boosted send accepted");
  }
  auto delayed = sounds;
  Scenario::Voice delay;
  delay.Id = "delay";
  delay.Does = Scenario::Makes::Delay;
  delay.From = {"oscillator"};
  delay.Parameters = {{"delayS", "0"}, {"feedback", "0"}};
  delayed[0].Graph.push_back(delay);
  CHECK(mixer.Stands(buses, delayed, 48000) && control.Stands(buses, sounds, 48000),
        "valid delay parameters prepare before rendering");
  CHECK(mixer.Fills(actual, sources, {}, error) && control.Fills(expected, sources, {}, error),
        "prepared delay renders");
  CHECK(actual[0] == 0 && actual[1] == 0, "delay ring starts silent");
  for (size_t sample = 2; sample < actual.size(); ++sample) {
    CHECK(actual[sample] == expected[sample - 2], "zero-second delay retains one stereo frame");
  }
  return Report();
}
