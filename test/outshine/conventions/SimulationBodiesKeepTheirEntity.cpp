#include "SimulationState.h"
#include "Check.h"
#include <type_traits>

static_assert(!std::is_move_constructible_v<outshine::SimulationState>);
static_assert(!std::is_copy_constructible_v<outshine::SimulationState>);

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  SimulationState simulation;
  CHECK(simulation.Scene.open(3) && simulation.Bodies.Open(simulation.Scene),
        "component storage opened");
  const Entity unrelated = simulation.Scene.addEntity(Role::Tool);
  const Entity unplaced = simulation.Scene.addEntity(Role::Body);
  const Entity placed = simulation.Scene.addEntity(Role::Body);
  Scenario::Body definition;
  definition.Name = "template";
  CHECK(simulation.Bodies.Put(unplaced, definition), "unplaced body registered");
  definition.Name = "moving";
  definition.Placed = true;
  definition.Stands.AtM[0] = 7;
  definition.MassKg = 3;
  CHECK(simulation.Bodies.Put(placed, definition), "placed body registered");
  simulation.Stood.Bodies = {unplaced, placed};
  simulation.PrepareBodies();
  CHECK(simulation.DynamicBodies.size() == 1, "only placed bodies receive physical state");
  if (simulation.DynamicBodies.size() != 1) { return Report(); }
  const auto &body = simulation.DynamicBodies.front();
  CHECK(body.Owner == placed && body.Owner != unplaced && body.Owner != unrelated,
        "filtered physical rows retain the original entity handle");
  CHECK(body.Motion.PositionM[0] == 7 && body.Motion.MassKg == 3,
        "physical state comes from the owning entity's component");
  CHECK(simulation.Bodies.Get(body.Owner)->Name == "moving",
        "entity binding resolves back to its component");
  simulation.Integrate(0.5, {{0.0, -10.0, 0.0}});
  CHECK(body.Owner == placed, "integration preserves entity identity");
  CHECK(body.Motion.VelocityMs[1] == -5 && body.Motion.PositionM[1] == -2.5,
        "half-second semi-implicit gravity step has the analytical velocity and displacement");
  return Report();
}
