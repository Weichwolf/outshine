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

struct NonDefaultValue {
  int Value;

  NonDefaultValue() = delete;

  constexpr explicit NonDefaultValue(int value) noexcept : Value(value) {}

  constexpr NonDefaultValue(const NonDefaultValue &) noexcept = default;
  constexpr NonDefaultValue(NonDefaultValue &&) noexcept = default;
  constexpr NonDefaultValue &operator=(const NonDefaultValue &) noexcept = default;
  constexpr NonDefaultValue &operator=(NonDefaultValue &&) noexcept = default;
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
  const size_t retainedBytes = map.HeapBytes();
  CHECK(map.Erase({99, -99, 198}), "erasing a colliding key succeeds");
  CHECK(!map.Find({99, -99, 198}) && map.Size() == count - 1 && map.HeapBytes() == retainedBytes,
        "erasure releases one key without changing retained storage");
  for (int64_t i = 100; i < count; ++i) {
    const auto *found = map.Find({i, -i, i * 2});
    CHECK(found && *found == i, "erasure repairs the following collision chain");
  }
  CHECK(!map.Erase({999, -999, 1998}), "erasing an absent key changes nothing");
  for (int64_t i = 0; i < count; ++i) {
    if (i == 99) { continue; }
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
  FlatMap<NonDefaultValue> nonDefault;
  const auto held = nonDefault.Emplace(8, NonDefaultValue{12});
  CHECK(held && held->second && held->first->Value == 12,
        "slot storage constructs only occupied non-default values");
  return Report();
}
