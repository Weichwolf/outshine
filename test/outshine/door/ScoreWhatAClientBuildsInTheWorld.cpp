#include <cstdio>

#include <Outshine.h>

#include "Check.h"

// THE WORLD IS REACHABLE THROUGH THE DOOR, AND ITS RULES REFUSE THROUGH THE DOOR TOO.
//
// Both benchmarks publish the world and keep the renderer's scene private: Unreal gives a client
// UWorld and SpawnActor while FScene stays inside; RAGE gives the map and keeps the draw list.
// This tree had the mesh value public and the world not, so a client could hand geometry in and
// then had no way to say what stood where or what was linked to what.
//
// This case reaches `Store` through `include/` alone -- no `src/` path is on its include line --
// and does the four things a world is for: make an entity, mark what it can, link two of them,
// and read the link back. Then it asks the store to break its own rule, because a graph that
// accepts anything is a dictionary rather than a world.
//
// THE NEGATIVE CONTROL IS THE COMPILER. `Store` publishes verbs and an opaque `Kept`, so a case
// cannot name a slot, a pair or a seat: the layout is not merely private, it is ABSENT from the
// translation unit. The commented line below is the control -- restoring it does not fail this
// case, it fails the BUILD, which is the strongest refusal a door can give.
//
//   const size_t slots = store.Slots_.size();   // does not compile: Store has no such member

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  outshine::Engine engine;
  engine.setRoots(outshine::Roots{"apps/driver/src", "src/assets", "/tmp/outshine-door-cache", true});

  outshine::Scenario declared;
  declared.Bodies.push_back(outshine::Body{});
  declared.Bodies.back().Name = "carrier";
  declared.Room = 8;

  const bool declaredStands = engine.declare(declared) && engine.assemble();
  CHECK(declaredStands,
        ("**A WORLD STANDS WITH NO DEVICE**: this case never asks for a canvas, and Unreal's "
         "LoadMap builds a UWorld the same way -- which is what every commandlet and dedicated "
         "server is: " + engine.error()).c_str());
  if (!declaredStands) { return Report(); }

  outshine::Store &world = engine.scene();

  const outshine::Entity body = world.Add(outshine::Role::Body);
  const outshine::Entity mind = world.Add(outshine::Role::Mind);
  const bool made = world.Alive(body) && world.Alive(mind);
  CHECK(made, "**A CLIENT MAKES ENTITIES THROUGH THE DOOR**: `Engine::Scene()` hands back the "
              "world, and the world hands back handles -- which is Unreal's UWorld and RAGE's "
              "entity store reached the way both engines reach them");

  const bool tagged = world.Give(body, outshine::TagCatalogue::Under(outshine::tags::Does, 1));
  CHECK(tagged, "and marks what one CAN do, from a catalogue a typo cannot enter");

  const bool linked = world.Link(body, outshine::Relation::DrivenBy, mind);
  CHECK(linked, ("and links a body to the mind that drives it: " +
                 std::string(world.Error())).c_str());

  const outshine::Entity found = world.TargetOf(body, outshine::Relation::DrivenBy);
  CHECK(found == mind, "and reads the link back -- so the graph answers, rather than only "
                       "accepting");

  const outshine::Entity tool = world.Add(outshine::Role::Tool);
  const bool refused = !world.Link(body, outshine::Relation::DrivenBy, tool);
  std::printf("A TOOL DRIVING A BODY  refused: %s   %s\n", refused ? "yes" : "NO",
              std::string(world.Error()).c_str());
  CHECK(refused, "**AND THE WORLD REFUSES WHAT ITS RULES FORBID**: DrivenBy targets a MIND, and a "
                 "graph that accepts anything is a dictionary rather than a world. The rule is "
                 "`constexpr` beside the relation, so it cannot drift from what the store does");

  Covers("the door: a client reaches the WORLD through `Engine::Scene()` and nothing of `src/` -- "
         "making entities, marking what they can, linking them and reading the link back -- and "
         "the store refuses a link its own rules forbid, while its slots and pairs are absent "
         "from the client's translation unit rather than merely private");
  return Report();
}
