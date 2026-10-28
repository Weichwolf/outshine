#include "Check.h"
#include "MaterialLifetime.h"
#include <memory>
#include <thread>
#include <utility>
#include <vector>

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  const auto first = std::make_shared<MaterialRetirementQueue>();
  const auto second = std::make_shared<MaterialRetirementQueue>();
  auto lifetime = std::make_shared<MaterialLifetime>();
  const MaterialRetirement left{.Slot = 4, .Generation = 7};
  const MaterialRetirement right{.Slot = 9, .Generation = 2};
  lifetime->Subscribe(first, left);
  lifetime->Subscribe(first, left);
  lifetime->Subscribe(second, right);
  {
    auto abandoned = std::make_shared<MaterialRetirementQueue>();
    const std::weak_ptr<MaterialRetirementQueue> weak = abandoned;
    lifetime->Subscribe(abandoned, left);
    abandoned.reset();
    CHECK(weak.expired(), "source subscriptions do not keep abandoned scenes alive");
  }
  std::vector<MaterialRetirement> events;
  first->Take(events);
  CHECK(events.empty(), "a living source cannot retire in one of its scenes");
  {
    const std::jthread worker([owned = std::move(lifetime)] mutable { owned.reset(); });
  }
  first->Take(events);
  CHECK(events.size() == 1 && events.front() == left,
        "the last source owner notifies the original scene once across threads");
  second->Take(events);
  CHECK(events.size() == 1 && events.front() == right,
        "the same source retires its independent world-candidate entry");
  first->Take(events);
  CHECK(events.empty(), "draining a queue cannot return or reinsert consumed events");
  return Report();
}
