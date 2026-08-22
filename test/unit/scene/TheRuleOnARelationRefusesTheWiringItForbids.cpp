#include <cstdio>
#include <string>

#include "Check.h"

#include "Store.h"

using outshine::Entity;
using outshine::kNoEntity;
using outshine::Kind;
using outshine::Relation;
using outshine::Store;
namespace tags = outshine::tags;

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Store scene;
  CHECK(scene.Open(64), "a store opens once, sized once, and grows never");

  const Entity car = scene.Add(Kind::Body);
  const Entity mind = scene.Add(Kind::Mind);
  const Entity second = scene.Add(Kind::Mind);
  const Entity nav = scene.Add(Kind::Tool);
  const Entity route = scene.Add(Kind::Assignment);
  CHECK(scene.Alive(car) && scene.Alive(mind) && scene.Alive(nav) && scene.Alive(route),
        "what was added stands");

  CHECK(!scene.Link(car, Relation::DrivenBy, mind),
        "**AN INERT BODY TAKES NO DRIVER.** DrivenBy asks its source to DO something -- the rule "
        "carries the tag -- and this car was given no function yet, so the wiring is refused at "
        "assembly rather than discovered dead at runtime");
  std::printf("NOTE the refusal says: %s\n", scene.Error().c_str());

  CHECK(scene.Give(car, tags::DoesSteer) && scene.Give(car, tags::DoesDrive) &&
            scene.Give(car, tags::DoesBrake),
        "functions are tags from the catalogue, given one by one");
  CHECK(scene.Link(car, Relation::DrivenBy, mind),
        "and a body that steers, drives and brakes takes its driver");

  CHECK(!scene.Link(car, Relation::DrivenBy, second),
        "**DRIVENBY IS EXCLUSIVE**: the second driver is refused by the trait on the relation, "
        "which is one declaration -- not a check some call site remembered");
  CHECK(!scene.Link(car, Relation::Uses, mind),
        "**THE TARGET KIND IS PART OF THE RULE**: Uses reaches a tool, and a mind is not one");
  CHECK(!scene.Link(mind, Relation::Assigned, route),
        "**AN ASSIGNMENT STANDS ONLY ON A WAY TO NAVIGATE**: Assigned requires Uses, and this "
        "mind holds no tool yet -- refuse on existence, degrade on detail");
  CHECK(scene.Link(mind, Relation::Uses, nav) && scene.Link(mind, Relation::Assigned, route),
        "give the mind its navigation and the assignment is taken");

  const Entity a = scene.Add(Kind::Body);
  const Entity b = scene.Add(Kind::Body);
  const Entity c = scene.Add(Kind::Body);
  CHECK(scene.Link(a, Relation::ChildOf, b) && scene.Link(b, Relation::ChildOf, c),
        "a hierarchy is pairs of the same relation");
  CHECK(!scene.Link(c, Relation::ChildOf, a),
        "**AND IT MAY NOT CLOSE A LOOP**: ChildOf is acyclic by trait, so the cycle is refused "
        "with the walk bounded by the pool");
  CHECK(!scene.Error().empty() && scene.Error().find("ChildOf") != std::string::npos,
        "every refusal names the relation whose rule spoke, because the same text must come out "
        "of the XML door and the C++ door");

  scene.Remove(mind);
  CHECK(!scene.Alive(mind), "what was removed stands no more");
  CHECK(scene.TargetOf(car, Relation::DrivenBy) == kNoEntity,
        "**A DEAD TARGET IS NO TARGET**: the pair to the removed mind answers nothing, because "
        "the generation on the handle died with the slot");
  CHECK(scene.Link(car, Relation::DrivenBy, second),
        "and the exclusive seat is free again -- the player takes the wheel at the same seam");

  Covers("II.1 the rule lives on the relation: exclusive, acyclic, target kind, required "
         "capability and required companion are declared once in the constexpr catalogue, and an "
         "illegal wiring is refused at assembly with a text that names the rule");
  return Report();
}
