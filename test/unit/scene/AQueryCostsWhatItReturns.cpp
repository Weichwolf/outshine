#include <cstdio>

#include "Check.h"

#include "Column.h"
#include "Store.h"

using outshine::Column;
using outshine::Entity;
using outshine::Relation;
using outshine::Role;
using outshine::Store;
namespace tags = outshine::tags;

namespace {
struct Mass {
  double Kg = 0.0;
};
} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Store scene;
  CHECK(scene.Open(64), "a store of sixty-four");

  for (int filler = 0; filler < 40; ++filler) { (void)scene.Add(Role::Body); }

  const Entity prefab = scene.Add(Role::Body);
  Entity children[3];
  for (int child = 0; child < 3; ++child) {
    children[child] = scene.Add(Role::Body);
    CHECK(scene.Link(children[child], Relation::ChildOf, prefab), "a child hangs under the prefab");
  }
  const Entity pumpA = scene.Add(Role::Body);
  const Entity pumpB = scene.Add(Role::Body);
  CHECK(scene.Offer(pumpA, tags::OffersRefuel, 1) && scene.Offer(pumpB, tags::OffersRefuel, 1),
        "two offers stand among sixty-four entities");
  const Entity mind = scene.Add(Role::Mind);
  (void)mind;

  scene.ResetTouched();
  Entity offering[4];
  const size_t offers = scene.Offering(tags::Offers, offering, 4);
  Note("offers found", (double)offers, "entities");
  Note("slots touched finding them", (double)scene.Touched(), "touches");
  CHECK(offers == 2 && scene.Touched() == 2,
        "**OFFERING WALKS THE OFFER LIST AND NOTHING ELSE**: two adverts among sixty-four "
        "entities cost exactly two touches, because advertising links the entity into a set "
        "the query reads -- the pool is never searched (board:1591)");

  scene.ResetTouched();
  const size_t minds = scene.Cast(Role::Mind, offering, 4);
  CHECK(minds == 1 && scene.Touched() == 1,
        "**A ROLE IS A SET**: one mind among sixty-four costs one touch");

  const Entity instance = scene.Instantiate(prefab);
  CHECK(scene.Alive(instance), "the prefab instantiates");
  scene.ResetTouched();
  const Entity copy = scene.CopyOf(instance, children[0]);
  Note("touches resolving the slot", (double)scene.Touched(), "touches");
  CHECK(scene.Alive(copy) && scene.Touched() <= 3 * 4,
        "**A SLOT NAME COSTS ITS SIBLINGS, NOT THE POOL**: CopyOf reads the instance's own "
        "children through the reverse index -- three of them, each with a handful of pair "
        "touches -- where the review measured a sixty-four-slot walk");

  scene.ResetTouched();
  scene.Remove(instance);
  Note("touches removing the subtree", (double)scene.Touched(), "touches");
  CHECK(!scene.Alive(copy) && scene.Touched() <= 4 * (3 + 2),
        "**THE CASCADE READS THE INDEX TOO**: removing the instance touches its own subtree's "
        "edges, never the sixty other entities");

  Entity sources[4];
  scene.ResetTouched();
  const size_t under = scene.Sources(prefab, Relation::ChildOf, sources, 4);
  CHECK(under == 3 && scene.Touched() == 3,
        "Sources answers the reverse index directly: three children, three touches");

  Column<Mass> masses;
  CHECK(masses.Open(scene, 64), "a column binds to its store once");
  CHECK(masses.Put(children[0], Mass{1.0}) && masses.Put(children[2], Mass{3.0}),
        "two masses ride on two children");
  double summed = 0.0;
  size_t rows = 0;
  masses.Each([&](Entity of, const Mass &mass) {
    (void)of;
    summed += mass.Kg;
    ++rows;
  });
  CHECK(rows == 2 && summed == 4.0,
        "**A SYSTEM'S PASS IS LINEAR**: Each visits the held rows in index order -- a physics "
        "step over every mass is one contiguous walk, not a lookup per name");

  Covers("II.8 a query costs what it returns: offers, roles, children and cascades read "
         "intrusive sets kept at link time, and a column iterates beside its one store");
  return Report();
}
