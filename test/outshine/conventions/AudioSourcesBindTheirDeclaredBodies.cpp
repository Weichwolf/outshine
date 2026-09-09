#include <Outshine.h>
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>

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
  Scenario::Sound tone;
  tone.Id = "tone";
  tone.On = "right";
  tone.Heard.Positional = true;
  tone.Graph.emplace_back().Id = "osc";
  tone.Graph.back().Parameters = {{"frequency", "1000"}};
  scene.Sounds.push_back(tone);
  Engine engine;
  std::array<float, 128> audio{};
  CHECK(engine.declare(scene).has_value(), "bound source declaration accepted");
  CHECK(!engine.prepareAudio(48000), "bound source requires current assembly");
  const auto render = [&](bool right) {
    const auto prepared = engine.prepareAudio(48000);
    CHECK(prepared.has_value(), prepared ? "audio prepared" : prepared.error().c_str());
    if (!prepared) { return; }
    CHECK(engine.mix(audio).has_value(), "initial source snapshot mixes without rendering");
    double silent = 0;
    double audible = 0;
    for (size_t frame = 0; frame < audio.size() / 2; ++frame) {
      silent += std::abs(audio[2 * frame + (right ? 0 : 1)]);
      audible += std::abs(audio[2 * frame + (right ? 1 : 0)]);
    }
    CHECK(silent == 0 && audible > 1, "named body determines the stereo side");
  };
  CHECK(engine.assemble().has_value(), "placed bodies assembled");
  render(true);
  std::swap(scene.Bodies[1], scene.Bodies[2]);
  CHECK(engine.declare(scene).has_value(), "body order changed");
  CHECK(!engine.prepareAudio(48000), "old assembly cannot bind new declaration");
  CHECK(engine.assemble().has_value(), "reordered bodies assembled");
  render(true);
  CHECK(engine.scene().open(1), "client replaces the public scene storage");
  CHECK(!engine.prepareAudio(48000),
        "removed body storage rejects binding without invalid indexing");
  CHECK(engine.assemble().has_value(), "assembly restores the native entity storage");
  render(true);
  scene.Sounds[0].On = "left";
  CHECK(engine.declare(scene) && engine.assemble(), "source target changed");
  render(false);
  for (const char *target : {"missing", "template"}) {
    scene.Sounds[0].On = target;
    CHECK(engine.declare(scene) && engine.assemble(), "invalid audio target awaits setup");
    CHECK(!engine.prepareAudio(48000), "missing or unplaced target rejected");
  }
  scene.Sounds[0].On = "right";
  scene.Bodies.push_back(scene.Bodies[1]);
  CHECK(engine.declare(scene) && engine.assemble(), "ambiguous names await binding validation");
  CHECK(!engine.prepareAudio(48000), "ambiguous target is never selected by order");
  return Report();
}
