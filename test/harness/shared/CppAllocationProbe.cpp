#include "CppAllocationProbe.h"
#include <cstddef>
#include <cstdlib>

namespace {
bool Tracking = false;
outshine::Test::CppAllocationCounts Counts;
}

void *operator new(std::size_t bytes) {
  void *memory = std::malloc(bytes == 0 ? 1 : bytes);
  if (memory == nullptr) { std::abort(); }
  if (Tracking) {
    ++Counts.Calls;
    Counts.Bytes += bytes;
  }
  return memory;
}

void operator delete(void *memory) noexcept {
  std::free(memory);
}

void operator delete(void *memory, [[maybe_unused]] std::size_t bytes) noexcept {
  std::free(memory);
}

void outshine::Test::BeginCppAllocationProbe() noexcept {
  Counts = {};
  Tracking = true;
}

outshine::Test::CppAllocationCounts outshine::Test::EndCppAllocationProbe() noexcept {
  Tracking = false;
  return Counts;
}
