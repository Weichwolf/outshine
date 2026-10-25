#include "Assembly.h"
#include "Assembled.h"
#include "Column.h"
#include "EntityRegistry.h"
#include "Scenario.h"
#include "Traits.h"
#include "Check.h"
#include <cstddef>
#include <string>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;

Scenario::Document Chain(size_t depth) {
  Scenario::Document source;
  for (size_t at = 0; at < depth; ++at) {
    Scenario::Kind kind;
    kind.Name = "kind-" + std::to_string(at);
    if (at > 0) { kind.Inherits = source.Kinds.back().Name; }
    source.Kinds.push_back(std::move(kind));
  }
  Scenario::Instance instance;
  instance.Id = "instance";
  instance.Of = source.Kinds.back().Name;
  source.Instances.push_back(std::move(instance));
  return source;
}

void Verify(const Scenario::Document &source, bool accepted, size_t expectedTraits = 0) {
  const auto capacity = RequiredEntityCapacity(source);
  CHECK(capacity && *capacity == 1, "definition depth does not consume world identities");
  if (!capacity) { return; }
  World::EntityRegistry identities;
  Column<Scenario::Body> bodies;
  Column<Traits> attributes;
  CHECK(identities.open(*capacity) && bodies.Open(identities) && attributes.Open(identities),
        "one actual instance owns bounded native storage");
  Assembled assembled;
  std::string error;
  const bool built = Assemble(source, identities, bodies, attributes, assembled, error);
  CHECK(built == accepted && (built || !error.empty()),
        "a rejected definition reports an error rather than silently dropping defaults");
  if (!built || !accepted) { return; }
  const auto *values = attributes.Get(assembled.InstanceNamed("instance"));
  CHECK(expectedTraits == 0 ? values == nullptr : values && values->Count == expectedTraits,
        "successful instance retains its complete inherited attribute union");
}
}

int main() {
  auto source = Chain(8);
  Verify(source, true);
  Verify(Chain(9), false);
  for (size_t at = 0; at < Traits::kMost; ++at) {
    source.Kinds.front().Attributes.push_back(
        {.Name = "attribute-" + std::to_string(at), .Value = "1"});
  }
  source.Instances.front().Attributes = {{.Name = "attribute-0", .Value = "2"}};
  Verify(source, true, Traits::kMost);
  source.Instances.front().Attributes.push_back({.Name = "one-too-many", .Value = "3"});
  Verify(source, false);
  source.Instances.front().Attributes.clear();
  source.Kinds.back().Attributes = {{.Name = "one-too-many", .Value = "3"}};
  Verify(source, false);
  source.Kinds.back().Attributes.clear();
  source.Kinds.front().Attributes.push_back({.Name = "one-too-many", .Value = "3"});
  Verify(source, false);
  return Report();
}
