#include "TilePool.h"
#include "SourceSet.h"
#include "Check.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace outshine::Data;

class NoTransport final : public Transport {
public:
  Ticket Begin(const std::string &) override { return Ticket::None; }

  Wire Collect(Ticket) override { return Wire::Never(); }

  void Cancel(Ticket) override {}
};

class CountingSource final : public Source {
public:
  SourceDecl Decl{.Id = "counting", .Revision = "test-r1", .Keeps = Cacheability::Never};
  mutable size_t Calls = 0;

  const SourceDecl &Declaration() const noexcept override { return Decl; }

  Coverage Covers(const Fetch &) const noexcept override { return Coverage::Inside; }

  Address Serves(const Fetch &request) const noexcept override { return request.Where(); }

  Ticket Begin(const Address &, Transport &) const override { return Ticket::None; }

  Fetched Collect(const Address &at, Ticket, Transport &) const override {
    ++Calls;
    return Fetched::Delivered({static_cast<uint8_t>(*at.Index() & 0xFFu)});
  }
};

}

int main() {
  using namespace outshine::Test;
  using outshine::Ground::TilePool;
  ContentStore store({.Using = ContentStore::Use::Off});
  SourceSet sources(store);
  NoTransport transport;
  auto source = std::make_unique<CountingSource>();
  const CountingSource *const counted = source.get();
  CHECK(sources.Add(std::move(source)) == SourceSet::Registration::Accepted, "source registered");
  TilePool pool({.ByteBudget = 4096}, sources, transport);
  const size_t empty = pool.ByteCacheBytes();
  constexpr uint64_t requests = 100;
  for (uint64_t at = 0; at < requests; ++at) {
    TilePool::Landing landing;
    CHECK(pool.BytesBlocking(Fetch(DataKind::Elevation, Address::Whole(at)), &landing) ==
                  TilePool::Reply::Ready &&
              landing.Bytes == std::vector<uint8_t>{static_cast<uint8_t>(at & 0xFFu)},
          "each distinct request enters the byte cache with its payload");
  }
  const size_t warm = pool.ByteCacheBytes();
  CHECK(warm > empty && counted->Calls == requests,
        "cache and its index grow once for each distinct request");
  for (uint64_t at = 0; at < requests; ++at) {
    TilePool::Landing landing;
    CHECK(pool.BytesBlocking(Fetch(DataKind::Elevation, Address::Whole(at)), &landing) ==
              TilePool::Reply::Ready,
          "cached request returns its retained payload");
  }
  CHECK(pool.ByteCacheBytes() == warm && counted->Calls == requests,
        "duplicate requests allocate neither cache payload nor index storage");
  return Report();
}
