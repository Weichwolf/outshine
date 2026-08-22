#include <cstdio>

#include "Check.h"

#include "Store.h"

using outshine::Entity;
using outshine::kNoEntity;
using outshine::Relation;
using outshine::Role;
using outshine::Store;
using outshine::Tag;
namespace tags = outshine::tags;

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Store scene;
  CHECK(scene.Open(32), "a store opens");

  const Entity fourWheel = scene.Add(Role::Body);
  CHECK(scene.Give(fourWheel, tags::DoesSteer) && scene.Give(fourWheel, tags::DoesDrive) &&
            scene.Give(fourWheel, tags::DoesBrake),
        "the prefab body carries its functions");
  const Entity seat = scene.Add(Role::Body);
  const Entity lamp = scene.Add(Role::Body);
  CHECK(scene.Give(lamp, tags::DoesLamp), "the lamp child is what lights");
  CHECK(scene.Link(seat, Relation::ChildOf, fourWheel) &&
            scene.Link(lamp, Relation::ChildOf, fourWheel),
        "a prefab is a subtree: a seat and a lamp under the body");

  const Entity car = scene.Instantiate(fourWheel);
  CHECK(scene.Alive(car) && scene.TargetOf(car, Relation::IsA) == fourWheel,
        "**AN INSTANCE IS ONE CALL**: 'glTF as four-wheel' adds an entity IsA the prefab");
  CHECK(scene.Has(car, tags::DoesSteer) && scene.Has(car, tags::DoesBrake),
        "what the prefab does, the instance does -- by query, never by copy");

  const Entity carSeat = scene.CopyOf(car, seat);
  const Entity carLamp = scene.CopyOf(car, lamp);
  CHECK(scene.Alive(carSeat) && scene.Alive(carLamp) && !(carSeat == seat) && !(carLamp == lamp),
        "**THE SUBTREE COMES WITH IT AND THE COPIES ARE ITS OWN**: the instance's seat and lamp "
        "stand as distinct entities under the instance, found through the slot the prefab named "
        "-- no string lookup, the pair (IsA, prefab-child) IS the name");
  CHECK(scene.TargetOf(carSeat, Relation::ChildOf) == car &&
            scene.TargetOf(carLamp, Relation::ChildOf) == car,
        "and they hang under the instance, not under the prefab");
  CHECK(scene.Has(carLamp, tags::DoesLamp),
        "a copied child inherits its prefab child's capability through the same IsA query");

  const Entity second = scene.Instantiate(fourWheel);
  CHECK(scene.Alive(second) && !(scene.CopyOf(second, seat) == carSeat),
        "a second instance owns a second seat -- instances share the prefab and nothing else");

  scene.Remove(car);
  CHECK(scene.Alive(fourWheel) && scene.Alive(seat) && scene.Alive(second),
        "**REMOVING AN INSTANCE TOUCHES NO PREFAB AND NO SIBLING**: the prefab is shared "
        "structure, the instances are owned structure");
  CHECK(!scene.Alive(carSeat) && !scene.Alive(carLamp),
        "**AND ITS SUBTREE DIES WITH IT**: ChildOf is owned-by-target by trait, so the copies "
        "cascade rather than orphan -- the rule lives on the relation, not in a destructor "
        "somebody remembered");

  CHECK(scene.Instantiate(kNoEntity) == kNoEntity && !scene.Error().empty(),
        "instantiating what does not stand is a loud refusal");

  Covers("II.6 a prefab instantiates as one call: the instance answers the prefab's "
         "capabilities by query, its subtree is copied with (IsA, prefab-child) as the slot "
         "name, and instances share nothing but the prefab");
  return Report();
}
