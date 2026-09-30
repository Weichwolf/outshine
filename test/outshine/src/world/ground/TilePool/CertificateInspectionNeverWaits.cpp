#include "TilePool.h"
#include "ContentStore.h"
#include "SourceSet.h"
#include <world/data/Transport.h>
#include "Check.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <utility>
#include <chrono>
#include <cstdlib>
#include <future>
#include <latch>
#include <memory>
#include <mutex>
#include <new>
#include <thread>

namespace outshine::Ground {
struct TerrainInspectionTestPeer {
  static std::array<std::mutex *, 3> Mutexes(TilePool &pool) {
    return {&pool.QueueMutex_, &pool.CacheMutex_, &pool.TerrainRevisions_->Mutex_};
  }

  static std::mutex &Mutex(TerrainRevisionIndex &index) { return index.Mutex_; }
};
}

namespace {
using namespace outshine;
using namespace outshine::Ground;
using namespace outshine::Data;
using Validation = TerrainCertificate::Validation;
thread_local bool measureAllocations = false;
thread_local size_t allocations = 0;

class NoNetwork final : public Transport {
public:
  FetchStart Begin(const std::string &) override { return Ticket::None; }

  Wire Collect(Ticket) override { return Wire::Never(); }

  void Cancel(Ticket) override {}
};

class Provider final : public Source {
public:
  SourceDecl Decl{.Id = "inspection-dem", .Keeps = Cacheability::Never};
  std::atomic<uint8_t> Content{7};

  const SourceDecl &Declaration() const noexcept override { return Decl; }

  Coverage Covers(const Fetch &) const noexcept override { return Coverage::Inside; }

  Address Serves(const Fetch &request) const noexcept override { return request.Where(); }

  FetchStart Begin(const Address &, Transport &) const override { return Ticket::None; }

  Fetched Collect(const Address &, Ticket, Transport &) const override {
    return Fetched::Delivered({42, Content.load()});
  }
};

struct Inspection {
  Validation State;
  size_t Allocations;
};

Inspection Inspect(const TilePool &pool, const TerrainCertificate &certificate) {
  allocations = 0;
  measureAllocations = true;
  const auto state = pool.InspectCertificate(certificate);
  measureAllocations = false;
  return {state, allocations};
}
}

void *operator new(size_t bytes) {
  if (measureAllocations) { ++allocations; }
  if (void *memory = std::malloc(bytes == 0 ? 1 : bytes)) { return memory; }
  std::abort();
}

void operator delete(void *memory) noexcept {
  std::free(memory);
}

void operator delete(void *memory, size_t) noexcept {
  std::free(memory);
}

int main() {
  using namespace outshine::Test;
  const Data::TileId a{.Zoom = 2, .X = 0, .Y = 1};
  const Data::TileId b{.Zoom = 2, .X = 1, .Y = 1};
  const Data::TileId c{.Zoom = 2, .X = 2, .Y = 1};
  ContentStore store({.Using = ContentStore::Use::Off});
  SourceSet sources(store);
  NoNetwork transport;
  auto provider = std::make_unique<Provider>();
  Provider *const observed = provider.get();
  CHECK(sources.Add(std::move(provider)) == SourceSet::Registration::Accepted,
        "local provider registers");
  TilePool pool({.Threads = 1, .ByteBudget = 2, .TerrainRevisionEntries = 2, .Carriers = 1},
                sources,
                transport);
  const auto deliver = [&](Data::TileId at) {
    TilePool::Landing landing;
    CHECK(pool.BytesBlocking(Fetch(DataKind::Elevation, Address::At(at)), &landing) ==
                  TilePool::Reply::Ready &&
              landing.TerrainStamp,
          "actual terrain delivery supplies its stamp");
    return TerrainCertificate::FromDelivery(at, landing.TerrainStamp, pool.TerrainScopeRevision());
  };
  const auto certificate = deliver(a);
  auto inspected = Inspect(pool, certificate);
  CHECK(inspected.State == Validation::Current && inspected.Allocations == 0,
        "current certificate inspection allocates nothing");

  for (std::mutex *mutex : TerrainInspectionTestPeer::Mutexes(pool)) {
    std::latch held(1), release(1);
    std::thread writer([&] {
      const std::scoped_lock lock(*mutex);
      held.count_down();
      release.wait();
    });
    held.wait();
    std::promise<Inspection> completion;
    auto result = completion.get_future();
    std::thread reader([&] { completion.set_value(Inspect(pool, certificate)); });
    const bool beforeRelease =
        result.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    release.count_down();
    writer.join();
    reader.join();
    inspected = result.get();
    CHECK(beforeRelease && inspected.State == Validation::Pending && inspected.Allocations == 0,
          "inspection reports Pending before the held queue/cache/index writer releases");
    CHECK(pool.InspectCertificate(certificate) == Validation::Current,
          "same certificate becomes current after contention without source changes");
  }

  (void)deliver(b);
  const auto identical = deliver(a);
  CHECK(identical.Dependencies().size() == 1 && certificate.Dependencies().size() == 1 &&
            identical.Dependencies().front() == certificate.Dependencies().front(),
        "identical bytes retain provenance after raw cache eviction");
  inspected = Inspect(pool, certificate);
  CHECK(inspected.State == Validation::Current && inspected.Allocations == 0,
        "identical redelivery keeps the old certificate current without allocation");
  (void)deliver(b);
  observed->Content.store(8);
  const auto later = deliver(a);
  CHECK(later.Dependencies().size() == 1 && certificate.Dependencies().size() == 1 &&
            later.Dependencies().front() != certificate.Dependencies().front(),
        "changed source bytes after raw eviction produce a genuinely new stamp");
  inspected = Inspect(pool, certificate);
  CHECK(inspected.State == Validation::Stale && inspected.Allocations == 0,
        "a fresh delivery invalidates old stamps without allocation");
  CHECK(pool.InspectCertificate(later) == Validation::Current, "new delivery is current");
  (void)deliver(b);
  (void)deliver(c);
  inspected = Inspect(pool, later);
  CHECK(inspected.State == Validation::Unknown && inspected.Allocations == 0,
        "evicted metadata is Unknown rather than known stale");
  TerrainCertificate incomplete;
  inspected = Inspect(pool, incomplete);
  CHECK(inspected.State == Validation::Unknown && inspected.Allocations == 0,
        "incomplete certificate cannot report Current");
  auto other = TerrainRevisionIndex::Create();
  CHECK(other.has_value(), "independent owner domain exists");
  if (!other) { return Report(); }
  const auto foreignStamp = (**other).IssueDeliveryStamp(a);
  CHECK(foreignStamp.has_value(), "independent delivery is stamped");
  const auto foreign =
      TerrainCertificate::FromDelivery(a, *foreignStamp, pool.TerrainScopeRevision());
  inspected = Inspect(pool, foreign);
  CHECK(inspected.State == Validation::Stale && inspected.Allocations == 0,
        "another owner domain cannot validate the certificate");
  pool.Shapes({.Kind = "flat"});
  inspected = Inspect(pool, later);
  CHECK(inspected.State == Validation::ScopeChanged && inspected.Allocations == 0,
        "scope change is distinct from metadata loss or delivery replacement");
  const auto unscoped = TerrainCertificate::FromDelivery(a, *foreignStamp);
  CHECK(pool.InspectCertificate(unscoped) == Validation::Unknown,
        "unknown shape scope cannot grant Current");

  const std::array stamps{*foreignStamp};
  CHECK((**other).TryInspectStamps(stamps) == TerrainRevisionIndex::Validation::Current,
        "index try-inspection agrees with blocking inspection");
  CHECK((**other).TryInspectStamps({}) == TerrainRevisionIndex::Validation::Unknown,
        "empty stamps retain Unknown classification");
  std::latch held(1), release(1);
  std::thread writer([&] {
    const std::scoped_lock lock(TerrainInspectionTestPeer::Mutex(**other));
    held.count_down();
    release.wait();
  });
  held.wait();
  std::promise<std::optional<TerrainRevisionIndex::Validation>> completion;
  auto result = completion.get_future();
  std::thread reader([&] { completion.set_value((**other).TryInspectStamps(stamps)); });
  const bool beforeRelease = result.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
  release.count_down();
  writer.join();
  reader.join();
  CHECK(beforeRelease && !result.get(), "direct index contention returns absence before release");
  return Report();
}
