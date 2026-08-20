/* THE DECLARATIONS IN THE TREE ARE PART OF THE TREE. Every mod under test/unit/scenario/mods is loaded here, so a
 * scenario that stopped being readable is a red test and not a client that exits 1 an hour later —
 * and a property the engine stops reading is caught the moment it stops, because the reader is
 * closed (AnUnreadPropertyIsRefusedByItsPath).
 *
 * It also states what is there: how many scenes, on which stage, so a round that adds or removes one
 * moves a number a reader can see. */
#include "Check.h"
#include "Mod.h"

#include <string>

using namespace outshine;
using namespace outshine::Test;

namespace {

/* The harness runs every test from the repository root, so the declarations are where the clients
 * look for them and no fixture is copied anywhere. */
const char *kRoot = "test/unit/scenario/mods";
const char *kMods[] = {"ardeche", "badwater", "demo", "preikestolen"};

}  // namespace

int main() {
  Covers("I.4 a scenario is a declared world: stage, clock, weather, what runs");

  int scenes = 0, world = 0, studio = 0;
  for (const char *name : kMods) {
    Scenario::Mod mod;
    const bool ok = mod.Load(kRoot, name);
    if (!ok) std::printf("       %s\n", mod.Error().c_str());
    CHECK(ok, "the committed declaration loads");
    if (!ok) continue;
    for (const Scenario::Scene &s : mod.Scenes()) {
      scenes++;
      if (s.Staged().AsWorld()) world++;
      if (s.Staged().AsStudio()) studio++;
      CHECK(!s.Id().empty() && s.FovDeg() > 0.0, "and every scene in it has an id and a lens");
    }
  }
  CHECK(scenes == world + studio, "every scene stands on exactly one stage");
  CHECK(studio > 0, "at least one committed scenario declares a studio with no world");
  Note("declared scenes", scenes, "scenes");
  Note("on a world stage", world, "scenes");
  Note("on a studio stage", studio, "scenes");
  return Report();
}
