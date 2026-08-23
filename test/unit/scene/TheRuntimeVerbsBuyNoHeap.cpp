#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>

#include "Check.h"

#include "Store.h"

using outshine::Entity;
using outshine::Relation;
using outshine::Role;
using outshine::Store;
namespace tags = outshine::tags;

// the whole binary's allocations flow through here -- the counter IS the proof
namespace {
size_t gAllocations = 0;
}

void *operator new(size_t bytes) {
  ++gAllocations;
  void *held = std::malloc(bytes ? bytes : 1);
  if (held == nullptr) { throw std::bad_alloc(); }
  return held;
}
void operator delete(void *held) noexcept { std::free(held); }
void operator delete(void *held, size_t) noexcept { std::free(held); }

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Store scene;
  CHECK(scene.Open(256), "a store opens, reserving its scratch with the pool");

  // a prefab with children, a chain to fell, and an exclusive seat to fight over
  const Entity prefab = scene.Add(Role::Body);
  const Entity wheel = scene.Add(Role::Body);
  CHECK(scene.Link(wheel, Relation::ChildOf, prefab), "the prefab carries a child");
  const Entity mindA = scene.Add(Role::Mind);
  const Entity mindB = scene.Add(Role::Mind);
  const Entity car = scene.Add(Role::Body);
  CHECK(scene.Give(car, tags::Does), "the car does something, as DrivenBy demands");
  CHECK(scene.Link(car, Relation::DrivenBy, mindA), "the first mind takes the seam");

  const size_t before = gAllocations;

  const Entity stood = scene.Instantiate(prefab);
  CHECK(scene.Alive(stood), "a prefab instantiates");
  scene.Remove(stood);
  CHECK(!scene.Alive(stood), "and the instance fells");

  CHECK(!scene.Relink(car, Relation::Uses, mindB),
        "a relink on a many-target relation refuses");
  CHECK(!scene.Error().empty(), "and the composed refusal is readable");

  const size_t spent = gAllocations - before;
  Note("allocations across instantiate, remove and a refused relink", (double)spent,
       "allocations");
  CHECK(spent == 0,
        "**THE RUNTIME VERBS BUY NO HEAP**: despawn, prefab evaluation and a refused "
        "relink run on the scratch the pool opened -- zero allocations, counted at the "
        "global operator new (board:1731)");

  Covers("IV.9 the store's runtime verbs never allocate: member work stacks reserved at "
         "open, composed refusals written into the reserved buffer, proven by a counting "
         "allocator (board:1731)");
  return Report();
}
