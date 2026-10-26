#include "EntityState.h"
#include "Check.h"
#include <atomic>
#include <cstddef>
#include <thread>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  World::EntityState world;
  CHECK(world.Open(1), "one native world opens");
  const auto entity = world.Registry().addEntity();
  CHECK(entity, "native owner exists");
  if (!entity) { return Report(); }
  World::EntityBodyState body;
  body.Owner = *entity;
  CHECK(world.SetBodies({body}), "native row installs");
  const auto held = world.PublishBodies();
  std::atomic<bool> valid{true};
  std::jthread reader([&] {
    constexpr size_t kReads = 65536;
    for (size_t i = 0; i < kReads; ++i) {
      const auto *pose = held.Body(*entity);
      if (!pose || pose->PositionM[0] != 0) { valid.store(false, std::memory_order_relaxed); }
    }
  });
  constexpr size_t kWrites = 64;
  for (size_t i = 0; i < kWrites; ++i) {
    CHECK(world.EditBody(*entity, [](World::RigidBodyState &pose) { pose.PositionM[0] += 1; }),
          "single writer changes the next pose");
    const auto published = world.PublishBodies();
    CHECK(published.Body(*entity)->PositionM[0] == static_cast<double>(i + 1),
          "new readers see the complete new pose");
  }
  reader.join();
  CHECK(valid.load(std::memory_order_relaxed),
        "a concurrent held reader never aliases the mutable native writer");
  return Report();
}
