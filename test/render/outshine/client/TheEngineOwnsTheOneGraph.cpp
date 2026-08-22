#include <cstdio>

#include <outshine/Outshine.h>

#include "Check.h"

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  outshine::Engine engine;
  CHECK(engine.Read("tools/driver/f31.scenario"),
        "the driver's declaration reads through the one door");
  CHECK(engine.Assemble(),
        "**AND IT ASSEMBLES THROUGH THE SAME DOOR**: Engine::Assemble walks the declaration "
        "through the store API -- the calls the XML reader and a C++ client share");

  outshine::Store &scene = engine.Scene();
  outshine::Entity bodies[2], minds[2];
  CHECK(scene.Cast(outshine::Role::Body, bodies, 2) == 1,
        "the declaration's one vehicle is one body in the graph");
  CHECK(scene.Cast(outshine::Role::Mind, minds, 2) == 1,
        "and its player is one mind");
  CHECK(scene.TargetOf(bodies[0], outshine::Relation::DrivenBy) == minds[0],
        "possessing the seat is the DrivenBy relation, not an API");
  CHECK(scene.Has(bodies[0], outshine::tags::DoesSteer) &&
            scene.Has(bodies[0], outshine::tags::DoesBrake),
        "the body's functions answer through the handle");

  CHECK(scene.Add(outshine::Role::Mind) == outshine::kNoEntity,
        "**THE POOL IS THE DECLARATION'S SIZE**: the store opened for exactly what the file "
        "names, so a stray actor is refused room rather than granted it silently");
  CHECK(!scene.Link(bodies[0], outshine::Relation::DrivenBy, minds[0]),
        "**THE RULES TRAVEL WITH THE DOOR**: the seat is exclusive from outside the library "
        "exactly as it is from inside, because there is one checker");

  CHECK(scene.Error().find("exclusive") != std::string::npos,
        "and the refusal names the trait");

  Covers("I.18 the assembly API has its public entrance: a client that includes nothing but "
         "outshine/ reads, assembles, queries and is refused by the same store the XML door "
         "calls -- one graph, two doors, one checker");
  return Report();
}
