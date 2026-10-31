#include "PreparationFixture.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <semaphore>
#include <thread>

namespace {
using namespace PreparationFixture;

void Retire(bool ready) {
  Tasks pool(1);
  auto source = Source();
  std::weak_ptr<const void> lifetime = source;
  WorkerBlock held;
  if (!ready) { CHECK(held.Start(pool), "the worker is held before preparation"); }
  auto preparation = std::make_unique<StructurePreparation>(
      pool, StructurePreparation::SourceInputs{source}, kDefaultStructurePolicy, kHeightZoom);
  source.reset();
  if (ready) {
    Barrier(pool);
    CHECK(preparation->Poll() == StructurePreparation::Phase::Ready,
          "ready input has a real building product before retirement");
    const auto products = preparation->Products();
    CHECK(products.size() == 1, "the ready preparation exposes its owned native product");
    if (products.size() == 1) { lifetime = products.front().Input; }
    CHECK(held.Start(pool), "the worker is held before cleanup of a ready product");
  }
  std::binary_semaphore retired(0);
  std::thread release([&] {
    preparation.reset();
    retired.release();
  });
  CHECK(retired.try_acquire_for(std::chrono::milliseconds(250)),
        "queued and ready preparation can retire before the worker is released");
  CHECK(!lifetime.expired(),
        "work retains its source and ready cleanup retains its native product");
  held.Release();
  release.join();
  Barrier(pool);
  CHECK(lifetime.expired(),
        "worker cleanup releases the last owned input/product after retirement");
}

void Discard() {
  auto pool = std::make_unique<Tasks>(1);
  WorkerBlock held;
  CHECK(held.Start(*pool), "the worker is held before queue discard");
  auto source = Source();
  const std::weak_ptr<const SourceSnapshot> lifetime = source;
  auto preparation = std::make_unique<StructurePreparation>(
      *pool, StructurePreparation::SourceInputs{source}, kDefaultStructurePolicy, kHeightZoom);
  source.reset();
  std::binary_semaphore retired(0);
  std::thread release([&] {
    preparation.reset();
    retired.release();
  });
  const bool returned = retired.try_acquire_for(std::chrono::milliseconds(250));
  CHECK(returned, "frontend retirement does not wait for queued generation");
  if (!returned) { held.Release(); }
  release.join();
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
        "discarded jobs release source ownership before joining the held worker");
  held.Release();
  stop.join();
  CHECK(stopped, "shutdown finishes after its running blocker is released");
}

void Completion() {
  Tasks pool(1);
  const auto unrelated = pool.Post([] {});
  CHECK(pool.AwaitCompletion(unrelated, 1), "an unrelated completion remains retained");
  WorkerBlock held;
  CHECK(held.Start(pool), "the worker is held before building preparation");
  StructurePreparation preparation(
      pool, StructurePreparation::SourceInputs{Source()}, kDefaultStructurePolicy, kHeightZoom);
  CHECK(!preparation.AwaitSlice(0) && !preparation.AwaitSlice(0.002),
        "a retained unrelated handle cannot satisfy the preparation wait");
  std::binary_semaphore entered(0);
  std::binary_semaphore finished(0);
  std::atomic_bool progress{false};
  std::thread waiter([&] {
    entered.release();
    progress = preparation.AwaitSlice(1);
    finished.release();
  });
  CHECK(entered.try_acquire_for(kDeadline), "the waiter starts before its job can execute");
  CHECK(!finished.try_acquire_for(std::chrono::milliseconds(1)),
        "the waiter cannot wake from the unrelated completion");
  held.Release();
  waiter.join();
  CHECK(progress, "the actual detached job completes its own waiter");
  Barrier(pool);
  CHECK(preparation.AwaitSlice(1) && preparation.Running(),
        "completed result stays observable until the caller polls it");
  CHECK(preparation.Poll() == StructurePreparation::Phase::Ready && !preparation.Running() &&
            !preparation.AwaitSlice(0.002),
        "publication consumes only its own completion");
  CHECK(pool.TakeCompletion(unrelated), "unrelated completion remains with its owner");
}
}

int main() {
  Retire(false);
  Retire(true);
  Discard();
  Completion();
  return outshine::Test::Report();
}
