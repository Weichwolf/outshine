#include <Outshine.h>
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
  CHECK(engine.declare(scene) && engine.assemble(),
        "trigger simulation prepared without a renderer");
  const auto fired = [&] {
    return std::ranges::count_if(engine.unacted(), [](const std::string &line) {
      return line.starts_with("a volume fired event ");
    });
  };
  CHECK(engine.advance().has_value(), "headless simulation advances");
  CHECK(fired() == 2, "both inside entities fire; first outside body does not mask them");
  CHECK(engine.advance().has_value(), "second headless step");
  CHECK(fired() == 2, "remaining inside does not repeat enter events");
  CHECK(engine.assemble() && engine.advance(), "reassembly resets trigger occupancy");
  CHECK(fired() == 4, "new simulation owns fresh occupancy state");
  auto invalid = scene;
  invalid.Volumes[0].Fires = "missing";
  CHECK(engine.declare(invalid).has_value(), "trigger configuration waits for assembly");
  const Scene *previous = &engine.scene();
  CHECK(!engine.assemble(), "unknown trigger event rejects candidate");
  CHECK(&engine.scene() == previous, "failed trigger assembly preserves simulation owner");
  CHECK(engine.advance().has_value(), "previous simulation remains available");
  CHECK(fired() == 0, "old triggers do not act on a new unassembled declaration");

  scene.Motion.StepS = 0;
  scene.Volumes[0].When = "dwell";
  scene.Volumes[0].DwellS = 0.01;
  CHECK(engine.declare(scene) && engine.assemble(), "dwell scene uses fallback physics step");
  CHECK(engine.advance().has_value(), "first dwell step enters the volume");
  CHECK(fired() == 0, "entry alone is not a dwell event");
  CHECK(engine.advance().has_value(), "second dwell step advances the same simulation clock");
  CHECK(fired() == 2, "both entities dwell after the effective physics step");
  scene.Volumes[0].When = "enter";

  auto field = TriggerField::Stand(scene.Volumes, scene.Events);
  CHECK(field.has_value(), "independent trigger field prepared");
  if (!field) { return Report(); }
  const Entity first{.Index = 7, .Generation = 1};
  const Entity reused{.Index = 7, .Generation = 2};
  field->Probe(first, {}, 0);
  field->Probe(reused, {}, 0);
  const auto events = field->Drain();
  CHECK(events.size() == 2, "reused index with new generation is a distinct entity");
  if (events.size() == 2) {
    CHECK(events[0].Body == first && events[1].Body == reused,
          "trigger events retain complete entity handles");
  }
  return Report();
}
