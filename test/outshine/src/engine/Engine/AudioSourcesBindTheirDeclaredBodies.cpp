#include "Engine.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string_view>

namespace {
void CheckSource(const outshine::Scenario::Document &scene,
                 bool right,
                 std::string_view expectedError = {}) {
  using namespace outshine;
  using namespace outshine::Test;
  Engine engine;
  CHECK(engine.loadScenario(scene), "audio body fixture starts independently");
  const auto prepared = engine.prepareAudio(48000);
  if (!expectedError.empty()) {
    CHECK(!prepared && prepared.error().contains(expectedError),
          "invalid audio binding is diagnosed");
    return;
  }
  CHECK(prepared, "named body binds to the audio source");
  if (!prepared) { return; }
  std::array<float, 128> audio{};
  CHECK(engine.mix(audio), "the first source snapshot mixes without rendering");
  double silent = 0;
  double audible = 0;
  for (size_t frame = 0; frame < audio.size() / 2; ++frame) {
    silent += std::abs(audio[2 * frame + (right ? 0 : 1)]);
    audible += std::abs(audio[2 * frame + (right ? 1 : 0)]);
  }
  CHECK(silent == 0 && audible > 1, "the named body determines the stereo side");
  CHECK(engine.entities().open(1), "native body storage can be cleared");
  CHECK(!engine.prepareAudio(48000), "removed body storage rejects stale bindings safely");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document scene;
  scene.Buses.emplace_back().Id = "master";
  Scenario::Body body;
  body.Name = "template";
  scene.Bodies.push_back(body);
  body.Placed = true;
  body.Name = "left";
  body.Stands.AtM[0] = -1;
  scene.Bodies.push_back(body);
  body.Name = "right";
  body.Stands.AtM[0] = 1;
  scene.Bodies.push_back(body);
  Audio::SoundSource tone;
  tone.Id = "tone";
  tone.Body = "right";
  tone.Spatial.Positional = true;
  tone.Graph.emplace_back().Id = "osc";
  tone.Graph.back().Parameters = {{.Name = "frequency", .Value = "1000"}};
  scene.Sounds.push_back(tone);
  for (const bool reordered : {false, true}) {
    if (reordered) { std::swap(scene.Bodies[1], scene.Bodies[2]); }
    for (const bool right : {false, true}) {
      scene.Sounds[0].Body = right ? "right" : "left";
      CheckSource(scene, right);
    }
  }
  scene.Sounds[0].Body = "missing";
  CheckSource(scene, false, "missing");
  scene.Sounds[0].Body = "template";
  CheckSource(scene, false, "placed body");
  scene.Sounds[0].Body = "right";
  scene.Bodies.push_back(scene.Bodies[1]);
  CheckSource(scene, true, "ambiguous");
  return Report();
}
