#include "MemoryBudget.h"
#include "Check.h"
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstddef>
#include <thread>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr size_t threads = 8;
constexpr size_t capacity = threads * (threads + 1) / 2;

void DebtWorker(const MemoryBudget &copy,
                size_t index,
                std::barrier<> &phase,
                std::atomic_size_t &failures) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  auto reservation = copy.TryReserve(index);
  while (!reservation && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::yield();
    reservation = copy.TryReserve(index);
  }
  if (!reservation) { ++failures; }
  phase.arrive_and_wait();
  phase.arrive_and_wait();
  if (copy.TryReserve(1) || (reservation && reservation->Resize(index + 1))) { ++failures; }
  if (reservation && !reservation->Resize(index - 1)) { ++failures; }
  phase.arrive_and_wait();
  phase.arrive_and_wait();
  if (reservation) {
    while (!reservation->Resize(index) && std::chrono::steady_clock::now() < deadline) {
      std::this_thread::yield();
    }
    if (reservation->Bytes() != index) { ++failures; }
  }
  phase.arrive_and_wait();
  phase.arrive_and_wait();
  reservation.reset();
}

void SynchronizedDebt() {
  MemoryBudget budget(capacity);
  std::atomic_size_t failures{0};
  std::barrier phase(static_cast<std::ptrdiff_t>(threads + 1));
  std::vector<std::jthread> workers;
  for (size_t index = 1; index <= threads; ++index) {
    workers.emplace_back([&, index] { DebtWorker(budget, index, phase, failures); });
  }
  phase.arrive_and_wait();
  CHECK(budget.Used() == capacity, "all independent workers fill one shared account");
  budget.SetMaximum(0);
  phase.arrive_and_wait();
  phase.arrive_and_wait();
  CHECK(budget.Maximum() == 0 && budget.Used() == capacity - threads && budget.Available() == 0 &&
            failures == 0,
        "synchronous limit reduction blocks every subsequent acquisition but permits shrinking");
  budget.SetMaximum(capacity);
  phase.arrive_and_wait();
  phase.arrive_and_wait();
  CHECK(budget.Used() == capacity && failures == 0,
        "raising the same account lets every retained worker recover its original capacity");
  phase.arrive_and_wait();
  workers.clear();
  CHECK(budget.Used() == 0, "the final independent owners return every byte");
}

void RacingLimits() {
  MemoryBudget budget(capacity);
  std::atomic_size_t failures{0};
  std::vector<std::jthread> workers;
  std::barrier race(static_cast<std::ptrdiff_t>(threads + 1));
  std::jthread updater([&] {
    race.arrive_and_wait();
    for (size_t step = 0; step < 2048; ++step) { budget.SetMaximum(step % (capacity + 1)); }
  });
  workers.clear();
  for (size_t index = 0; index < threads; ++index) {
    workers.emplace_back([&] {
      race.arrive_and_wait();
      for (size_t step = 0; step < 2048; ++step) {
        const auto reservation = budget.TryReserve(1);
        const auto memory = budget.Read();
        if (memory.Available() > memory.Maximum || memory.Used > threads) { ++failures; }
      }
    });
  }
  updater.join();
  workers.clear();
  CHECK(failures == 0 && budget.Used() == 0,
        "concurrent limit updates and acquisitions preserve coherent snapshots and accounting");
}
}

int main() {
  SynchronizedDebt();
  RacingLimits();
  Covers("concurrent live logical limits, debt and synchronous admission; no OS headroom claim");
  return Report();
}
