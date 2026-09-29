#include "FlatMap.h"
#include "Check.h"
#include <cstdint>
#include <memory>
#include <utility>

namespace {
struct CollisionHash {
  uint64_t operator()(const uint64_t &) const noexcept { return 0; }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  FlatMap<uint32_t, uint64_t, CollisionHash> map;
  for (uint32_t epoch = 0; epoch < 64; ++epoch) {
    for (uint32_t key = 0; key < 160; ++key) {
      const auto added = map.Emplace(key, epoch);
      CHECK(added && added->second && *added->first == epoch,
            "reused colliding slots expose only this generation");
    }
    CHECK(map.Erase(80) && !map.Find(80) && map.Find(159),
          "collision repair remains valid after generation reuse");
    const auto bytes = map.HeapBytes();
    map.Clear();
    CHECK(map.Empty() && map.HeapBytes() == bytes && !map.Find(0) && !map.Find(159),
          "clear retains storage without exposing old entries");
    size_t visited = 0;
    map.Visit([&](uint64_t, uint32_t) { ++visited; });
    CHECK(visited == 0, "iteration excludes every previous generation");
  }
  auto moved = std::move(map);
  const auto added = moved.Emplace(1, 9);
  CHECK(added && added->second && *added->first == 9 && map.Empty(),
        "move preserves the current generation and permits reuse");

  FlatMap<std::shared_ptr<int>> owning;
  auto object = std::make_shared<int>(7);
  std::weak_ptr<int> weak = object;
  const auto owned = owning.Emplace(1, std::move(object));
  CHECK(owned && !weak.expired(), "map owns the nontrivial value");
  owning.Clear();
  CHECK(weak.expired() && owning.Empty(), "clear immediately releases nontrivial resources");
  owning.Clear();
  CHECK(owning.Empty(), "repeated empty clears preserve lifetime invariants");
  return Report();
}
