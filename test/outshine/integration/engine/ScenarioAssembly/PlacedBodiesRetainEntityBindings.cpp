#include "ScenarioRuntime.h"
#include "Check.h"
#include <type_traits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(!std::is_move_constructible_v<ScenarioRuntime> &&
            !std::is_copy_constructible_v<ScenarioRuntime>,
        "component registry ownership remains stable for bound columns");
  ScenarioRuntime runtime;
  CHECK(runtime.Entities.Open(3) && runtime.Definitions.Open(runtime.Entities.Registry()),
        "component storage opened");
  const auto addedUnrelated = runtime.Entities.Registry().addEntity();
  const auto addedUnplaced = runtime.Entities.Registry().addEntity();
  const auto addedPlaced = runtime.Entities.Registry().addEntity();
  CHECK(addedUnrelated && addedUnplaced && addedPlaced, "fixture entities are allocated");
  if (!addedUnrelated || !addedUnplaced || !addedPlaced) { return Report(); }
  const Entity unrelated = *addedUnrelated;
  const Entity unplaced = *addedUnplaced;
  const Entity placed = *addedPlaced;
  Scenario::Body definition;
  definition.Name = "template";
  CHECK(runtime.Definitions.Put(unplaced, definition), "unplaced body registered");
  definition.Name = "moving";
  definition.Placed = true;
  definition.Stands.AtM[0] = 7;
  definition.MassKg = 3;
  CHECK(runtime.Definitions.Put(placed, definition), "placed body registered");
  runtime.Stood.Bodies = {unplaced, placed};
  CHECK(runtime.PrepareBodies(), "native physical rows publish");
  CHECK(runtime.Entities.Bodies().size() == 1, "only placed bodies receive physical state");
  if (runtime.Entities.Bodies().size() != 1) { return Report(); }
  const auto &body = runtime.Entities.Bodies().front();
  CHECK(body.Owner == placed && body.Owner != unplaced && body.Owner != unrelated,
        "filtered physical rows retain the original entity handle");
  CHECK(body.Motion.PositionM[0] == 7 && body.Motion.MassKg == 3,
        "physical state comes from the owning entity's component");
  CHECK(runtime.Definitions.Get(body.Owner)->Name == "moving",
        "entity binding resolves back to its component");
  return Report();
}
