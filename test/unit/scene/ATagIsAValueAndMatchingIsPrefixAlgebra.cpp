#include <cstdio>

#include "Check.h"

#include "Store.h"

using outshine::Entity;
using outshine::Kind;
using outshine::Relation;
using outshine::Store;
using outshine::Tag;
namespace tags = outshine::tags;

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  static_assert(tags::DoesSteer.Within(tags::Does), "a function is within Does");
  static_assert(!tags::DoesSteer.Within(tags::DoesBrake), "and not within its sibling");
  static_assert(tags::Does.Within(tags::Does), "a tag is within itself");
  static_assert(!Tag{}.Within(tags::Does) && !tags::Does.Within(Tag{}),
                "the empty tag matches nothing in either direction");
  CHECK(true, "the algebra above is constexpr, so a wrong match is a compile error -- these "
              "asserts ran before the binary existed");

  Store scene;
  CHECK(scene.Open(8), "a small store is a store");

  const Entity fourWheel = scene.Add(Kind::Body);
  CHECK(scene.Give(fourWheel, tags::DoesSteer) && scene.Give(fourWheel, tags::DoesDrive) &&
            scene.Give(fourWheel, tags::DoesBrake) && scene.Give(fourWheel, tags::DoesLamp),
        "a prefab body carries the four functions of the actor chain -- steer, drive, brake, "
        "lamps -- as catalogue values, never strings");

  const Entity car = scene.Add(Kind::Body);
  CHECK(scene.Link(car, Relation::IsA, fourWheel),
        "an instance is IsA its prefab, which is one pair");
  CHECK(scene.Has(car, tags::DoesSteer),
        "**WHAT THE PREFAB DOES, THE INSTANCE DOES**: Has walks the IsA chain, so 'glTF as "
        "four-wheel' is a declaration and not a copy");
  CHECK(scene.Has(car, tags::Does),
        "and asking for the parent tag finds any function, which is the prefix algebra doing the "
        "work a list of booleans cannot");
  CHECK(!scene.Has(car, Tag{0x02000000}),
        "a tag from another branch is not found");

  Covers("II.2 a tag is a value from the constexpr catalogue with hierarchical prefix matching, "
         "and capability flows down the IsA chain by query rather than by copy");
  return Report();
}
