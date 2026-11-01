#include <Engine.h>
#include "Triggers.h"
#include "Check.h"
#include <algorithm>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document scene;
  scene.Events.emplace_back().Name = "entered";
  Scenario::Volume volume;
  volume.Id = "room";
  volume.Shape = "box";
  volume.ExtentM = {{1, 1, 1}};
  volume.When = "enter";
  volume.Fires = "entered";
  scene.Volumes.push_back(volume);
  Scenario::Body body;
  body.Placed = true;
  body.Name = "outside";
  body.Stands.AtM[0] = 10;
  scene.Bodies.push_back(body);
  body.Name = "inside-a";
  body.Stands.AtM[0] = 0;
  scene.Bodies.push_back(body);
  body.Name = "inside-b";
  scene.Bodies.push_back(body);
  Engine engine;
  CHECK(engine.loadScenario(scene), "trigger simulation prepared without a renderer");
  const auto fired = [&] {
    return std::ranges::count_if(engine.unacted(), [](const std::string &line) {
      return line.starts_with("a volume fired event ");
    });
  };
  CHECK(engine.advance().has_value(), "headless simulation advances");
  CHECK(fired() == 2, "both inside entities fire; first outside body does not mask them");
  CHECK(engine.advance().has_value(), "second headless step");
  CHECK(fired() == 2, "remaining inside does not repeat enter events");
  auto invalid = scene;
  invalid.Volumes[0].Fires = "missing";
  Engine rejected;
  const auto loaded = rejected.loadScenario(invalid);
  CHECK(!loaded && loaded.error().contains("missing"), "startup rejects an unknown trigger event");
  return Report();
}
