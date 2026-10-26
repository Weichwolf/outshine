#include "EntityState.h"
#include "BodySnapshotData.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <vector>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr size_t kCount = 2 * World::kBodiesPerSnapshotPage + 1;
  World::EntityState world;
  CHECK(world.Open(kCount), "the native world opens");
  std::vector<World::EntityBodyState> bodies;
  std::array<Entity, kCount> entities;
  for (size_t i = 0; i < kCount; ++i) {
    const auto entity = world.Registry().addEntity();
    CHECK(entity, "each physical body has a native identity");
    if (!entity) { return Report(); }
    entities[i] = *entity;
    World::EntityBodyState body;
    body.Owner = *entity;
    body.Motion.PositionM[0] = static_cast<double>(i);
    bodies.push_back(body);
  }
  CHECK(world.SetBodies(std::move(bodies)), "the writer accepts native rows");
  const auto held = world.PublishBodies();
  CHECK(held.Count() == kCount && world.PublicationCost().NewPages == 3,
        "initial publication contains three immutable physical pages");
  CHECK(world.PublicationCost().CopiedBytes == kCount * sizeof(World::EntityBodyState),
        "initial copy accounting includes every native body record");
  const auto unchanged = world.PublishBodies();
  CHECK(held.Revision() == unchanged.Revision() && &held.At(0) == &unchanged.At(0) &&
            world.PublicationCost().NewPages == 0 && world.PublicationCost().CopiedBytes == 0,
        "an unchanged world reuses its publication without pose copies");
  CHECK(world.EditBody(entities.front(), [](World::RigidBodyState &) {}),
        "an edit may leave logical state unchanged");
  CHECK(world.PublishBodies().Revision() == held.Revision(),
        "a no-op edit requires no new pose page");
  CHECK(world.EditBody(entities.front(),
                       [](World::RigidBodyState &body) { body.PositionM[0] = 1; }) &&
            world.EditBody(entities.front(),
                           [](World::RigidBodyState &body) { body.PositionM[0] = 0; }),
        "edits may restore the published state");
  CHECK(world.PublishBodies().Revision() == held.Revision() &&
            world.PublicationCost().CopiedBytes == 0,
        "a reverted edit does not produce an unchanged replacement page");
  CHECK(world.EditBody(entities.front(),
                       [](World::RigidBodyState &body) { body.PositionM[0] = -1; }) &&
            world.EditBody(entities.back(),
                           [](World::RigidBodyState &body) { body.PositionM[0] = -2; }),
        "the single native writer changes two different pages");
  CHECK(held.Body(entities.front())->PositionM[0] == 0,
        "an uncommitted edit cannot change a held read view");
  const auto changed = world.PublishBodies();
  CHECK(changed.Body(entities.front())->PositionM[0] == -1 &&
            changed.Body(entities.back())->PositionM[0] == -2 &&
            held.Body(entities.front())->PositionM[0] == 0 &&
            held.Body(entities.back())->PositionM[0] == static_cast<double>(kCount - 1),
        "new and held publications contain their own complete poses");
  CHECK(&held.At(World::kBodiesPerSnapshotPage) == &changed.At(World::kBodiesPerSnapshotPage),
        "the untouched middle page is actually shared");
  CHECK(world.PublicationCost().NewPages == 2 &&
            world.PublicationCost().CopiedBytes ==
                (World::kBodiesPerSnapshotPage + 1) * sizeof(World::EntityBodyState),
        "only the two dirty page payloads are copied");
  CHECK(changed.Revision() == held.Revision() + 1,
        "one body publication advances one body revision");
  return Report();
}
