#include <Outshine.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace {
std::atomic<size_t> allocations{0};

void Exhausted() {
  std::abort();
}
}

void *operator new(size_t bytes) {
  void *block = std::malloc(bytes == 0 ? 1 : bytes);
  if (block == nullptr) { std::abort(); }
  allocations.fetch_add(1, std::memory_order_relaxed);
  return block;
}

void operator delete(void *block) noexcept {
  std::free(block);
}

int main() {
  const auto previous = std::set_new_handler(Exhausted);
  const size_t before = allocations.load();
  {
    outshine::Engine engine;
    if (allocations.load() <= before || std::get_new_handler() != Exhausted) { return 1; }
  }
  if (std::get_new_handler() != Exhausted) { return 2; }
  std::set_new_handler(previous);
  std::puts("host allocator retained through engine lifetime");
  return 0;
}
