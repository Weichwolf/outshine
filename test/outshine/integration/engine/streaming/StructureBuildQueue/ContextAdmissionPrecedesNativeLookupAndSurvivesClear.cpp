#include "NativeBasisLookupFixture.h"
#include "JobAdmissionFixture.h"
#include "Check.h"
#include <chrono>
#include <memory>
#include <thread>

int main() {
  using namespace NativeBasisFixture;
  Fixture env;
  if (!Open(env)) { return Report(); }
  const auto contexts = std::make_shared<JobAdmission>(1);
  StructureBuildQueue first;
  StructureBuildQueue next;
  first.Opens(&env.Worker, nullptr, outshine::Test::JobAdmissions(), contexts);
  next.Opens(&env.Worker, nullptr, outshine::Test::JobAdmissions(), contexts);
  WorkerBlock held;
  CHECK(held.Start(env.Worker), "hold the actual native lookup worker");
  const auto pending = Query(first, env);
  CHECK(pending && pending->Where == StructureBuildQueue::NativeAssetQuery::Phase::Pending &&
            first.Queued() == 1 && contexts->Active() == 1,
        "a real native lookup owns its context before queue admission");
  const auto before = env.Stack.BuildingAssets()->Costs();
  first.Clear();
  for (int attempt = 0; attempt < 32; ++attempt) {
    const auto denied = Query(next, env);
    CHECK(denied && denied->Where == StructureBuildQueue::NativeAssetQuery::Phase::Pending &&
              next.Queued() == 0 && contexts->Active() == 1,
          "repeated cache requests remain pending before allocating a lookup");
    next.Clear();
  }
  const auto after = env.Stack.BuildingAssets()->Costs();
  CHECK(before.BasisHits == after.BasisHits && before.BasisMisses == after.BasisMisses &&
            before.BasisReadBytes == after.BasisReadBytes,
        "a denied context performs no native cache read");
  held.Release();
  Barrier(env.Worker);
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  while (contexts->Active() != 0 && std::chrono::steady_clock::now() < until) {
    std::this_thread::yield();
  }
  CHECK(contexts->Active() == 0, "queued cancellation and worker cleanup return the context");
  while (std::chrono::steady_clock::now() < until) {
    const auto ready = Query(next, env);
    if (ready && ready->Where == StructureBuildQueue::NativeAssetQuery::Phase::Ready) {
      CHECK(ready->Basis() != nullptr, "the admitted replacement returns its native basis");
      return Report();
    }
    (void)next.AwaitSlice(0.01);
  }
  CHECK(false, "the unchanged native request must make progress after context release");
  return Report();
}
