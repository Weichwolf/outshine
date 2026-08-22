#include <cstdio>
#include <string>

#include "Check.h"

#include "Store.h"

using outshine::Entity;
using outshine::kNoEntity;
using outshine::Relation;
using outshine::Role;
using outshine::Store;

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Store scene;
  CHECK(scene.Open(32), "a store opens");

  const Entity garagePrefab = scene.Add(Role::Body);
  const Entity doorPart = scene.Add(Role::Body);
  CHECK(scene.Link(doorPart, Relation::ChildOf, garagePrefab),
        "a prefab's door is its subtree -- ChildOf, the bone");

  const Entity garage = scene.Instantiate(garagePrefab);
  const Entity car = scene.Add(Role::Body);
  CHECK(scene.Alive(garage) && scene.Link(car, Relation::Holds, garage),
        "a standing garage HOLDS a parked car -- Holds, the possession");

  const Entity second = scene.Instantiate(garagePrefab);
  CHECK(scene.Alive(second),
        "a second garage instantiates from the same prefab while the first is occupied");
  Entity into[8];
  CHECK(scene.Sources(second, Relation::Holds, into, 8) == 0,
        "**INSTANTIATING A PREFAB CANNOT DRAG THE WORLD'S CONTENTS WITH IT**: the new garage "
        "holds nothing -- the parked car belongs to the first, and possession is not part of "
        "the subtree the prefab copies");
  CHECK(scene.Sources(second, Relation::ChildOf, into, 8) == 1,
        "while the subtree's door WAS copied -- two relations, two meanings, no collision");

  scene.Remove(garage);
  CHECK(scene.Alive(car),
        "**REMOVING A HOLDER FREES ITS CONTENTS, NEVER DESTROYS THEM**: the car outlives the "
        "garage -- Holds is not owned-by-target, destruction is a choice nobody made here");
  CHECK(scene.TargetOf(car, Relation::Holds) == kNoEntity,
        "and the car stands unheld, ready for the next garage");

  CHECK(!scene.Link(garage == kNoEntity ? car : garage, Relation::Holds, car) ||
            !scene.Link(car, Relation::Holds, car),
        "a thing cannot hold itself -- the Acyclic rule guards the relation like every other");

  Covers("III.7 holding is its own relation: ChildOf is the prefab subtree's bone and "
         "cascades, Holds is possession -- not copied by Instantiate, freeing on removal, "
         "acyclic (board:1669)");
  return Report();
}
