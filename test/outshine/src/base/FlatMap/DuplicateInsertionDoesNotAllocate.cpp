#include "FlatMap.h"
#include "Check.h"
#include <cstdlib>
#include <new>
#include <cstddef>
#include <cstdint>

namespace {
thread_local bool rejectAllocation = false;
thread_local size_t allocationAttempts = 0;
}

void *operator new(size_t bytes, std::align_val_t alignment, const std::nothrow_t &) noexcept {
  ++allocationAttempts;
  if (rejectAllocation) { return nullptr; }
  const size_t requested = static_cast<size_t>(alignment);
  const size_t supported = requested < sizeof(void *) ? sizeof(void *) : requested;
  void *block = nullptr;
  return posix_memalign(&block, supported, bytes) == 0 ? block : nullptr;
}

void operator delete(void *block, std::align_val_t) noexcept {
  std::free(block);
}

void operator delete(void *block) noexcept {
  std::free(block);
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  FlatMap<uint64_t> map;
  for (uint64_t key = 0; key < 256; ++key) {
    const auto inserted = map.Emplace(key, key + 1000);
    CHECK(inserted && inserted->second, "new keys remain insertable across growth");
    uint64_t *const previous = map.Find(0);
    const size_t before = allocationAttempts;
    rejectAllocation = true;
    const auto duplicate = map.Emplace(0, 999);
    const bool completed = duplicate.has_value();
    const bool retained = duplicate && !duplicate->second && duplicate->first == previous &&
                          *duplicate->first == 1000;
    rejectAllocation = false;
    CHECK(completed && allocationAttempts == before, "duplicate insertion never allocates");
    CHECK(retained, "duplicate insertion preserves the existing value and its address");
    CHECK(map.Size() == key + 1, "duplicate insertion retains cardinality");
  }
  return Report();
}
