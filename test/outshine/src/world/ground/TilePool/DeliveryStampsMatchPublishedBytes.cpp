#include "TilePool.h"
#include "TerrainDelivery.h"
#include "ContentStore.h"
#include "SourceSet.h"
#include <world/data/Transport.h>
#include "Check.h"
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace {
using namespace outshine;
using namespace outshine::Data;
using namespace outshine::Ground;

class NoNetwork final : public Transport {
public:
  FetchStart Begin(const std::string &) override { return Ticket::None; }

  Wire Collect(Ticket) override { return Wire::Never(); }

  void Cancel(Ticket) override {}
};

struct FirstDeliveryGate {
  std::mutex Mutex;
  std::condition_variable Changed;
  bool Entered = false, Released = false, TimedOut = false;

  void Pause() {
    std::unique_lock lock(Mutex);
    Entered = true;
    Changed.notify_all();
    TimedOut = !Changed.wait_for(lock, std::chrono::seconds(5), [&] { return Released; });
  }

  bool Wait() {
    std::unique_lock lock(Mutex);
    return Changed.wait_for(lock, std::chrono::seconds(1), [&] { return Entered; });
  }

  void Release() {
    const std::scoped_lock lock(Mutex);
    Released = true;
    Changed.notify_all();
  }
};

class Provider final : public Source {
public:
  SourceDecl Decl{.Id = "revision-dem", .Keeps = Cacheability::Never};
  mutable std::atomic<unsigned> Calls = 0;
  std::atomic<bool> Absent = false;
  std::atomic<bool> Stable = false;
  std::atomic<bool> ShiftServed = false;
  std::shared_ptr<FirstDeliveryGate> Gate;

  const SourceDecl &Declaration() const noexcept override { return Decl; }

  Coverage Covers(const Fetch &) const noexcept override { return Coverage::Inside; }

  Address Serves(const Fetch &request) const noexcept override {
    if (ShiftServed && request.Where().Tile()) {
      auto tile = *request.Where().Tile();
      tile.X ^= 1u;
      return Address::At(tile);
    }
    return request.Where();
  }

  FetchStart Begin(const Address &, Transport &) const override { return Ticket::None; }

  Fetched Collect(const Address &, Ticket, Transport &) const override {
    const auto serial = ++Calls;
    if (serial == 1 && Gate) { Gate->Pause(); }
    if (Absent) { return Fetched::NotFound(); }
    return Fetched::Delivered({Stable ? uint8_t{7} : static_cast<uint8_t>(serial), 42});
  }
};
}

int main() {
  using namespace outshine::Test;
  const Data::TileId a{.Zoom = 2, .X = 0, .Y = 1};
  const Data::TileId b{.Zoom = 2, .X = 1, .Y = 1};
  const Fetch askA(DataKind::Elevation, Address::At(a));
  const Fetch askB(DataKind::Elevation, Address::At(b));
  for (const bool evictMetadata : {false, true}) {
    ContentStore store({.Using = ContentStore::Use::Off});
    SourceSet sources(store);
    NoNetwork transport;
    auto provider = std::make_unique<Provider>();
    auto *observed = provider.get();
    CHECK(sources.Add(std::move(provider)) == SourceSet::Registration::Accepted,
          "provider registers");
    TilePool pool(
        {.ByteBudget = evictMetadata ? 64u : 2u, .TerrainRevisionEntries = evictMetadata ? 1u : 8u},
        sources,
        transport);
    TilePool::Landing first, cached, neighbour, next;
    CHECK(pool.BytesBlocking(askA, &first) == TilePool::Reply::Ready, "first delivery arrives");
    CHECK(first.TerrainStamp.has_value(), "delivery carries its requested-address stamp");
    if (!first.TerrainStamp) { continue; }
    const auto original = *first.TerrainStamp;
    CHECK(original.Requested == a, "stamp belongs to requested address");
    CHECK(pool.BytesBlocking(askA, &cached) == TilePool::Reply::Ready, "raw cache serves bytes");
    CHECK(cached.Bytes == first.Bytes && cached.TerrainStamp == first.TerrainStamp,
          "raw hit keeps bytes and stamp paired");
    CHECK(observed->Calls == 1, "raw hit performs no provider work");
    CHECK(pool.BytesBlocking(askB, &neighbour) == TilePool::Reply::Ready, "neighbour arrives");
    CHECK(pool.ValidTerrainStamps(std::array{original}) != evictMetadata,
          "only metadata eviction invalidates the first certificate");
    CHECK(pool.BytesBlocking(askA, &next) == TilePool::Reply::Ready, "first address returns");
    CHECK(next.TerrainStamp && next.TerrainStamp != first.TerrainStamp,
          "re-entry or fresh delivery receives a new stamp");
    CHECK(!pool.ValidTerrainStamps(std::array{original}), "older certificate stays revoked");
    CHECK(observed->Calls == (evictMetadata ? 2u : 3u), "retained bytes re-certify without IO");
    CHECK((next.Bytes == first.Bytes) == evictMetadata, "fresh bytes replace evicted bytes");
    auto payload = FromTerrainDelivery(askA, next).Take();
    CHECK(payload && payload->Stamp == next.TerrainStamp &&
              std::get<std::vector<uint8_t>>(payload->Samples) == next.Bytes,
          "terrain adapter preserves exact byte/stamp pair");
    CHECK(pool.TerrainMetadataBytes() > 0, "metadata storage is accounted separately");
    if (!evictMetadata && next.TerrainStamp) {
      const auto priorAbsence = *next.TerrainStamp;
      CHECK(pool.BytesBlocking(askB, &neighbour) == TilePool::Reply::Ready,
            "raw eviction releases address");
      observed->Absent = true;
      CHECK(pool.BytesBlocking(askA, &next) == TilePool::Reply::Absent, "explicit absence arrives");
      CHECK(!pool.ValidTerrainStamps(std::array{priorAbsence}),
            "explicit absence revokes prior bytes");
      CHECK(next.Bytes.empty(), "absent delivery has no stale payload");
    }
  }
  {
    ContentStore store({.Using = ContentStore::Use::Off});
    SourceSet sources(store);
    NoNetwork transport;
    auto provider = std::make_unique<Provider>();
    auto gate = std::make_shared<FirstDeliveryGate>();
    provider->Gate = gate;
    CHECK(sources.Add(std::move(provider)) == SourceSet::Registration::Accepted,
          "concurrent provider registers");
    TilePool pool({.ByteBudget = 64}, sources, transport);
    TilePool::Landing delayed, earlier, cached;
    TilePool::Reply delayedReply = TilePool::Reply::Pending;
    std::thread first([&] { delayedReply = pool.BytesBlocking(askA, &delayed); });
    CHECK(gate->Wait(), "first delivery reaches the controlled barrier");
    CHECK(pool.BytesBlocking(askA, &earlier) == TilePool::Reply::Ready,
          "independent delivery publishes first");
    gate->Release();
    first.join();
    CHECK(!gate->TimedOut && delayedReply == TilePool::Reply::Ready,
          "delayed delivery publishes without timeout");
    CHECK(pool.BytesBlocking(askA, &cached) == TilePool::Reply::Ready,
          "latest publication is cached");
    CHECK(cached.Bytes == delayed.Bytes && cached.TerrainStamp == delayed.TerrainStamp,
          "fresh publication replaces existing bytes and stamp together");
    CHECK(earlier.TerrainStamp && !pool.ValidTerrainStamps(std::array{*earlier.TerrainStamp}),
          "replaced delivery cannot certify current terrain");
    CHECK(delayed.TerrainStamp && pool.ValidTerrainStamps(std::array{*delayed.TerrainStamp}),
          "latest published bytes retain their exact stamp");
  }
  {
    ContentStore store({.Using = ContentStore::Use::Off});
    SourceSet sources(store);
    NoNetwork transport;
    auto provider = std::make_unique<Provider>();
    auto *observed = provider.get();
    observed->Stable = true;
    CHECK(sources.Add(std::move(provider)) == SourceSet::Registration::Accepted,
          "stable terrain provider registers");
    TilePool pool({.ByteBudget = 2, .TerrainRevisionEntries = 8}, sources, transport);
    TilePool::Landing first, neighbour, again, relocated;
    CHECK(pool.BytesBlocking(askA, &first) == TilePool::Reply::Ready && first.TerrainStamp,
          "first immutable payload has a certificate");
    CHECK(pool.BytesBlocking(askB, &neighbour) == TilePool::Reply::Ready &&
              pool.BytesBlocking(askA, &again) == TilePool::Reply::Ready,
          "evicted bytes are delivered again");
    CHECK(observed->Calls == 3 && first.Bytes == again.Bytes &&
              first.TerrainStamp == again.TerrainStamp,
          "identical re-delivery preserves certificates held by in-flight world candidates");
    CHECK(first.TerrainStamp && pool.ValidTerrainStamps(std::array{*first.TerrainStamp}),
          "candidate can continue after raw-byte cache eviction and identical refill");
    CHECK(pool.BytesBlocking(askB, &neighbour) == TilePool::Reply::Ready,
          "evict the first address before changing served provenance");
    observed->ShiftServed = true;
    CHECK(pool.BytesBlocking(askA, &relocated) == TilePool::Reply::Ready &&
              relocated.Bytes == first.Bytes && relocated.At != first.At,
          "negative control delivers equal bytes from a different source address");
    CHECK(relocated.TerrainStamp != first.TerrainStamp && first.TerrainStamp &&
              !pool.ValidTerrainStamps(std::array{*first.TerrainStamp}),
          "equal bytes cannot hide changed terrain provenance");
  }
  return Report();
}
