#include "WaterGeometryPreparation.h"
#include "WaterAsset.h"
#include "TangentFrame.h"
#include "Tasks.h"
#include "Check.h"
#include <memory>
#include <semaphore>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Tasks pool(1);
  const Generators::WaterAsset source({.Surfaces = {{.FirstRing = 0, .RingCount = 1, .LevelM = 10}},
                                       .Rings = {{.FirstPoint = 0, .PointCount = 3}},
                                       .Points = {-.001, -.001, -.001, .001, .001, 0}});
  WaterGeometryPreparation worker(pool, source, TangentFrame::At({}));
  CHECK(worker.Matches(source) && !worker.Matches(Generators::WaterAsset{}),
        "a worker cannot publish for a replaced input snapshot");
  const auto barrier = pool.Post([] {});
  CHECK(barrier != Tasks::kNoTask, "completion barrier is queued after preparation");
  pool.Wait(barrier);
  const auto result = worker.Collect();
  CHECK(result && *result && result->value().Water.SurfacesReady() &&
            result->value().Water.SharesInputsWith(source) && result->value().WorkerMs >= 0,
        "worker publishes ready geometry bound to its immutable source snapshot");
  CHECK(!worker.Collect(), "the completed product is collected exactly once");
  std::binary_semaphore started(0), release(0);
  const auto blocker = pool.Post([&] {
    started.release();
    release.acquire();
  });
  CHECK(blocker != Tasks::kNoTask, "a controlled job blocks the compute worker");
  started.acquire();
  auto canceled = std::make_unique<WaterGeometryPreparation>(pool, source, TangentFrame::At({}));
  canceled->Cancel();
  CHECK(!canceled->Complete(), "canceling queued work never pretends it has already completed");
  auto destroyed = std::make_unique<WaterGeometryPreparation>(pool, source, TangentFrame::At({}));
  destroyed.reset();
  release.release();
  const auto drained = pool.Post([] {});
  pool.Wait(drained);
  const auto abandoned = canceled->Collect();
  CHECK(canceled->Complete() && abandoned && !*abandoned,
        "canceled preparation reaches a terminal result before replacement");
  canceled.reset();
  CHECK(!source.SurfacesReady(),
        "canceling queued work neither blocks its caller nor modifies the shared source");
  return Report();
}
