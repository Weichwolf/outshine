#include "test/outshine/src/generators/osm/StructurePreparation/PreparationFixture.h"
#include "JobAdmissionFixture.h"
#include "StructureBuildQueue.h"
#include "Check.h"
#include <chrono>
#include <memory>
#include <thread>

int main() {
  using namespace PreparationFixture;
  Tasks pool(1);
  WorkerBlock held;
  CHECK(held.Start(pool), "hold the original preparation worker");
  const auto contexts = std::make_shared<JobAdmission>(1);
  StructureBuildQueue first;
  StructureBuildQueue next;
  first.Opens(&pool, nullptr, JobAdmissions(), contexts);
  next.Opens(&pool, nullptr, JobAdmissions(), contexts);
  const auto source = Source();
  const auto queued = first.PrepareOriginal(source, kDefaultStructurePolicy, kHeightZoom);
  CHECK(queued && !*queued && first.Queued() == 1 && contexts->Active() == 1,
        "original preparation owns a context before posting its real source");
  first.Clear();
  const auto owners = source.use_count();
  for (int attempt = 0; attempt < 32; ++attempt) {
    const auto denied = next.PrepareOriginal(source, kDefaultStructurePolicy, kHeightZoom);
    CHECK(denied && !*denied && next.Queued() == 0 && source.use_count() == owners &&
              contexts->Active() == 1,
          "denied preparation neither retains source copies nor creates another worker");
    next.Clear();
  }
  held.Release();
  Barrier(pool);
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  while (contexts->Active() != 0 && std::chrono::steady_clock::now() < until) {
    std::this_thread::yield();
  }
  CHECK(contexts->Active() == 0 && !first.HasOriginal(),
        "old preparation releases its context without publishing a late original");
  while (std::chrono::steady_clock::now() < until) {
    const auto ready = next.PrepareOriginal(source, kDefaultStructurePolicy, kHeightZoom);
    CHECK(ready, "the unchanged original remains a valid request");
    if (!ready) { break; }
    if (*ready) {
      CHECK(next.HasOriginal() && contexts->Active() == 1,
            "the replacement publishes its original under the same context budget");
      return Report();
    }
    (void)next.AwaitSlice(0.01);
  }
  CHECK(false, "original preparation must progress after the previous context retires");
  return Report();
}
