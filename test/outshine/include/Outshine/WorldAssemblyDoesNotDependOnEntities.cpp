#include <Outshine.h>
#include <SDL3/SDL.h>
#include "Check.h"
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Engine engine;
    CHECK(engine.setRoots({.Shipped = "src/assets", .Offline = true}).has_value(),
          "roots are accepted before declaration");
    CHECK(engine.drawsInto(Extent{64, 64}).has_value(), "offscreen target configured");
    Scenario::Document initial;
    initial.Room = 4;
    Scenario::Kind kind;
    kind.Name = "actor";
    initial.Kinds.push_back(kind);
    initial.Instances.push_back({.Of = "actor", .Id = "kept"});
    const auto ready = engine.declare(initial) && engine.assemble();
    CHECK(ready, "initial simulation assembled");
    if (!ready) { return Report(); }
    EntityRegistry *const previous = &engine.entities();
    const auto changedRoots = engine.setRoots({.Shipped = "wrong", .Offline = false});
    CHECK(!changedRoots && changedRoots.error().find("roots") != std::string::npos,
          "roots reject a change after declaration");
    CHECK(&engine.entities() == previous, "rejected roots preserve the assembled simulation");
    const auto marker = previous->addEntity(Role::Tool);
    CHECK(marker.has_value(), "spare entity capacity remains usable");
    if (!marker) { return Report(); }
    for (const bool populated : {true, false}) {
      Scenario::Document world = populated ? initial : Scenario::Document{};
      world.Ground.Declared = true;
      world.Ground.VegetationEnabled = false;
      world.Ground.Origin.LatitudeDeg = 47;
      world.Ground.Origin.LongitudeDeg = 8;
      world.Providers.push_back({.Kind = "unavailable"});
      CHECK(engine.declare(world).has_value(), "ground declaration accepted before assembly");
      const auto built = engine.assemble();
      CHECK(!built, "invalid world source fails independently of entity capacity");
      CHECK(!built && built.error().find("unavailable") != std::string::npos,
            "world source failure reaches the caller");
      CHECK(&engine.entities() == previous, "failed world setup preserves simulation ownership");
      if (&engine.entities() != previous) { return Report(); }
      CHECK(previous->alive(*marker), "previous simulation remains usable");
    }
  }
  SDL_Quit();
  return Report();
}
