#include <world/EntityRegistry.h>
#include <array>
#include <limits>
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  EntityRegistry registry;
  CHECK(registry.open(3), "bounded registry opens");
  const Entity parent = registry.addEntity(Role::Body);
  const Entity child = registry.addEntity(Role::Body);
  CHECK(registry.link(child, Relation::ChildOf, parent), "valid relation establishes baseline");
  for (const size_t invalid : {size_t{0}, std::numeric_limits<size_t>::max()}) {
    CHECK(!registry.open(invalid) && !registry.error().empty(), "invalid capacity is rejected");
    CHECK(registry.capacity() == 3 && registry.alive(parent) && registry.alive(child),
          "rejected capacity preserves storage and handle epoch");
  }
  for (const auto invalid : {static_cast<Role>(4), static_cast<Role>(255)}) {
    CHECK(registry.addEntity(invalid) == kNoEntity && !registry.error().empty(),
          "invalid roles cannot consume a slot or index a role table");
    std::array output{parent};
    CHECK(registry.entitiesWithRole(invalid, output) == 0 && output[0] == parent,
          "invalid role query preserves output");
    CHECK(registry.entitiesWithTagAndRole(tags::Does, invalid, output) == 0 && output[0] == parent,
          "invalid combined query preserves output");
  }
  CHECK(registry.alive(registry.addEntity(Role::Tool)),
        "rejected roles preserve remaining capacity");
  for (const auto invalid : {static_cast<Relation>(6), static_cast<Relation>(255)}) {
    CHECK(!registry.link(child, invalid, parent) && !registry.relink(child, invalid, parent),
          "invalid relations cannot reach rule or adjacency tables");
    CHECK(registry.targetOf(child, Relation::ChildOf) == parent,
          "invalid mutation preserves the accepted relation");
    std::array sources{parent};
    std::array targets{child};
    CHECK(registry.sources(parent, invalid, sources) == 0 && sources[0] == parent,
          "invalid incoming query preserves output");
    CHECK(registry.targets(child, invalid, targets) == 0 && targets[0] == child,
          "invalid outgoing query preserves output");
    CHECK(registry.linkedPairs(invalid, sources, targets) == 0 && sources[0] == parent &&
              targets[0] == child,
          "invalid pair query preserves both outputs");
  }
  return Report();
}
