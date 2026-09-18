#include "Mixer.h"
#include "Check.h"
#include <array>
#include <string>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::array<Audio::MixBus, 1> buses{};
  buses[0].Id = "master";
  std::array<Audio::SoundSource, 1> sounds{};
  sounds[0].Id = "tone";
  Audio::SignalNode oscillator;
  oscillator.Id = "oscillator";
  oscillator.Processor = Audio::ProcessorKind::Oscillator;
  sounds[0].Graph.push_back(oscillator);
  Audio::Mixer mixer;
  Audio::Mixer control;
  std::string error;
  CHECK(mixer.Configure(buses, sounds, 48000) && control.Configure(buses, sounds, 48000),
        "matching mixers initialized");
  std::array<Audio::Heard, 1> sources{};
  sources[0].Id = "tone";
  sources[0].Standing = true;
  std::array<float, 128> actual{};
  std::array<float, 128> expected{};
  const auto compare = [&] {
    CHECK(mixer.Mix(actual, sources, {}, error) && control.Mix(expected, sources, {}, error),
          "both mixers render the next block");
    CHECK(actual == expected, "rejected setup preserves sample rate and oscillator phase");
  };
  compare();
  auto invalidBuses = buses;
  invalidBuses[0].Output = "absent";
  CHECK(!mixer.Configure(invalidBuses, sounds, 96000), "invalid routing rejected");
  compare();
  auto invalidSounds = sounds;
  invalidSounds[0].Id = "broken";
  invalidSounds[0].Graph.clear();
  CHECK(!mixer.Configure(buses, invalidSounds, 96000), "missing source rejected");
  CHECK(mixer.VoiceCount() == 1 && mixer.Buses().GainOf("tone") == 1,
        "late validation failure preserves published sources and voices");
  for (const char *value : {"440garbage", "nan", "inf", "", "1e9999"}) {
    auto invalidParameter = sounds;
    invalidParameter[0].Graph[0].Parameters.push_back({"frequency", value});
    CHECK(!mixer.Configure(buses, invalidParameter, 48000), "malformed numeric parameter rejected");
  }
  auto invalidDelay = sounds;
  invalidDelay[0].Graph[0].Processor = Audio::ProcessorKind::Delay;
  for (const char *value : {"-1", "1e30"}) {
    invalidDelay[0].Graph[0].Parameters = {{"delayS", value}};
    CHECK(!mixer.Configure(buses, invalidDelay, 48000), "invalid or over-budget delay rejected");
  }
  for (const bool positional : {false, true}) {
    for (const double value : {-1.0,
                               std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()}) {
      for (const auto member : {&Audio::SpatialSource::MaximumDistanceM,
                                &Audio::SpatialSource::Rolloff,
                                &Audio::SpatialSource::ObstructedCutoffHz,
                                &Audio::SpatialSource::ObstructedGain}) {
        auto invalid = sounds;
        invalid[0].Spatial.Positional = positional;
        invalid[0].Spatial.*member = value;
        CHECK(!mixer.Configure(buses, invalid, 96000), "invalid spatial parameter rejected");
        compare();
      }
      auto invalid = sounds;
      invalid[0].Spatial.Positional = positional;
      invalid[0].ReverbSend = value;
      CHECK(!mixer.Configure(buses, invalid, 96000), "invalid reverb send rejected");
      compare();
    }
    auto invalid = sounds;
    invalid[0].Spatial.Positional = positional;
    invalid[0].Spatial.ObstructedGain = 1.01;
    CHECK(!mixer.Configure(buses, invalid, 96000), "obstruction cannot amplify the source");
    compare();
    invalid[0].Spatial.ObstructedGain = 1;
    invalid[0].Spatial.Attenuation = static_cast<Audio::AttenuationModel>(255);
    CHECK(!mixer.Configure(buses, invalid, 96000), "unknown distance model rejected");
    compare();
  }
  for (const auto law : {Audio::AttenuationModel::Linear,
                         Audio::AttenuationModel::Inverse,
                         Audio::AttenuationModel::Exponential}) {
    auto boundary = sounds;
    boundary[0].Spatial.Positional = true;
    boundary[0].Spatial.Attenuation = law;
    boundary[0].Spatial.MaximumDistanceM = 0;
    boundary[0].Spatial.Rolloff = 0;
    boundary[0].Spatial.ObstructedGain = 0;
    boundary[0].Spatial.ObstructedCutoffHz = 0;
    boundary[0].ReverbSend = 0;
    CHECK(mixer.Configure(buses, boundary, 48000).has_value(), "valid zero boundaries accepted");
    boundary[0].Spatial.ObstructedGain = 1;
    boundary[0].ReverbSend = 2;
    CHECK(mixer.Configure(buses, boundary, 48000).has_value(),
          "unity obstruction and boosted send accepted");
  }
  auto delayed = sounds;
  Audio::SignalNode delay;
  delay.Id = "delay";
  delay.Processor = Audio::ProcessorKind::Delay;
  delay.Inputs = {"oscillator"};
  delay.Parameters = {{"delayS", "0"}, {"feedback", "0"}};
  delayed[0].Graph.push_back(delay);
  CHECK(mixer.Configure(buses, delayed, 48000) && control.Configure(buses, sounds, 48000),
        "valid delay parameters prepare before rendering");
  CHECK(mixer.Mix(actual, sources, {}, error) && control.Mix(expected, sources, {}, error),
        "prepared delay renders");
  CHECK(actual[0] == 0 && actual[1] == 0, "delay ring starts silent");
  for (size_t sample = 2; sample < actual.size(); ++sample) {
    CHECK(actual[sample] == expected[sample - 2], "zero-second delay retains one stereo frame");
  }
  return Report();
}
