#include <cstdio>
#include <string>

#include "Check.h"

#include "Store.h"

using outshine::Entity;
using outshine::kNoEntity;
using outshine::Relation;
using outshine::Role;
using outshine::Store;
namespace tags = outshine::tags;

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Store scene;
  CHECK(scene.Open(64), "a store opens");

  const Entity car = scene.Add(Role::Body);
  const Entity autopilot = scene.Add(Role::Mind);
  const Entity player = scene.Add(Role::Mind);
  const Entity crate = scene.Add(Role::Body);
  CHECK(scene.Give(car, tags::DoesSteer) && scene.Give(car, tags::DoesDrive) &&
            scene.Give(car, tags::DoesBrake),
        "the car declares its functions");

  CHECK(!scene.Relink(car, Relation::DrivenBy, autopilot),
        "**AN EMPTY SEAT IS NOT RELINKED, IT IS LINKED** -- relink demands something held");
  CHECK(scene.Link(car, Relation::DrivenBy, autopilot), "the autopilot takes the seat");
  CHECK(!scene.Link(car, Relation::DrivenBy, player),
        "a second Link refuses -- the seat is exclusive");

  CHECK(scene.Relink(car, Relation::DrivenBy, player),
        "**TAKING THE WHEEL IS ONE RELINK.** The player and the autopilot take the same seam, "
        "and possession moves as one atomic retarget -- no unlink-then-link window in which "
        "the car is driverless");
  CHECK(scene.TargetOf(car, Relation::DrivenBy) == player, "the seat says who holds it");

  Entity driven[4];
  CHECK(scene.Sources(autopilot, Relation::DrivenBy, driven, 4) == 0,
        "and the autopilot's reverse index no longer lists the car -- the indices moved with "
        "the retarget");
  CHECK(scene.Sources(player, Relation::DrivenBy, driven, 4) == 1 && driven[0] == car,
        "the player's lists it");

  CHECK(scene.Relink(car, Relation::DrivenBy, player),
        "relinking to the same holder is a quiet yes, not a refusal");
  CHECK(!scene.Relink(car, Relation::DrivenBy, crate),
        "**THE RULES TRAVEL WITH THE VERB**: a crate is no Mind, and the same checker that "
        "guards Link refuses the retarget");
  CHECK(scene.TargetOf(car, Relation::DrivenBy) == player,
        "and a refused relink leaves the seat exactly as it was");
  CHECK(!scene.Relink(car, Relation::Uses, player),
        "relink is the exclusive relation's verb -- a many-target relation refuses it");

  const Entity tool = scene.Add(Role::Tool);
  const Entity routeA = scene.Add(Role::Assignment);
  const Entity routeB = scene.Add(Role::Assignment);
  CHECK(scene.Link(player, Relation::Uses, tool), "the mind takes a tool");
  CHECK(scene.Link(player, Relation::Assigned, routeA), "an assignment stands on Uses");
  scene.Remove(tool);
  CHECK(!scene.Link(autopilot, Relation::Assigned, routeB),
        "**EVERY RULE TRAVELS THROUGH BOTH VERBS**: a mind without a tool fails Assigned's "
        "Requires at Link");
  const std::string linkWhy = scene.Error();
  CHECK(!scene.Relink(player, Relation::Assigned, routeB),
        "and refuses the Relink of the held assignment just the same -- two verbs, one truth "
        "about one rule (board:1646)");
  CHECK(scene.Error() == linkWhy, "with the SAME refusal text: one checker guards both doors");
  CHECK(scene.TargetOf(player, Relation::Assigned) == routeA,
        "and the refused retarget leaves the held assignment standing");

  Covers("III.4 possession is the DrivenBy relation and taking the wheel is one relink: an "
         "atomic, rule-checked retarget of the exclusive seat, with the reverse indices moving "
         "in the same step -- the player and the autopilot take the same seam");
  return Report();
}
