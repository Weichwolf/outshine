#include "EntityRegistry.h"
#include "Entity.h"
#include "Check.h"
#include <cstdint>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  World::EntityRegistry registry;
  CHECK(registry.LivenessRevision() == 0 && registry.open(2), "first open creates an owner epoch");
  const auto opened = registry.LivenessRevision();
  const auto first = registry.addEntity();
  const auto second = registry.addEntity();
  CHECK(first && second && *first != *second && registry.LivenessRevision() == opened + 2,
        "successful creations have distinct identities and advance liveness");
  if (!first || !second) { return Report(); }
  registry.remove(*first);
  const auto removed = registry.LivenessRevision();
  CHECK(!registry.alive(*first) && registry.alive(*second),
        "removal invalidates only its exact process handle");
  registry.remove(*first);
  registry.remove(kNoEntity);
  CHECK(registry.LivenessRevision() == removed, "stale removals do not change liveness");
  const auto replacement = registry.addEntity();
  CHECK(replacement && replacement->Index == first->Index &&
            replacement->Generation == first->Generation + 1 &&
            replacement->Owner == first->Owner && !registry.alive(*first),
        "slot reuse changes the generation while preserving its process owner");
  World::EntityRegistry foreign;
  CHECK(foreign.open(2), "another process-local registry opens");
  const auto outsider = foreign.addEntity();
  CHECK(outsider && outsider->Owner != first->Owner && !registry.alive(*outsider),
        "matching slot numbers from another owner cannot alias local identities");
  const auto beforeForeign = registry.LivenessRevision();
  if (outsider) { registry.remove(*outsider); }
  CHECK(registry.LivenessRevision() == beforeForeign && registry.alive(*second),
        "a foreign removal cannot change storage or liveness");
  CHECK(registry.open(2) && !registry.alive(*second) &&
            (!replacement || !registry.alive(*replacement)),
        "reopen changes the epoch and invalidates every previously issued local handle");
  const auto reopened = registry.addEntity();
  CHECK(reopened && reopened->Index == first->Index && reopened->Generation == first->Generation &&
            reopened->Owner != first->Owner && !registry.alive(*first),
        "reopen cannot alias an old handle even when index and generation repeat");
  return Report();
}
