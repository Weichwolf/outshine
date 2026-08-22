#include <cstdio>

#include "Check.h"

#include "Column.h"

using outshine::Column;
using outshine::Entity;
using outshine::Role;
using outshine::Store;

namespace {
struct Payload {
  double MassKg = 0.0;
};
} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Store scene;
  CHECK(scene.Open(4), "a store opens");
  Column<Payload> weights;
  CHECK(weights.Open(4), "a column opens to the same capacity, sized once");

  const Entity car = scene.Add(Role::Body);
  CHECK(weights.Get(car, scene) == nullptr, "what was never put is not there");
  CHECK(weights.Put(car, scene, Payload{1610.0}), "data is put against a living handle");
  CHECK(weights.Get(car, scene) != nullptr && weights.Get(car, scene)->MassKg == 1610.0,
        "and read back through it");

  const Entity stale = car;
  scene.Remove(car);
  CHECK(weights.Get(stale, scene) == nullptr,
        "**A DEAD HANDLE READS NOTHING**: the entity died, so its data is unreachable even "
        "though the bytes still sit in the column");

  const Entity next = scene.Add(Role::Body);
  CHECK(next.Index == stale.Index && next.Generation != stale.Generation,
        "the pool reuses the slot under a new generation");
  CHECK(weights.Get(next, scene) == nullptr,
        "**AND THE NEW TENANT DOES NOT INHERIT THE OLD ONE'S DATA**: the column checks the "
        "generation it stored against the handle that asks, which is what makes a handle a "
        "capability and not an index");

  CHECK(!weights.Put(stale, scene, Payload{1.0}),
        "a dead handle cannot write either");

  Covers("II.5 a data column is keyed by the generation-checked handle: dead handles read and "
         "write nothing, and a reused slot starts empty");
  return Report();
}
