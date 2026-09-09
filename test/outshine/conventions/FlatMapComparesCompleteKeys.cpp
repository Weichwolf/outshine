#include "../../../src/base/FlatMap.h"
#include "Check.h"

namespace {
struct PositionKey {
  int64_t East;
  int64_t North;
  int64_t Height;
  constexpr bool operator==(const PositionKey &) const = default;
};

struct CollisionHash {
  constexpr uint64_t operator()(const PositionKey &) const noexcept { return 0; }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  FlatMap<uint32_t, PositionKey, CollisionHash> map;
  constexpr int64_t count = 200;
  for (int64_t i = 0; i < count; ++i) {
    const auto inserted = map.Emplace({i, -i, i * 2}, static_cast<uint32_t>(i));
    CHECK(inserted.second && *inserted.first == i, "colliding distinct key inserts its own value");
  }
  CHECK(map.Size() == count, "growth retains all colliding keys");
  for (int64_t i = 0; i < count; ++i) {
    const auto *found = map.Find({i, -i, i * 2});
    CHECK(found && *found == i, "full key comparison survives rehashing");
    const auto duplicate = map.Emplace({i, -i, i * 2}, 999);
    CHECK(!duplicate.second && *duplicate.first == i, "equal keys retain the original value");
  }
  const auto &immutable = map;
  CHECK(!immutable.Find({1, 1, 1}), "same hash does not make an absent key present");
  map.Clear();
  CHECK(map.Empty() && !map.Find({0, 0, 0}), "epoch clear invalidates prior keys");
  CHECK(map.Emplace({1, 1, 0}, 7).second && map.Emplace({-1, -1, 0}, 8).second,
        "mirrored coordinates are distinct identities after reuse");
  FlatMap<int> integers;
  CHECK(integers.Emplace(42, 7).second && *integers.Find(42) == 7,
        "existing integer-key interface remains usable");
  return Report();
}
