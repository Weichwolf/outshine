#include "Mixer.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <numbers>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Audio;
  using namespace outshine::Test;
  std::array<MixBus, 1> buses{};
  buses[0].Id = "master";
  std::array<SoundSource, 1> sounds{};
  sounds[0].Id = "tone";
  SignalNode oscillator;
  oscillator.Id = "osc";
  oscillator.Parameters = {{"frequency", "1000"}};
  SignalNode gain;
  gain.Id = "gain";
  gain.Processor = ProcessorKind::Gain;
  gain.Inputs = {"osc"};
  gain.Parameters = {{"gain", "0.25"}};
  SignalNode output;
  output.Id = "output";
  output.Processor = ProcessorKind::Mix;
  output.Inputs = {"gain", "osc"};
  SignalNode unused = gain;
  unused.Id = "unused";
  unused.Inputs = {"output"};
  sounds[0].Graph = {unused, gain, oscillator, output};
  std::array<Audio::Heard, 1> sources{};
  sources[0].Id = "tone";
  sources[0].Standing = true;
  Audio::Mixer mixer;
  Audio::Mixer control;
  CHECK(mixer.Configure(buses, sounds, 48000).has_value(), "forward references compile");
  auto ordered = sounds;
  ordered[0].Graph = {oscillator, gain, output};
  CHECK(control.Configure(buses, ordered, 48000).has_value(), "ordered graph compiles");
  std::array<float, 128> actual{};
  std::array<float, 128> expected{};
  std::string error;
  CHECK(mixer.Mix(actual, sources, {}, error), "compiled graph renders");
  CHECK(control.Mix(expected, sources, {}, error), "ordered graph renders");
  CHECK(actual == expected, "authored order and unused downstream node do not change output");
  for (size_t frame = 0; frame < actual.size() / 2; ++frame) {
    const double sample = 0.625 * std::sin(2 * std::numbers::pi * static_cast<double>(frame) / 48);
    CHECK(std::abs(actual[frame * 2] - sample) < 1e-6 && actual[frame * 2] == actual[frame * 2 + 1],
          "two inputs sum at declared output with stereo gain");
  }
  const auto reject = [&](const auto &invalid) {
    CHECK(!mixer.Configure(buses, invalid, 96000), "invalid graph rejected before publication");
    CHECK(mixer.Mix(actual, sources, {}, error) && control.Mix(expected, sources, {}, error),
          "mixers continue after rejected setup");
    CHECK(actual == expected, "graph rejection preserves state and sample rate");
  };
  auto invalid = sounds;
  invalid[0].Graph[1].Inputs = {"missing"};
  reject(invalid);
  invalid = sounds;
  invalid[0].Graph[1].Id = "osc";
  reject(invalid);
  invalid = sounds;
  invalid[0].Graph[1].Id.clear();
  reject(invalid);
  invalid = sounds;
  invalid[0].Graph[1].Inputs = {"gain"};
  reject(invalid);
  invalid = sounds;
  invalid[0].Graph[1].Inputs = {"output"};
  reject(invalid);
  std::array<SoundSource, 2> capacity{sounds[0], sounds[0]};
  capacity[1].Id = "second";
  for (auto &sound : capacity) {
    sound.Graph.clear();
    for (size_t index = 0; index < 512; ++index) {
      auto node = oscillator;
      node.Id = std::to_string(index);
      sound.Graph.push_back(node);
    }
  }
  Audio::Mixer boundary;
  CHECK(boundary.Configure(buses, capacity, 48000).has_value(), "aggregate node budget accepted");
  capacity[1].Graph.push_back(oscillator);
  reject(capacity);
  for (auto &sound : capacity) {
    sound.Graph = {oscillator, output};
    sound.Graph.back().Inputs.assign(2048, "osc");
  }
  CHECK(boundary.Configure(buses, capacity, 48000).has_value(), "aggregate edge budget accepted");
  capacity[1].Graph.back().Inputs.push_back("osc");
  reject(capacity);
  return Report();
}
