#include "src/base/FlatMap.h"
#include "Check.h"

#include <array>
#include <cstddef>
#include <cstdint>

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
    CHECK(inserted && inserted->second && *inserted->first == i,
          "colliding distinct key inserts its own value");
  }
  CHECK(map.Size() == count, "growth retains all colliding keys");
  for (int64_t i = 0; i < count; ++i) {
    const auto *found = map.Find({i, -i, i * 2});
    CHECK(found && *found == i, "full key comparison survives rehashing");
    const auto duplicate = map.Emplace({i, -i, i * 2}, 999);
    CHECK(duplicate && !duplicate->second && *duplicate->first == i,
          "equal keys retain the original value");
  }
  const auto &immutable = map;
  CHECK(!immutable.Find({1, 1, 1}), "same hash does not make an absent key present");
  map.Clear();
  CHECK(map.Empty() && !map.Find({0, 0, 0}), "epoch clear invalidates prior keys");
  const auto positive = map.Emplace({1, 1, 0}, 7);
  const auto negative = map.Emplace({-1, -1, 0}, 8);
  CHECK(positive && positive->second && negative && negative->second,
        "mirrored coordinates are distinct identities after reuse");
  FlatMap<int> integers;
  const auto integer = integers.Emplace(42, 7);
  CHECK(integer && integer->second && *integers.Find(42) == 7,
        "existing integer-key interface remains usable");
  std::array<uint64_t, 2> visited{};
  size_t visits = 0;
  const auto &readOnly = integers;
  readOnly.Visit([&](uint64_t key, const int value) {
    visited[visits++] = key;
    CHECK(value == 7, "visitation exposes the stored value");
  });
  CHECK(visits == 1 && visited[0] == 42, "const visitation reaches each occupied slot once");
  return Report();
}
