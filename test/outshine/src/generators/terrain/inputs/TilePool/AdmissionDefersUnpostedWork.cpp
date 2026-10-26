#include "TilePool.h"
#include "SourceSet.h"
#include "Check.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

namespace {
using namespace outshine::Data;

class PendingTransport final : public Transport {
public:
  FetchStart Begin(const std::string &) override { return static_cast<Ticket>(1); }

  Wire Collect(Ticket) override { return Wire::Never(); }

  void Cancel(Ticket) override {}

  bool Await(double) override {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return false;
  }
};

class PendingSource final : public Source {
public:
  PendingSource() {
    Decl.Id = "pending";
    Decl.Keeps = Cacheability::Never;
  }

  SourceDecl Decl;
  mutable std::atomic<int> Polls = 0;

  const SourceDecl &Declaration() const noexcept override { return Decl; }

  Coverage Covers(const SourceRequest &) const noexcept override { return Coverage::Inside; }

  Address Serves(const SourceRequest &request) const noexcept override { return request.Where(); }

  FetchStart Begin(const Address &, Transport &transport) const override {
    return transport.Begin("fixture");
  }

  SourceResponse Collect(const Address &, Ticket, Transport &) const override {
    ++Polls;
    return SourceResponse::Working();
  }
};

}

int main() {
  using namespace outshine::Test;
  using outshine::Ground::TilePool;
  RawSourceCache::Config cache;
  cache.Using = RawSourceCache::Use::Off;
  RawSourceCache store(cache);
  SourceSet sources(store);
  PendingTransport transport;
  auto source = std::make_unique<PendingSource>();
  const PendingSource *const probe = source.get();
  CHECK(sources.Add(std::move(source)) == SourceSet::Registration::Accepted,
        "pending source registers");
  TilePool::Config config;
  config.Carriers = 1;
  config.OutstandingMost = 1;
  TilePool pool(config, sources, transport);
  TilePool::Landing landing;
  const SourceRequest first(DataKind::Elevation, Address::Whole(0));
  CHECK(pool.Bytes(first, &landing) == TilePool::Reply::Pending,
        "first request occupies the only admission slot");
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (probe->Polls == 0 && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(probe->Polls > 0, "carrier owns the admitted request");
  CHECK(pool.Bytes(SourceRequest(DataKind::Elevation, Address::Whole(1)), &landing) ==
            TilePool::Reply::Deferred,
        "a distinct request beyond capacity remains unposted for a later retry");
  CHECK(pool.Bytes(first, &landing) == TilePool::Reply::Pending,
        "a repeated admitted request remains coalesced while capacity is full");
  const TilePool::Ledger ledger = pool.Counters();
  CHECK(ledger.Outstanding == 1 && ledger.AdmissionDeferred == 1 && ledger.Repeats == 1,
        "admission reports one deferred request without duplicating the live request");
  return Report();
}
