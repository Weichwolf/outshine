#include "Mixer.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <numbers>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::array<Scenario::Bus, 1> buses{};
  buses[0].Id = "master";
  std::array<Scenario::Sound, 1> sounds{};
  sounds[0].Id = "tone";
  Scenario::Voice oscillator;
  oscillator.Id = "osc";
  oscillator.Parameters = {{"frequency", "1000"}};
  Scenario::Voice gain;
  gain.Id = "gain";
  gain.Does = Scenario::Makes::Gain;
  gain.From = {"osc"};
  gain.Parameters = {{"gain", "0.25"}};
  Scenario::Voice output;
  output.Id = "output";
  output.Does = Scenario::Makes::Mix;
  output.From = {"gain", "osc"};
  Scenario::Voice unused = gain;
  unused.Id = "unused";
  unused.From = {"output"};
  sounds[0].Graph = {unused, gain, oscillator, output};
  std::array<Audio::Heard, 1> sources{};
  sources[0].Id = "tone";
  sources[0].Standing = true;
  Audio::Mixer mixer;
  Audio::Mixer control;
  CHECK(mixer.Stands(buses, sounds, 48000).has_value(), "forward references compile");
  auto ordered = sounds;
  ordered[0].Graph = {oscillator, gain, output};
  CHECK(control.Stands(buses, ordered, 48000).has_value(), "ordered graph compiles");
  std::array<float, 128> actual{};
  std::array<float, 128> expected{};
  std::string error;
  CHECK(mixer.Fills(actual, sources, {}, error), "compiled graph renders");
  CHECK(control.Fills(expected, sources, {}, error), "ordered graph renders");
  CHECK(actual == expected, "authored order and unused downstream node do not change output");
  for (size_t frame = 0; frame < actual.size() / 2; ++frame) {
    const double sample = 0.625 * std::sin(2 * std::numbers::pi * static_cast<double>(frame) / 48);
    CHECK(std::abs(actual[frame * 2] - sample) < 1e-6 && actual[frame * 2] == actual[frame * 2 + 1],
          "two inputs sum at declared output with stereo gain");
  }
  const auto reject = [&](const auto &invalid) {
    CHECK(!mixer.Stands(buses, invalid, 96000), "invalid graph rejected before publication");
    CHECK(mixer.Fills(actual, sources, {}, error) && control.Fills(expected, sources, {}, error),
          "mixers continue after rejected setup");
    CHECK(actual == expected, "graph rejection preserves state and sample rate");
  };
  auto invalid = sounds;
  invalid[0].Graph[1].From = {"missing"};
  reject(invalid);
  invalid = sounds;
  invalid[0].Graph[1].Id = "osc";
  reject(invalid);
  invalid = sounds;
  invalid[0].Graph[1].Id.clear();
  reject(invalid);
  invalid = sounds;
  invalid[0].Graph[1].From = {"gain"};
  reject(invalid);
  invalid = sounds;
  invalid[0].Graph[1].From = {"output"};
  reject(invalid);
  std::array<Scenario::Sound, 2> capacity{sounds[0], sounds[0]};
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
  CHECK(boundary.Stands(buses, capacity, 48000).has_value(), "aggregate node budget accepted");
  capacity[1].Graph.push_back(oscillator);
  reject(capacity);
  for (auto &sound : capacity) {
    sound.Graph = {oscillator, output};
    sound.Graph.back().From.assign(2048, "osc");
  }
  CHECK(boundary.Stands(buses, capacity, 48000).has_value(), "aggregate edge budget accepted");
  capacity[1].Graph.back().From.push_back("osc");
  reject(capacity);
  return Report();
}
