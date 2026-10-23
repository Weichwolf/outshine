#include "FlatMap.h"
#include "Check.h"
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <new>
#include <limits>
#include <memory>
#include <utility>

namespace {
thread_local bool rejectAllocation = false;
thread_local size_t allowedAllocations = std::numeric_limits<size_t>::max();

struct alignas(64) TrackedValue {
  inline static size_t Live = 0;
  uint64_t Id = 0;

  TrackedValue() noexcept { ++Live; }

  ~TrackedValue() { --Live; }

  TrackedValue(const TrackedValue &) = delete;
  TrackedValue &operator=(const TrackedValue &) = delete;

  TrackedValue(TrackedValue &&other) noexcept : Id(std::exchange(other.Id, 0)) { ++Live; }

  TrackedValue &operator=(TrackedValue &&other) noexcept {
    Id = std::exchange(other.Id, 0);
    return *this;
  }
};
}

void *operator new(size_t sz,
                   std::align_val_t alignment,
                   [[maybe_unused]] const std::nothrow_t &tag) noexcept {
  if (rejectAllocation || allowedAllocations == 0) { return nullptr; }
  if (allowedAllocations != std::numeric_limits<size_t>::max()) { --allowedAllocations; }
  const auto requested = static_cast<size_t>(alignment);
  const size_t supported = requested < sizeof(void *) ? sizeof(void *) : requested;
  void *block = nullptr;
  return posix_memalign(&block, supported, sz) == 0 ? block : nullptr;
}

void operator delete(void *p, [[maybe_unused]] std::align_val_t alignment) noexcept {
  std::free(p);
}

namespace {
void CheckMovedFrom(outshine::FlatMap<uint64_t> &map) {
  using namespace outshine::Test;
  CHECK(map.Empty() && map.HeapBytes() == 0 && !map.Find(0), "move leaves a coherent empty source");
  map.Clear();
  CHECK(map.Empty() && map.HeapBytes() == 0, "a moved-from owner can be cleared");
}

void CheckPartialCloneFailure() {
  using namespace outshine;
  using namespace outshine::Test;
  FlatMap<std::shared_ptr<int>> source;
  const auto value = std::make_shared<int>(7);
  constexpr uint64_t sourceCount = 100;
  constexpr uint64_t firstGrowthCount = 46;
  for (uint64_t key = 0; key < sourceCount; ++key) {
    CHECK(source.Emplace(key, value).has_value(), "clone control grows its source table");
  }
  for (uint64_t key = firstGrowthCount; key < sourceCount; ++key) {
    CHECK(source.Erase(key), "clone control retains enough entries for two allocations");
  }
  const auto *first = source.Find(0);
  const auto owners = value.use_count();
  allowedAllocations = 1;
  const auto failed = source.Clone();
  allowedAllocations = std::numeric_limits<size_t>::max();
  CHECK(!failed && failed.error() == FlatMapError::AllocationFailed && source.Size() == 46 &&
            source.Find(0) == first && value.use_count() == owners,
        "failed clone compaction releases partial copies without changing the source");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CheckPartialCloneFailure();
  FlatMap<uint64_t> map;
  static_assert(noexcept(map.Emplace(0, 0)));
  rejectAllocation = true;
  const auto emptyClone = map.Clone();
  CHECK(emptyClone && emptyClone->Empty() && emptyClone->HeapBytes() == 0,
        "cloning an unallocated map does not allocate");
  const auto emptyFailure = map.Emplace(0, 1000);
  rejectAllocation = false;
  CHECK(!emptyFailure && emptyFailure.error() == FlatMapError::AllocationFailed,
        "initial storage failure is explicit");
  CHECK(map.Empty() && map.HeapBytes() == 0 && !map.Find(0),
        "failed initial allocation publishes nothing");
  CHECK(map.Emplace(0, 1000).has_value(), "initial allocation can be retried");
  size_t failures = 0;
  for (uint64_t key = 1; key < 256; ++key) {
    const uint64_t *const previous = map.Find(0);
    const size_t bytes = map.HeapBytes();
    rejectAllocation = true;
    const auto inserted = map.Emplace(key, key + 1000);
    rejectAllocation = false;
    if (inserted) { continue; }
    ++failures;
    CHECK(inserted.error() == FlatMapError::AllocationFailed, "growth failure is explicit");
    CHECK(map.Size() == key && map.HeapBytes() == bytes && map.Find(0) == previous &&
              !map.Find(key),
          "failed growth preserves storage, references and cardinality");
    for (uint64_t existing = 0; existing < key; ++existing) {
      const uint64_t *value = map.Find(existing);
      CHECK(value && *value == existing + 1000, "failed growth retains every prior entry");
    }
    CHECK(map.Emplace(key, key + 1000).has_value(),
          "growth can be retried after allocation recovers");
  }
  CHECK(failures > 1, "fault injection exercised multiple growth allocations");
  const uint64_t *const first = map.Find(0);
  rejectAllocation = true;
  const auto cloneFailure = map.Clone();
  rejectAllocation = false;
  CHECK(!cloneFailure && cloneFailure.error() == FlatMapError::AllocationFailed &&
            map.Size() == 256 && map.Find(0) == first,
        "clone allocation failure preserves the complete original owner");
  FlatMap<uint64_t> moved(std::move(map));
  CheckMovedFrom(map);
  CHECK(moved.Find(0) == first && moved.Size() == 256,
        "move preserves owned entries and their addresses");
  CHECK(map.Emplace(900, 4).has_value(), "moved-from map can be reused");
  map = std::move(moved);
  CheckMovedFrom(moved);
  CHECK(map.Find(0) == first && !map.Find(900), "move assignment replaces ownership");
  moved.Clear();
  CHECK(moved.Emplace(700, 8).has_value(), "cleared moved-from map can allocate again");
  FlatMap<uint32_t, uint32_t> small;
  const auto smallValue = small.Emplace(1, 2);
  CHECK(smallValue && *smallValue->first == 2, "small valid alignments are supported");
  {
    constexpr uint64_t entries = 100;
    FlatMap<TrackedValue> aligned;
    for (uint64_t key = 0; key < entries; ++key) {
      TrackedValue value;
      value.Id = key;
      const auto inserted = aligned.Emplace(key, std::move(value));
      CHECK(inserted && reinterpret_cast<uintptr_t>(inserted->first) % alignof(TrackedValue) == 0,
            "over-aligned slot values retain their alignment across growth");
    }
    FlatMap<TrackedValue> owner(std::move(aligned));
    for (uint64_t key = 0; key < entries; ++key) {
      const auto *value = owner.Find(key);
      CHECK(value && value->Id == key, "rehash preserves nontrivial moved values");
    }
  }
  CHECK(TrackedValue::Live == 0, "all constructed slot values are destroyed exactly once");
  return Report();
}
