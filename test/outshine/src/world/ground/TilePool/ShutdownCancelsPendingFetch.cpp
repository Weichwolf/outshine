#include "TilePool.h"
#include "SourceSet.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

namespace {
using namespace outshine::Data;

class PendingTransport final : public Transport {
public:
  std::atomic<int> Cancelled{0};

  Ticket Begin(const std::string &) override { return static_cast<Ticket>(1); }

  Wire Collect(Ticket) override { return Wire::Never(); }

  void Cancel(Ticket ticket) override {
    if (ticket == static_cast<Ticket>(1)) { ++Cancelled; }
  }

  bool Await(double) override {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return false;
  }
};

class PendingSource final : public Source {
public:
  SourceDecl Decl{.Id = "pending", .Keeps = Cacheability::Never};
  mutable std::atomic<int> Polls{0};

  const SourceDecl &Declaration() const noexcept override { return Decl; }

  Coverage Covers(const Fetch &) const noexcept override { return Coverage::Inside; }

  Address Serves(const Fetch &request) const noexcept override { return request.Where(); }

  Ticket Begin(const Address &, Transport &transport) const override {
    return transport.Begin("fixture");
  }

  Fetched Collect(const Address &, Ticket, Transport &) const override {
    ++Polls;
    return Fetched::Working();
  }
};
}

int main() {
  using namespace outshine::Test;
  using outshine::Ground::TilePool;
  ContentStore store({.Using = ContentStore::Use::Off});
  SourceSet sources(store);
  PendingTransport transport;
  auto source = std::make_unique<PendingSource>();
  const auto *probe = source.get();
  CHECK(sources.Add(std::move(source)) == SourceSet::Registration::Accepted,
        "pending source registers");
  constexpr int attempts = 3000;
  auto pool = std::make_unique<TilePool>(
      TilePool::Config{.PollAttempts = attempts, .Carriers = 1}, sources, transport);
  TilePool::Landing landing;
  CHECK(pool->Bytes(Fetch(DataKind::Elevation, Address::Whole(0)), &landing) ==
            TilePool::Reply::Pending,
        "fetch schedules asynchronously");
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (probe->Polls == 0 && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(probe->Polls > 0, "carrier is polling before shutdown");
  const auto began = std::chrono::steady_clock::now();
  pool.reset();
  const double elapsed =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
  // The fixture's full retry budget requires at least 3000 × 1 ms; shutdown gets a separate 1 s
  // bound.
  CHECK(elapsed < 1.0, "shutdown does not consume the outstanding retry budget");
  CHECK(probe->Polls < attempts, "shutdown interrupts source polling");
  CHECK(transport.Cancelled == 1, "the outstanding ticket is cancelled exactly once");
  return Report();
}
