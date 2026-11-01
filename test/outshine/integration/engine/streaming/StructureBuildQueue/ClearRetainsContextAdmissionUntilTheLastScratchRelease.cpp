#include "test/outshine/src/engine/streaming/StructureBuildQueue/BuildingQueueFixture.h"
#include "test/outshine/src/engine/streaming/building/StructureBuildTask/RetirementFixture.h"
#include "Check.h"
#include <chrono>
#include <memory>
#include <thread>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using namespace std::chrono_literals;
  Tasks workers(2);
  BuildingQueueFixture first;
  BuildingQueueFixture next;
  if (!first.Open() || !next.Open()) { return Report(); }
  const auto contexts = std::make_shared<JobAdmission>(1);
  const auto jobs = std::make_shared<JobAdmission>(1);
  const auto control = std::make_shared<Retirement::Control>();
  control->Frame = std::this_thread::get_id();
  control->BlockScratch = true;
  const auto mesher = std::make_shared<Retirement::BlockingMesher>(control);
  first.Queue.Opens(&workers, mesher, jobs, contexts);
  next.Queue.Opens(&next.Pool, next.MesherOwner, jobs, contexts);
  CHECK(first.Post() == 1 && control->MeshEntered.try_acquire_for(5s),
        "a real admitted queue bake holds its mesher inside the worker");
  const auto began = std::chrono::steady_clock::now();
  first.Queue.Clear();
  CHECK(std::chrono::steady_clock::now() - began < 250ms && first.Queue.Queued() == 0,
        "Clear detaches the old context without waiting for its running mesher");
  const auto barrier = workers.Post([] {});
  CHECK(barrier != Tasks::kNoTask, "the second worker accepts a context cleanup barrier");
  if (barrier != Tasks::kNoTask) { workers.Wait(barrier); }
  CHECK(contexts->Active() == 1,
        "a running payload retains the context after its retired container has gone");
  const auto copies = next.Copies;
  for (int attempt = 0; attempt < 32; ++attempt) {
    CHECK(next.Post() == 0 && next.Copies == copies && next.Queue.Queued() == 0 &&
              contexts->Active() == 1 && jobs->Active() == 1,
          "repeated replacements cannot copy heights or bypass the retiring context");
    next.Queue.Clear();
  }
  CHECK(next.Queue.ContextAdmissionDeferred() == 32,
        "context backpressure is distinct from bake admission");
  control->ReleaseMesh.release();
  CHECK(control->ScratchEntered.try_acquire_for(5s), "scratch destruction really begins");
  CHECK(contexts->Active() == 1 && jobs->Active() == 1 && !control->FreedOnFrame.load(),
        "both credits outlive the blocked scratch destructor on a worker");
  control->ReleaseScratch.release();
  const auto until = std::chrono::steady_clock::now() + 5s;
  while (contexts->Active() != 0 && std::chrono::steady_clock::now() < until) {
    std::this_thread::yield();
  }
  CHECK(contexts->Active() == 0 && jobs->Active() == 0 && control->Freed.load(),
        "the final payload and context release return both independent credits");
  CHECK(next.Post() == 1 && next.Copies > copies,
        "the unchanged next world progresses once a context becomes available");
  auto landed = next.Finish();
  CHECK(!landed.empty(), "the next context produces a usable native building product");
  if (!landed.empty()) { next.Queue.CommitsLandings(next.Stack.Footprints(), landed); }
  return Report();
}
