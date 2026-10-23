#include "FlatMap.h"
#include "Check.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace {
struct CollisionHash {
  uint64_t operator()([[maybe_unused]] const uint64_t &key) const noexcept { return 0; }
};

struct alignas(64) AlignedValue {
  uint64_t Value = 0;
};

template <typename Map>
concept Cloneable = requires(const Map &map) { map.Clone(); };

bool CheckGeneration(outshine::FlatMap<uint32_t, uint64_t, CollisionHash> &map, uint32_t epoch) {
  using namespace outshine::Test;
  constexpr uint32_t entries = 160;
  constexpr uint32_t erased = 80;
  constexpr uint32_t growthFirst = 1000;
  constexpr uint32_t growthEnd = 1300;
  for (uint32_t key = 0; key < entries; ++key) {
    CHECK(map.Emplace(key, epoch).has_value(), "colliding source entries are inserted");
  }
  CHECK(map.Erase(80), "the source includes a repaired collision chain");
  auto copy = map.Clone();
  CHECK(copy && copy->Size() == map.Size() && copy->HeapBytes() == map.HeapBytes(),
        "clone preserves the table size and capacity");
  if (!copy) { return false; }
  CHECK(!copy->Find(80), "an erased key stays absent from the clone");
  for (uint32_t key = 0; key < entries; ++key) {
    if (key == erased) { continue; }
    const auto *value = copy->Find(key);
    CHECK(value && *value == epoch && value != map.Find(key),
          "clone retains the current generation with independent slot storage");
  }
  CHECK(copy->Erase(159) && map.Find(159), "clone erasure does not alter the source");
  CHECK(copy->Emplace(900, epoch).has_value() && !map.Find(900),
        "clone insertion remains independent");
  for (uint32_t key = growthFirst; key < growthEnd; ++key) {
    CHECK(copy->Emplace(key, epoch).has_value(), "clone can grow independently");
  }
  CHECK(map.Size() == 159 && !map.Find(1000) && copy->Find(0),
        "clone growth preserves both independent key sets");
  map.Clear();
  CHECK(copy->Find(0) && *copy->Find(0) == epoch, "source clear cannot invalidate cloned entries");
  auto empty = map.Clone();
  CHECK(empty && empty->Empty() && empty->HeapBytes() == 0 && !empty->Find(0),
        "cloning a cleared table avoids unused storage and prior generations");
  CHECK(empty && empty->Emplace(2000, epoch).has_value() && !map.Find(2000),
        "a cleared clone can independently reuse its slots");
  return true;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  static_assert(!Cloneable<FlatMap<std::unique_ptr<int>>>);
  FlatMap<uint32_t, uint64_t, CollisionHash> map;
  for (uint32_t epoch = 0; epoch < 8; ++epoch) {
    if (!CheckGeneration(map, epoch)) { return Report(); }
  }
  FlatMap<uint32_t> sparse;
  for (uint32_t key = 0; key < 256; ++key) {
    CHECK(sparse.Emplace(key, key).has_value(), "sparse control first grows its storage");
  }
  for (uint32_t key = 4; key < 256; ++key) { CHECK(sparse.Erase(key), "control is thinned"); }
  auto compact = sparse.Clone();
  CHECK(compact && compact->Size() == 4 && compact->HeapBytes() < sparse.HeapBytes() &&
            compact->Find(3) && *compact->Find(3) == 3 && !compact->Find(4),
        "clone preserves live entries without copying obsolete oversized storage");
  FlatMap<std::shared_ptr<int>> owning;
  auto value = std::make_shared<int>(7);
  const std::weak_ptr<int> weak = value;
  CHECK(owning.Emplace(1, std::move(value)).has_value(), "source owns its nontrivial value");
  auto retained = owning.Clone();
  CHECK(retained && weak.use_count() == 2, "clone copies each live shared owner once");
  owning.Clear();
  CHECK(retained && retained->Find(1) && **retained->Find(1) == 7 && weak.use_count() == 1,
        "source clear leaves the cloned owner alive");
  if (retained) { retained->Clear(); }
  CHECK(weak.expired(), "clearing the final clone releases its value");
  FlatMap<AlignedValue> aligned;
  CHECK(aligned.Emplace(3, AlignedValue{.Value = 19}).has_value(),
        "an over-aligned source value is inserted");
  auto alignedCopy = aligned.Clone();
  CHECK(alignedCopy && alignedCopy->Find(3) && alignedCopy->Find(3)->Value == 19 &&
            reinterpret_cast<uintptr_t>(alignedCopy->Find(3)) % alignof(AlignedValue) == 0,
        "cloned slots preserve value alignment");
  return Report();
}
