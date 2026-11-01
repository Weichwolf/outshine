#include "Check.h"
#include "JobAdmission.h"
#include <atomic>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

void MovesAndOwners() {
  auto first = std::make_unique<JobAdmission>(1);
  const JobAdmission second(1);
  auto lease = first->TryAcquire();
  auto replacement = second.TryAcquire();
  CHECK(lease && replacement && first->Active() == 1 && second.Active() == 1,
        "each capacity domain admits one independent job");
  if (!lease || !replacement) { return; }
  CHECK(!first->TryAcquire() && !second.TryAcquire(), "held leases exhaust their domains");
  {
    auto moved = std::move(*lease);
    CHECK(!*lease && moved && first->Active() == 1, "move construction transfers one credit");
    *replacement = std::move(moved);
  }
  CHECK(first->Active() == 1 && !first->TryAcquire() && second.Active() == 0,
        "move assignment releases the previous credit before transferring the new one");
  first.reset();
  replacement.reset();
  CHECK(second.TryAcquire().has_value(), "admission remains usable after a foreign owner ends");
}

void ConcurrentWork() {
  JobAdmission admission(3);
  std::atomic<unsigned> entered{0};
  std::atomic<unsigned> peak{0};
  std::vector<std::thread> workers;
  workers.reserve(8);
  for (int thread = 0; thread < 8; ++thread) {
    workers.emplace_back([&] {
      for (int attempt = 0; attempt < 1000; ++attempt) {
        const auto lease = admission.TryAcquire();
        if (!lease) { continue; }
        const auto active = entered.fetch_add(1, std::memory_order_relaxed) + 1;
        auto previous = peak.load(std::memory_order_relaxed);
        while (previous < active &&
               !peak.compare_exchange_strong(previous, active, std::memory_order_relaxed)) {}
        std::this_thread::yield();
        entered.fetch_sub(1, std::memory_order_relaxed);
      }
    });
  }
  for (auto &worker : workers) { worker.join(); }
  CHECK(peak.load() > 0 && peak.load() <= 3 && admission.Active() == 0,
        "concurrent grants never exceed capacity and every lease returns its credit");
  std::vector<JobAdmission::Lease> held;
  held.reserve(3);
  for (int index = 0; index < admission.Maximum(); ++index) {
    auto lease = admission.TryAcquire();
    CHECK(lease.has_value(), "all released credits are available again");
    if (lease) { held.push_back(std::move(*lease)); }
  }
  CHECK(held.size() == 3 && !admission.TryAcquire(), "exhaustion remains bounded after contention");
}
}

int main() {
  MovesAndOwners();
  ConcurrentWork();
  return outshine::Test::Report();
}
