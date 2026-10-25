#include "EntityRegistry.h"
#include "Check.h"
#include <cstddef>
#include <cstdint>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  World::EntityRegistry registry;
  CHECK(!registry.addEntity() && !registry.alive(kNoEntity) && registry.capacity() == 0,
        "an unopened registry has no identities or implicit capacity");
  CHECK(registry.open(2), "a bounded registry opens");
  const auto first = registry.addEntity();
  CHECK(first.has_value(), "the first process identity is available");
  if (!first) { return Report(); }
  const uint64_t revision = registry.LivenessRevision();
  for (const size_t capacity : {size_t{0}, std::numeric_limits<size_t>::max()}) {
    const auto rejected = registry.open(capacity);
    CHECK(!rejected && !rejected.error().Message.empty() && registry.capacity() == 2 &&
              registry.alive(*first) && registry.LivenessRevision() == revision,
          "invalid capacity preserves the existing owner, storage and liveness revision");
  }
  const auto second = registry.addEntity();
  CHECK(second && registry.alive(*second), "rejected opens do not consume an identity");
  if (!second) { return Report(); }
  const auto fullRevision = registry.LivenessRevision();
  const auto rejected = registry.addEntity();
  CHECK(!rejected && !rejected.error().Message.empty() && registry.alive(*first) &&
            registry.alive(*second) && registry.LivenessRevision() == fullRevision,
        "capacity failure preserves all live identities and the revision");
  return Report();
}
