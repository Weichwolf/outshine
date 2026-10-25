#include "EntityState.h"
#include "Check.h"
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  World::EntityState world;
  CHECK(world.Open(2), "native identities open");
  const auto first = world.Registry().addEntity(Role::Body);
  const auto second = world.Registry().addEntity(Role::Body);
  CHECK(first && second, "both native body identities exist");
  if (!first || !second) { return Report(); }
  World::EntityBodyState left;
  World::EntityBodyState right;
  left.Owner = *first;
  right.Owner = *second;
  left.Motion.PositionM[0] = -1;
  right.Motion.PositionM[0] = 1;
  CHECK(world.SetBodies({left, right}), "native bodies install");
  const auto held = world.PublishBodies();
  CHECK(!world.SetBodies({left, left}) && world.PublishBodies().Revision() == held.Revision(),
        "a rejected structural change cannot replace the read publication");
  CHECK(world.SetBodies({right, left}), "physical row order may change");
  const auto reordered = world.PublishBodies();
  CHECK(reordered.Body(*first)->PositionM[0] == -1 && held.At(0).Owner == *first &&
            reordered.At(0).Owner == *second,
        "lookup and held storage preserve identity across reorder");
  world.Registry().remove(*first);
  const auto replacement = world.Registry().addEntity(Role::Body);
  CHECK(replacement && *replacement != *first, "a registry slot gets a new generation");
  if (!replacement) { return Report(); }
  const auto removed = world.PublishBodies();
  CHECK(removed.Count() == 1 && removed.Body(*first) == nullptr &&
            removed.Body(*replacement) == nullptr && held.Body(*first) &&
            held.Body(*first)->PositionM[0] == -1,
        "current liveness cannot invalidate a held snapshot or transfer its old pose");
  left.Owner = *replacement;
  left.Motion.PositionM[0] = 2;
  CHECK(world.SetBodies({left, right}), "the replacement receives its own physical state");
  const auto newest = world.PublishBodies();
  CHECK(newest.Body(*replacement) && newest.Body(*replacement)->PositionM[0] == 2 &&
            newest.Body(*first) == nullptr,
        "the new generation owns only its new publication");
  CHECK(world.SetBodies({}), "the writer can clear physical state");
  const auto empty = world.PublishBodies();
  CHECK(empty.Count() == 0 && newest.Count() == 2 && world.PublicationCost().CopiedBytes == 0,
        "empty publication releases the current rows while held versions stay readable");
  CHECK(world.PublishBodies().Revision() == empty.Revision(),
        "empty repeated publication is stable");
  CHECK(world.SetBodies({left, right}),
        "physical rows may be prepared before registry replacement");
  const auto beforeReopen = world.PublishBodies();
  CHECK(world.Registry().open(2), "registry replacement assigns a new identity owner");
  const auto reopened = world.PublishBodies();
  CHECK(reopened.Count() == 0 && beforeReopen.Count() == 2 && beforeReopen.Body(*replacement),
        "registry replacement invalidates current native rows without destroying held history");
  return Report();
}
