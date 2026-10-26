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

class NoTransport final : public Transport {
public:
  FetchStart Begin(const std::string & /*url*/) override { return Ticket::None; }

  Wire Collect(Ticket /*ticket*/) override { return Wire::Never(); }

  void Cancel(Ticket /*ticket*/) override {}
};

class PendingSource final : public Source {
public:
  SourceDecl Decl{.Id = "pending",
                  .Revision = {},
                  .Endpoint = {},
                  .Keeps = Cacheability::Never,
                  .PayloadSha256 = {},
                  .Schema = {}};

  [[nodiscard]] const SourceDecl &Declaration() const noexcept override { return Decl; }

  [[nodiscard]] Coverage Covers(const SourceRequest & /*request*/) const noexcept override {
    return Coverage::Inside;
  }

  [[nodiscard]] Address Serves(const SourceRequest &request) const noexcept override {
    return request.Where();
  }

  FetchStart Begin(const Address & /*at*/, Transport &transport) const override {
    return transport.Begin("fixture");
  }

  SourceResponse
  Collect(const Address & /*at*/, Ticket /*ticket*/, Transport & /*transport*/) const override {
    return SourceResponse::Working();
  }
};

}

void *
operator new(size_t bytes, std::align_val_t alignment, const std::nothrow_t & /*unused*/) noexcept {
  if (rejectAllocation) { return nullptr; }
  const auto requested = static_cast<size_t>(alignment);
  const size_t supported = requested < sizeof(void *) ? sizeof(void *) : requested;
  void *block = nullptr;
  return posix_memalign(&block, supported, bytes) == 0 ? block : nullptr;
}

void operator delete(void *block, std::align_val_t /*unused*/) noexcept {
  std::free(block);
}

int main() {
  using namespace outshine::Test;
  using outshine::Ground::TilePool;
  RawSourceCache store({.Directory = {}, .Using = RawSourceCache::Use::Off});
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
