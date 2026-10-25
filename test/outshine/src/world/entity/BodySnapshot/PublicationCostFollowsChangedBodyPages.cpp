#include "EntityState.h"
#include "BodySnapshotData.h"
#include "Check.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <print>
#include <vector>
#include <utility>
#include <ratio>

namespace {
void Measure(size_t changed) {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr size_t kBodies = 1024;
  constexpr size_t kFrames = 120;
  World::EntityState world;
  CHECK(world.Open(kBodies), "publication cost world opens");
  std::vector<World::EntityBodyState> bodies;
  std::vector<Entity> owners;
  for (size_t i = 0; i < kBodies; ++i) {
    const auto entity = world.Registry().addEntity();
    CHECK(entity, "cost case allocates a native identity");
    if (!entity) { return; }
    owners.push_back(*entity);
    World::EntityBodyState body;
    body.Owner = *entity;
    bodies.push_back(body);
  }
  CHECK(world.SetBodies(std::move(bodies)), "cost case publishes native source rows");
  auto previous = world.PublishBodies();
  std::vector<double> microseconds;
  size_t copied = 0;
  size_t pages = 0;
  for (size_t frame = 0; frame < kFrames; ++frame) {
    for (size_t i = 0; i < changed; ++i) {
      CHECK(world.EditBody(owners[i], [](World::RigidBodyState &body) { body.PositionM[0] += 1; }),
            "native writer marks each changed body");
    }
    const auto begin = std::chrono::steady_clock::now();
    const auto next = world.PublishBodies();
    microseconds.push_back(
        std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - begin)
            .count());
    copied += world.PublicationCost().CopiedBytes;
    pages += world.PublicationCost().NewPages;
    CHECK(previous.Body(owners.front())->PositionM[0] == (changed ? static_cast<double>(frame) : 0),
          "the previous publication stays frozen while the new one is created");
    previous = next;
  }
  const size_t dirtyPages =
      (changed + World::kBodiesPerSnapshotPage - 1) / World::kBodiesPerSnapshotPage;
  CHECK(pages == kFrames * dirtyPages && copied == kFrames * dirtyPages *
                                                       World::kBodiesPerSnapshotPage *
                                                       sizeof(World::EntityBodyState),
        "native copied payload follows changed pages rather than all bodies");
  std::ranges::sort(microseconds);
  constexpr size_t kP99 = kFrames - 2;
  std::println("body-publication bodies={} changed={} frames={} copied-bytes={} new-pages={} "
               "native-p50-us={} native-p99-us={}",
               kBodies,
               changed,
               kFrames,
               copied,
               pages,
               microseconds[kFrames / 2],
               microseconds[kP99]);
}
}

int main() {
  Measure(0);
  Measure(1);
  constexpr size_t kBodies = 1024;
  Measure(kBodies);
  return outshine::Test::Report();
}
