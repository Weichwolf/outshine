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

void *operator new(size_t bytes) {
  ++allocationAttempts;
  if (rejectAllocation) { throw std::bad_alloc{}; }
  if (void *block = std::malloc(bytes == 0 ? 1 : bytes)) { return block; }
  throw std::bad_alloc{};
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
    CHECK(inserted.second, "new keys remain insertable across growth");
    uint64_t *const previous = map.Find(0);
    const size_t before = allocationAttempts;
    bool completed = false;
    bool retained = false;
    rejectAllocation = true;
    try {
      const auto duplicate = map.Emplace(0, 999);
      completed = true;
      retained = !duplicate.second && duplicate.first == previous && *duplicate.first == 1000;
    } catch (const std::bad_alloc &) {}
    rejectAllocation = false;
    CHECK(completed && allocationAttempts == before, "duplicate insertion never allocates");
    CHECK(retained, "duplicate insertion preserves the existing value and its address");
    CHECK(map.Size() == key + 1, "duplicate insertion retains cardinality");
  }
  return Report();
}
