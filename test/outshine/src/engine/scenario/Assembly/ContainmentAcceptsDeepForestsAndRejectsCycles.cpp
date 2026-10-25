#include "Assembly.h"
#include "Assembled.h"
#include "Column.h"
#include "EntityRegistry.h"
#include "Scenario.h"
#include "Traits.h"
#include "Check.h"
#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;

Scenario::Document Chain(size_t count) {
  Scenario::Document source;
  Scenario::Kind kind;
  kind.Name = "node";
  source.Kinds.push_back(kind);
  source.Instances.reserve(count);
  for (size_t at = 0; at < count; ++at) {
    Scenario::Instance instance;
    instance.Of = "node";
    instance.Id = "instance-" + std::to_string(at);
    if (at > 0) { instance.In = source.Instances.back().Id; }
    source.Instances.push_back(std::move(instance));
  }
  return source;
}

void Verify(const Scenario::Document &source, bool accepted) {
  const auto capacity = RequiredEntityCapacity(source);
  CHECK(capacity && *capacity == source.Instances.size(),
        "the complete deep source fits the unchanged scenario admission budget");
  if (!capacity) { return; }
  World::EntityRegistry identities;
  Column<Scenario::Body> bodies;
  Column<Traits> attributes;
  CHECK(identities.open(*capacity) && bodies.Open(identities) && attributes.Open(identities),
        "the complete candidate has one identity authority");
  Assembled assembled;
  std::string error;
  const bool built = Assemble(source, identities, bodies, attributes, assembled, error);
  CHECK(built == accepted && (built || error.contains("cycle")),
        "deep forests remain valid and complete cycles fail explicitly");
  if (!built || !accepted) { return; }
  CHECK(assembled.Instances.size() == source.Instances.size(),
        "no suffix is dropped to make a deep forest fit");
  if (assembled.Instances.size() != source.Instances.size()) { return; }
  bool complete = true;
  for (size_t at = 0; at < assembled.Instances.size(); ++at) {
    const auto parent = assembled.Instances[at].Holder;
    const bool bound = parent ? *parent < assembled.Instances.size() &&
                                    assembled.Instances[*parent].Name == source.Instances[at].In
                              : source.Instances[at].In.empty();
    complete = complete && bound && identities.alive(assembled.Instances[at].Value);
  }
  CHECK(complete, "all declared parent bindings survive either declaration order");
}
}

int main() {
  constexpr size_t kDeepInstances = 60000;
  auto source = Chain(kDeepInstances);
  Verify(source, true);
  std::ranges::reverse(source.Instances);
  Verify(source, true);
  source.Instances.back().In = source.Instances.front().Id;
  Verify(source, false);
  auto self = Chain(1);
  self.Instances.front().In = self.Instances.front().Id;
  Verify(self, false);
  return Report();
}
