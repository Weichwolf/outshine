#include "TilePool.h"
#include "SourceSet.h"
#include "Check.h"

#include <cstdlib>
#include <cstddef>
#include <memory>
#include <new>
#include <string>

namespace {
using namespace outshine::Data;

thread_local bool rejectAllocation = false;

class NoTransport final : public SourceTransport {
public:
  FetchStart Begin(const std::string &) override { return Ticket::None; }

  Wire Collect(Ticket) override { return Wire::Never(); }

  void Cancel(Ticket) override {}
};

class PendingSource final : public Source {
public:
  SourceDecl Decl{.Id = "pending", .Keeps = Cacheability::Never};

  const SourceDecl &Declaration() const noexcept override { return Decl; }

  Coverage Covers(const SourceRequest &) const noexcept override { return Coverage::Inside; }

  Address Serves(const SourceRequest &request) const noexcept override { return request.Where(); }

  FetchStart Begin(const Address &, SourceTransport &transport) const override {
    return transport.Begin("fixture");
  }

  SourceResponse Collect(const Address &, Ticket, SourceTransport &) const override {
    return SourceResponse::Working();
  }
};

}

void *operator new(size_t bytes, std::align_val_t alignment, const std::nothrow_t &) noexcept {
  if (rejectAllocation) { return nullptr; }
  const size_t requested = static_cast<size_t>(alignment);
  const size_t supported = requested < sizeof(void *) ? sizeof(void *) : requested;
  void *block = nullptr;
  return posix_memalign(&block, supported, bytes) == 0 ? block : nullptr;
}

void operator delete(void *block, std::align_val_t) noexcept {
  std::free(block);
}

int main() {
  using namespace outshine::Test;
  using outshine::Ground::TilePool;
  RawSourceCache store({.Using = RawSourceCache::Use::Off});
  SourceSet sources(store);
  NoTransport transport;
  CHECK(sources.Add(std::make_unique<PendingSource>()) == SourceSet::Registration::Accepted,
        "pending source registers");
  TilePool pool({}, sources, transport);
  TilePool::Landing landing;
  rejectAllocation = true;
  CHECK(pool.Bytes(SourceRequest(DataKind::Elevation, Address::Whole(0)), &landing) ==
            TilePool::Reply::Deferred,
        "failed posted-index allocation defers rather than inventing pending work");
  rejectAllocation = false;
  const TilePool::Ledger rejected = pool.Counters();
  CHECK(rejected.Outstanding == 0 && rejected.AdmissionDeferred == 1 && rejected.Posts == 0,
        "failed admission leaves no queued ownership behind");
  CHECK(pool.Bytes(SourceRequest(DataKind::Elevation, Address::Whole(0)), &landing) ==
            TilePool::Reply::Pending,
        "same request admits once allocation recovers");
  return Report();
}
