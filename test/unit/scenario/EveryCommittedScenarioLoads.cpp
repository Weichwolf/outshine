#include "Check.h"
#include "Mod.h"

#include <string>

using namespace outshine;
using namespace outshine::Test;

namespace {

const char *kRoot = "test/unit/scenario/mods";
const char *kMods[] = {"ardeche", "badwater", "demo", "preikestolen"};

}

int main() {
  Covers("I.4 a scenario is a declared world: stage, clock, weather, what runs");

  int scenes = 0, world = 0, studio = 0;
  for (const char *name : kMods) {
    SceneLegacy::Mod mod;
    const bool ok = mod.Load(kRoot, name);
    if (!ok) std::printf("       %s\n", mod.Error().c_str());
    CHECK(ok, "the committed declaration loads");
    if (!ok) continue;
    for (const SceneLegacy::Scene &s : mod.Scenes()) {
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
