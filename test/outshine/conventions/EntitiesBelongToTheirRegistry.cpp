#include <scene/Scene.h>
#include "Column.h"
#include "Check.h"
#include <type_traits>

static_assert(!std::is_move_constructible_v<outshine::Scene>);
static_assert(!std::is_move_assignable_v<outshine::Scene>);

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scene first;
  Scene second;
  CHECK(first.open(2) && second.open(2), "independent registries open");
  const Entity original = first.addEntity(Role::Body);
  const Entity foreign = second.addEntity(Role::Body);
  CHECK(original.Index == foreign.Index && original.Generation == foreign.Generation,
        "fixture collides in local slot and generation");
  CHECK(original != foreign && !first.alive(foreign) && !second.alive(original),
        "registry ownership distinguishes identical local indices and generations");
  Column<int> values;
  CHECK(values.Open(first) && values.Put(original, 42), "owned component is stored");
  CHECK(!values.Put(foreign, 99), "foreign entity cannot overwrite a component");
  values.Drop(foreign);
  CHECK(values.Get(original) && *values.Get(original) == 42,
        "foreign drop preserves the local component");
  first.remove(foreign);
  CHECK(first.alive(original), "foreign removal cannot destroy the local entity");
  CHECK(first.open(2), "registry opens a fresh epoch");
  const Entity replacement = first.addEntity(Role::Body);
  CHECK(replacement != original && !first.alive(original), "reopening invalidates old handles");
  CHECK(values.Get(replacement) == nullptr, "new entity cannot inherit a stale component");
  int count = 0;
  values.Each([&](Entity, int) { ++count; });
  CHECK(count == 0, "iteration never republishes components from a previous epoch");
  CHECK(values.Put(replacement, 7), "current entity can receive a new component");
  values.Drop(original);
  CHECK(values.Get(replacement) && *values.Get(replacement) == 7,
        "old epoch cannot erase a replacement component");
  return Report();
}
