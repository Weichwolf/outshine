#include <Outshine.h>
#include "Check.h"
#include <array>
#include <cmath>
#include <numbers>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Engine engine;
  Engine control;
  std::array<float, 128> actual{};
  actual.fill(7);
  const auto untouched = actual;
  CHECK(!engine.mix(actual) && actual == untouched,
        "unprepared mixing leaves the buffer untouched");
  CHECK(!engine.prepareAudio(48000), "audio preparation requires a declaration");
  Scenario::Document scene;
  scene.Buses.emplace_back().Id = "master";
  Scenario::Sound tone;
  tone.Id = "tone";
  Scenario::Voice oscillator;
  oscillator.Id = "osc";
  oscillator.Parameters = {{"frequency", "1000"}};
  tone.Graph.push_back(oscillator);
  scene.Sounds.push_back(tone);
  const bool declared = engine.declare(scene) && control.declare(scene);
  CHECK(declared, "audio-only declaration needs no render target");
  if (!declared) { return Report(); }
  CHECK(!engine.mix(actual), "declaring does not implicitly prepare audio");
  const bool prepared = engine.prepareAudio(48000) && control.prepareAudio(48000);
  CHECK(prepared, "audio preparation is explicit and device-independent");
  if (!prepared) { return Report(); }
  std::array<float, 128> expected{};
  const auto compare = [&] {
    CHECK(engine.mix(actual) && control.mix(expected), "both engines mix the next block");
    CHECK(actual == expected, "failed operation preserves DSP state");
  };
  compare();
  const auto checkRate = [&](double samplesPerCycle) {
    for (size_t frame = 0; frame < actual.size() / 2; ++frame) {
      const double value =
          0.5 * std::sin(2 * std::numbers::pi * static_cast<double>(frame) / samplesPerCycle);
      CHECK(std::abs(actual[2 * frame] - value) < 1e-6 &&
                actual[2 * frame] == actual[2 * frame + 1],
            "first samples use the prepared rate without waiting for a simulation tick");
    }
  };
  checkRate(48);
  const auto rejected = engine.prepareAudio(0);
  CHECK(!rejected, "invalid rate rejected");
  compare();
  CHECK(!rejected && !rejected.error().empty(),
        "preparation error remains owned after successful mixing");
  std::array<float, 3> odd{7, 8, 9};
  const auto untouchedOdd = odd;
  CHECK(!engine.mix(odd) && odd == untouchedOdd, "odd buffer rejected unchanged");
  CHECK(engine.mix({}).has_value(), "empty output advances nothing");
  compare();
  CHECK(engine.prepareAudio(96000) && control.prepareAudio(96000),
        "explicit reprepare changes rate and resets DSP");
  compare();
  checkRate(96);
  scene.Sounds[0].Graph[0].Parameters[0].Value = "2000";
  CHECK(engine.declare(scene).has_value(), "new sound declaration published");
  CHECK(!engine.mix(actual), "new declaration invalidates old audio preparation");
  CHECK(engine.prepareAudio(48000) && engine.mix(actual), "new audio graph prepared and mixed");
  checkRate(24);
  CHECK(engine.declare({}).has_value(), "empty declaration replaces sound scene");
  CHECK(!engine.mix(actual), "removing all sounds invalidates audio preparation too");
  CHECK(!engine.prepareAudio(48000), "missing master bus is an explicit preparation error");
  scene.Sounds.clear();
  CHECK(engine.declare(scene).has_value(), "silent scene retains explicit master routing");
  CHECK(engine.prepareAudio(48000) && engine.mix(actual), "empty audio scene prepares as silence");
  const std::array<float, 128> silence{};
  CHECK(actual == silence, "old sound does not survive empty redeclaration");
  return Report();
}
