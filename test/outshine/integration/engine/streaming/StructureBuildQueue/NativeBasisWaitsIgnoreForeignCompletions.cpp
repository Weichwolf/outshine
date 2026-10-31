#include "NativeBasisLookupFixture.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <semaphore>
#include <thread>

namespace {
using namespace NativeBasisFixture;

void OwnCompletion() {
  Fixture env;
  if (!Open(env)) { return; }
  const auto foreign = env.Worker.Post([] {});
  CHECK(env.Worker.AwaitCompletion(foreign, 1), "an unrelated retained completion is ready");
  WorkerBlock held;
  CHECK(held.Start(env.Worker), "the actual lookup worker is held");
  const auto pending = Query(env.Queue, env);
  CHECK(pending && pending->Where == StructureBuildQueue::NativeBasisQuery::Phase::Pending,
        "native query queues a real cache lookup");
  CHECK(!env.Queue.AwaitSlice(0) && !env.Queue.AwaitSlice(0.002),
        "the foreign completion cannot satisfy this queue wait");
  std::binary_semaphore entered(0);
  std::binary_semaphore finished(0);
  std::atomic_bool signaled{false};
  std::thread waiter([&] {
    entered.release();
    signaled = env.Queue.AwaitSlice(1);
    finished.release();
  });
  CHECK(entered.try_acquire_for(kDeadline), "waiter starts before its lookup can run");
  CHECK(!finished.try_acquire_for(std::chrono::milliseconds(1)),
        "the retained foreign completion cannot wake the own lookup waiter");
  held.Release();
  waiter.join();
  Barrier(env.Worker);
  CHECK(signaled && env.Queue.AwaitSlice(0.001),
        "real completion signals this queue and remains ready until consumed");
  const auto ready = Query(env.Queue, env);
  CHECK(ready && ready->Where == StructureBuildQueue::NativeBasisQuery::Phase::Ready &&
            ready->Source && ready->Source->Product,
        "only the complete original native product is published");
  CHECK(!env.Queue.AwaitSlice(0.002), "the consumed lookup cannot leave a hot ready signal");
  CHECK(env.Worker.TakeCompletion(foreign), "the foreign completion still belongs to its caller");
}
}

int main() {
  OwnCompletion();
  return outshine::Test::Report();
}
