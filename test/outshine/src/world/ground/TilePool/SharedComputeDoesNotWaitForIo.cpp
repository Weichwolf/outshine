#include "Check.h"
#include "ContentStore.h"
#include "SourceSet.h"
#include "Tasks.h"
#include "TilePool.h"
#include "TerrainLoader.h"

#include <world/data/Source.h>
#include <world/data/Transport.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <semaphore>
#include <thread>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Data;
using namespace std::chrono_literals;

class SlowWire final : public Transport {
public:
  std::atomic<unsigned> Began{0};
  std::atomic<unsigned> Cancelled{0};

  FetchStart Begin(const std::string &) override { return Ticket{++Began}; }

  Wire Collect(Ticket) override { return Wire::Working(); }

  void Cancel(Ticket) override { ++Cancelled; }
};

class SlowElevation final : public Source {
public:
  SourceDecl Decl;

  SlowElevation() {
    Decl.Id = "stalled-original-elevation";
    Decl.MaxZoom = 15;
    Decl.Keeps = Cacheability::Never;
  }

  const SourceDecl &Declaration() const noexcept override { return Decl; }

  Coverage Covers(const Fetch &request) const noexcept override {
    return request.Kind() == DataKind::Elevation && request.Where().Tile() ? Coverage::Inside
                                                                           : Coverage::Outside;
  }

  Address Serves(const Fetch &request) const noexcept override { return request.Where(); }

  FetchStart Begin(const Address &, Transport &transport) const override {
    return transport.Begin("test://pending-raster");
  }

  Fetched Collect(const Address &, Ticket ticket, Transport &transport) const override {
    (void)transport.Collect(ticket);
    return Fetched::Working();
  }
};

bool AwaitDone(Tasks &compute, Tasks::Handle handle) {
  const auto deadline = std::chrono::steady_clock::now() + 3s;
  while (std::chrono::steady_clock::now() < deadline) {
    if (compute.TakeCompletion(handle)) { return true; }
    (void)compute.AwaitCompletion(0.01);
  }
  return false;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  CHECK(Tasks::ComputeThreads() == 1, "the client compute budget is one worker");
  const auto policy = GroundPoolConfig({.LongitudeDeg = 8.0, .LatitudeDeg = 47.0});
  CHECK(policy && policy->Threads == 1 && policy->Carriers > policy->Threads,
        "default parallel IO is independent of the single compute budget");
  Tasks compute(1);
  {
    Data::ContentStore store({.Using = Data::ContentStore::Use::Off});
    Data::SourceSet sources(store);
    SlowWire wire;
    TilePool first({.Threads = 4, .Compute = &compute}, sources, wire);
    TilePool second({.Threads = 4, .Compute = &compute}, sources, wire);
    first.Shapes({.Kind = "plane", .AmplitudeM = 10.0});
    second.Shapes({.Kind = "plane", .AmplitudeM = 20.0});
    CHECK(first.ThreadCount() == 1 && second.ThreadCount() == 1,
          "borrowers do not create the requested private four-worker pools");
    std::binary_semaphore entered(0), release(0);
    const auto blocker = compute.Post([&] {
      entered.release();
      release.acquire();
    });
    CHECK(entered.try_acquire_for(3s), "shared worker enters the controlled job");
    constexpr TileId tile{.Zoom = 14, .X = 8192, .Y = 8192};
    CHECK(first.Wants(tile, 4) == TilePool::Reply::Pending &&
              second.Wants(tile, 4) == TilePool::Reply::Pending,
          "both terrain jobs queue behind work on the same executor");
    for (uint32_t offset = 1; offset <= 8; ++offset) {
      CHECK(first.Wants({.Zoom = tile.Zoom, .X = tile.X + offset, .Y = tile.Y}, 4) ==
                TilePool::Reply::Pending,
            "terrain backlog is admitted without starting private compute");
    }
    CHECK(first.Counters().MeshTiles == 0 && second.Counters().MeshTiles == 0,
          "blocking the shared worker blocks all terrain processing");
    std::atomic<long long> terrainBeforeOtherJob{-1};
    const auto other = compute.Post(
        [&] { terrainBeforeOtherJob = first.Counters().MeshTiles + second.Counters().MeshTiles; });
    release.release();
    CHECK(AwaitDone(compute, blocker) && AwaitDone(compute, other),
          "finite terrain jobs give the shared worker back to other world producers");
    CHECK(terrainBeforeOtherJob >= 0 && terrainBeforeOtherJob <= 2,
          "one terrain backlog does not monopolize the executor");
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (first.Wants(tile, 4) == TilePool::Reply::Pending &&
           std::chrono::steady_clock::now() < deadline) {
      (void)first.AwaitLanding(0.01);
    }
    CHECK(first.Wants(tile, 4) == TilePool::Reply::Ready,
          "the same shared terrain path produces real mesh output");
  }
  {
    Data::ContentStore store({.Using = Data::ContentStore::Use::Off});
    Data::SourceSet sources(store);
    CHECK(sources.Add(std::make_unique<SlowElevation>()) == Data::SourceSet::Registration::Accepted,
          "slow raster source registers");
    SlowWire wire;
    {
      TilePool pool({.Compute = &compute, .Carriers = 2}, sources, wire);
      std::shared_ptr<const TerrainField> field;
      CHECK(pool.Field({.Zoom = 14, .X = 8192, .Y = 8192}, &field) == TilePool::Reply::Pending,
            "terrain transformation requests unavailable input");
      const auto deadline = std::chrono::steady_clock::now() + 3s;
      while (wire.Began == 0 && std::chrono::steady_clock::now() < deadline) {
        (void)pool.AwaitLanding(0.01);
      }
      CHECK(wire.Began > 0, "IO is actually pending before the compute liveness check");
      const auto independent = compute.Post([] {});
      CHECK(AwaitDone(compute, independent) && pool.Counters().FetchGaveUp == 0,
            "stalled input parks terrain while independent compute continues");
      const auto partial = pool.Counters();
      CHECK(partial.FieldAttempts > 0 && partial.FieldCpuMs > 0.0 && partial.FieldTiles == 0,
            "parked terrain accounts for executed work without claiming a finished field");
      CHECK(pool.Counters().FetchOnCompute == 0, "network acquisition never moves onto compute");
    }
    CHECK(wire.Cancelled == wire.Began, "shutdown cancels every pending source transfer");
  }
  for (unsigned at = 0; at < 100; ++at) {
    CHECK(compute.PostDetached([] {}), "untracked terrain-style job is admitted");
  }
  CHECK(AwaitDone(compute, compute.Post([] {})), "tracked fence follows every untracked job");
  CHECK(!compute.AwaitCompletion(0.01), "untracked jobs leave no unconsumed completion records");
  return Report();
}
