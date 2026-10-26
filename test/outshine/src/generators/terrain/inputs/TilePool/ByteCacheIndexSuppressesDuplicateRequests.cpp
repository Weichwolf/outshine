#include "TilePool.h"
#include "SourceSet.h"
#include "Check.h"

#include <cstdint>
#include <cstddef>
#include <utility>
#include <memory>
#include <limits>
#include <string>
#include <vector>

namespace {
constexpr uint64_t kByteMask = 0xFFu;
using namespace outshine::Data;

class NoTransport final : public SourceTransport {
public:
  FetchStart Begin(const std::string & /*url*/) override { return Ticket::None; }

  Wire Collect(Ticket /*ticket*/) override { return Wire::Never(); }

  void Cancel(Ticket /*ticket*/) override {}
};

class CountingSource final : public Source {
public:
  CountingSource() {
    Decl.Id = "counting";
    Decl.Revision = "test-r1";
    Decl.Keeps = Cacheability::Never;
  }

  SourceDecl Decl;
  mutable size_t Calls = 0;

  const SourceDecl &Declaration() const noexcept override { return Decl; }

  Coverage Covers(const SourceRequest & /*request*/) const noexcept override {
    return Coverage::Inside;
  }

  Address Serves(const SourceRequest &request) const noexcept override { return request.Where(); }

  FetchStart Begin(const Address & /*at*/, SourceTransport & /*transport*/) const override {
    return Ticket::None;
  }

  SourceResponse
  Collect(const Address &at, Ticket /*ticket*/, SourceTransport & /*transport*/) const override {
    const auto index = at.Index();
    if (!index) { return SourceResponse::Meant(Meaning::Refused); }
    ++Calls;
    return SourceResponse::Delivered({static_cast<uint8_t>(*index & kByteMask)});
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
  NoTransport transport;
  auto source = std::make_unique<CountingSource>();
  const CountingSource *const counted = source.get();
  CHECK(sources.Add(std::move(source)) == SourceSet::Registration::Accepted, "source registered");
  TilePool::Config config;
  config.ByteBudget = 4096;
  TilePool pool(config, sources, transport);
  const size_t empty = pool.ByteCacheBytes();
  constexpr uint64_t requests = 100;
  for (uint64_t at = 0; at < requests; ++at) {
    TilePool::Landing landing;
    CHECK(pool.BytesBlocking(SourceRequest(DataKind::Elevation, Address::Whole(at)), &landing) ==
                  TilePool::Reply::Ready &&
              landing.Bytes == std::vector<uint8_t>{static_cast<uint8_t>(at & kByteMask)},
          "each distinct request enters the byte cache with its payload");
  }
  const size_t warm = pool.ByteCacheBytes();
  CHECK(warm > empty && counted->Calls == requests,
        "cache and its index grow once for each distinct request");
  for (uint64_t at = 0; at < requests; ++at) {
    TilePool::Landing landing;
    CHECK(pool.BytesBlocking(SourceRequest(DataKind::Elevation, Address::Whole(at)), &landing) ==
              TilePool::Reply::Ready,
          "cached request returns its retained payload");
  }
  CHECK(pool.ByteCacheBytes() == warm && counted->Calls == requests,
        "duplicate requests allocate neither cache payload nor index storage");
  TilePool::Landing pinned;
  const SourceRequest recent(DataKind::Elevation, Address::Whole(0));
  CHECK(pool.BytesBlocking(recent, &pinned) == TilePool::Reply::Ready,
        "a retained caller delivery marks its cache entry most recently used");
  CHECK(pool.EvictSourceBytes(0) == 0 && pool.ByteCacheBytes() == warm,
        "zero memory pressure preserves the resident cache");
  CHECK(pool.EvictSourceBytes(1) == 1 && pool.ByteCacheBytes() < warm,
        "pressure releases the oldest source payload and its index entry");
  TilePool::Landing landing;
  CHECK(pool.BytesBlocking(recent, &landing) == TilePool::Reply::Ready &&
            counted->Calls == requests && pinned.Bytes == landing.Bytes,
        "the recently used payload remains resident and caller bytes stay owned");
  CHECK(pool.BytesBlocking(SourceRequest(DataKind::Elevation, Address::Whole(1)), &landing) ==
                TilePool::Reply::Ready &&
            counted->Calls == requests + 1 && landing.Bytes == std::vector<uint8_t>{1},
        "an evicted source can be acquired again with intact address and bytes");
  CHECK(pool.EvictSourceBytes(std::numeric_limits<size_t>::max()) == requests &&
            pool.EvictSourceBytes(1) == 0 && pinned.Bytes == std::vector<uint8_t>{0},
        "full eviction is bounded by actual payloads and never invalidates a caller delivery");
  return Report();
}
