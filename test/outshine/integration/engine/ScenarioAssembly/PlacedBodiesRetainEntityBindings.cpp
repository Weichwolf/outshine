#include "SimulationState.h"
#include "Check.h"
#include <type_traits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(!std::is_move_constructible_v<SimulationState> &&
            !std::is_copy_constructible_v<SimulationState>,
        "component registry ownership remains stable for bound columns");
  SimulationState simulation;
  CHECK(simulation.Entities.open(3) && simulation.Bodies.Open(simulation.Entities),
        "component storage opened");
  const auto addedUnrelated = simulation.Entities.addEntity(Role::Tool);
  const auto addedUnplaced = simulation.Entities.addEntity(Role::Body);
  const auto addedPlaced = simulation.Entities.addEntity(Role::Body);
  CHECK(addedUnrelated && addedUnplaced && addedPlaced, "fixture entities are allocated");
  if (!addedUnrelated || !addedUnplaced || !addedPlaced) { return Report(); }
  const Entity unrelated = *addedUnrelated;
  const Entity unplaced = *addedUnplaced;
  const Entity placed = *addedPlaced;
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
  return Report();
}
