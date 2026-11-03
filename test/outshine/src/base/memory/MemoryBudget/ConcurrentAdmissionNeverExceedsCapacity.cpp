#include "MemoryBudget.h"
#include "Check.h"
#include <atomic>
#include <latch>
#include <chrono>
#include <cstddef>
#include <thread>
#include <vector>

int main() {
  using outshine::MemoryBudget;
  using namespace outshine::Test;
  constexpr size_t capacity = 31;
  constexpr size_t threads = 8;
  constexpr size_t iterations = 256;
  const MemoryBudget budget(capacity);
  std::atomic_size_t live{0};
  std::atomic_size_t completed{0};
  std::atomic_size_t failures{0};
  std::latch start(static_cast<std::ptrdiff_t>(threads));
  std::vector<std::jthread> workers;
  workers.reserve(threads);
  for (size_t index = 0; index < threads; ++index) {
    workers.emplace_back([&, copy = budget, index] {
      start.arrive_and_wait();
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
      for (size_t step = 0; step < iterations; ++step) {
        const size_t bytes = 1 + (index + step) % 9;
        auto reservation = copy.TryReserve(bytes);
        while (!reservation && std::chrono::steady_clock::now() < deadline) {
          std::this_thread::yield();
          reservation = copy.TryReserve(bytes);
        }
        if (!reservation) {
          ++failures;
          return;
        }
        if (live.fetch_add(bytes) + bytes > capacity || copy.Used() > capacity) { ++failures; }
        std::this_thread::yield();
        live.fetch_sub(bytes);
        reservation.reset();
        ++completed;
      }
    });
  }
  workers.clear();
  CHECK(failures == 0 && completed == threads * iterations,
        "concurrent copied facades finish all admitted work within the deadline");
  CHECK(live == 0 && budget.Used() == 0 && budget.Maximum() == capacity,
        "the last worker returns every reserved byte");
  Covers("ConcurrentAdmissionNeverExceedsCapacity; logical reserved bytes, not physical process "
         "memory");
  return Report();
}
