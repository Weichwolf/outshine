#include "NativeBasisLookupFixture.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <semaphore>
#include <thread>

namespace {
using namespace NativeBasisFixture;

void QueuedClear() {
  Fixture env;
  if (!Open(env)) { return; }
  const auto cache = env.Stack.BuildingAssets();
  const auto before = cache->Costs();
  WorkerBlock held;
  CHECK(held.Start(env.Worker), "the compute worker is held before lookup");
  const auto pending = Query(env.Queue, env);
  CHECK(pending && pending->Where == StructureBuildQueue::NativeBasisQuery::Phase::Pending,
        "the actual queue has a pending native basis lookup");
  std::binary_semaphore cleared(0);
  std::thread clear([&] {
    env.Queue.Clear();
    cleared.release();
  });
  const bool returned = cleared.try_acquire_for(std::chrono::milliseconds(250));
  CHECK(returned, "clearing native lookups does not wait for the held worker");
  if (!returned) { held.Release(); }
  clear.join();
  Fixture next;
  const bool nextOpened = Open(next);
  if (!nextOpened) {
    held.Release();
    return;
  }
  const auto nextCache = next.Stack.BuildingAssets();
  const auto nextBefore = nextCache->Costs();
  const auto replacement = Query(env.Queue, next);
  CHECK(replacement && replacement->Where == StructureBuildQueue::NativeBasisQuery::Phase::Pending,
        "clear accepts a different cache independently of the retired job");
  held.Release();
  Barrier(env.Worker);
  const auto ready = Query(env.Queue, next);
  CHECK(ready && ready->Where == StructureBuildQueue::NativeBasisQuery::Phase::Ready &&
            ready->Source && replacement && ready->RequestKey == replacement->RequestKey,
        "fresh demand publishes its own native basis after old work retires");
  const auto after = cache->Costs();
  CHECK(after.BasisHits == before.BasisHits && after.BasisWrites == before.BasisWrites,
        "canceled queued lookup does no read or generation in the old cache");
  CHECK(nextCache->Costs().BasisHits == nextBefore.BasisHits + 1,
        "new demand reads exactly once from its own independent cache");
  env.Queue.Clear();
  Barrier(env.Worker);
}

void ReadyClear(bool release) {
  Fixture env;
  if (!Open(env)) { return; }
  const std::weak_ptr<Generators::PreparedBuildingAssets> lifetime = env.Stack.BuildingAssets();
  const auto pending = Query(env.Queue, env);
  CHECK(pending && pending->Where == StructureBuildQueue::NativeBasisQuery::Phase::Pending,
        "the ready case starts a real pending native lookup");
  Barrier(env.Worker);
  const auto ready = Query(env.Queue, env);
  CHECK(ready && ready->Where == StructureBuildQueue::NativeBasisQuery::Phase::Ready,
        "a real ready basis is retained before cleanup");
  if (release && ready) { env.Queue.ReleaseNativeBasis(ready->RequestKey); }
  WorkerBlock held;
  CHECK(held.Start(env.Worker), "the worker is held before ready cache cleanup");
  env.Queue.Clear();
  env.Stack.Close();
  CHECK(!lifetime.expired(), "ready cache and lookup cleanup stay owned off the caller");
  held.Release();
  Barrier(env.Worker);
  CHECK(lifetime.expired(), "worker cleanup releases the last prepared cache owner");
}

void Discard() {
  Fixture env;
  if (!Open(env)) { return; }
  const std::weak_ptr<Generators::PreparedBuildingAssets> lifetime = env.Stack.BuildingAssets();
  auto pool = std::make_unique<Tasks>(1);
  auto queue = std::make_unique<StructureBuildQueue>();
  queue->Opens(pool.get(), nullptr);
  WorkerBlock held;
  CHECK(held.Start(*pool), "the worker is held before discard");
  const auto pending = Query(*queue, env);
  CHECK(pending && pending->Where == StructureBuildQueue::NativeBasisQuery::Phase::Pending,
        "discard starts with a queued native lookup");
  std::binary_semaphore retired(0);
  std::thread clear([&] {
    queue.reset();
    retired.release();
  });
  const bool returned = retired.try_acquire_for(std::chrono::milliseconds(250));
  CHECK(returned, "destruction does not join the queued native lookup");
  if (!returned) { held.Release(); }
  clear.join();
  env.Stack.Close();
  if (!returned) { return; }
  std::atomic_bool stopped{false};
  std::thread stop([&] {
    pool.reset();
    stopped = true;
  });
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  while (!lifetime.expired() && std::chrono::steady_clock::now() < until) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(lifetime.expired() && !stopped,
        "queue discard releases retained cache before joining its blocked worker");
  held.Release();
  stop.join();
}

void ConcurrentClear() {
  Fixture env;
  if (!Open(env)) { return; }
  Tasks pool(2);
  StructureBuildQueue queue;
  queue.Opens(&pool, nullptr);
  for (int iteration = 0; iteration < 96; ++iteration) {
    const auto query = Query(queue, env);
    CHECK(query && query->Where == StructureBuildQueue::NativeBasisQuery::Phase::Pending,
          "each new generation starts an independent actual cache job");
    std::this_thread::yield();
    queue.Clear();
  }
  const auto pending = Query(queue, env);
  CHECK(pending && pending->Where == StructureBuildQueue::NativeBasisQuery::Phase::Pending,
        "a final lookup follows concurrent retirement");
  Barrier(pool);
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  while (std::chrono::steady_clock::now() < until) {
    const auto query = Query(queue, env);
    if (query && query->Where == StructureBuildQueue::NativeBasisQuery::Phase::Ready) { return; }
    (void)queue.AwaitSlice(0.01);
  }
  CHECK(false, "a final live native basis survives concurrent replacements");
}
}

int main() {
  QueuedClear();
  ReadyClear(false);
  ReadyClear(true);
  Discard();
  ConcurrentClear();
  return outshine::Test::Report();
}
