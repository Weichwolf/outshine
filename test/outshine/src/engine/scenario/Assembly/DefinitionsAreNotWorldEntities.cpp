#include "Assembly.h"
#include "Assembled.h"
#include "Column.h"
#include "EntityRegistry.h"
#include "Scenario.h"
#include "Traits.h"
#include "Check.h"
#include <cstddef>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document source;
  constexpr size_t kDefinitions = 257;
  for (size_t at = 0; at < kDefinitions; ++at) {
    Scenario::Kind kind;
    kind.Name = "kind-" + std::to_string(at);
    kind.Capabilities = {"capability-" + std::to_string(at)};
    source.Kinds.push_back(kind);
  }
  const auto capacity = RequiredEntityCapacity(source);
  CHECK(capacity && *capacity == 0, "definitions reserve no world identity slots");
  World::EntityRegistry identities;
  Column<Scenario::Body> bodies;
  Column<Traits> attributes;
  Assembled assembled;
  std::string error;
  CHECK(Assemble(source, identities, bodies, attributes, assembled, error) &&
            assembled.Kinds.size() == kDefinitions && assembled.Instances.empty() &&
            identities.capacity() == 0 && identities.LivenessRevision() == 0,
        "more than 255 kind definitions remain bootstrap data without opening a world registry");
  return Report();
}
