#include "RetirementFixture.h"
#include "Check.h"
#include <memory>
#include <semaphore>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace outshine::Test::Retirement;

void ReadyCleanup() {
  Tasks pool(1);
  const JobAdmission admission(1);
  const auto control = std::make_shared<Control>();
  control->BlockScratch = true;
  std::binary_semaphore returned{0};
  auto ready = Task(admission, control);
  bool accepted = false;
  bool emptied = false;
  std::thread frame([&, owned = std::move(ready)] mutable {
    control->Frame = std::this_thread::get_id();
    {
      auto task = std::move(owned);
      accepted = task.Retire(pool);
      emptied = !task.Running() && !task.TakeCompletion();
    }
    returned.release();
  });
  CHECK(control->ScratchEntered.try_acquire_for(2s), "the real scratch destructor is blocked");
  CHECK(returned.try_acquire_for(2s), "the frame facade is gone while cleanup is still blocked");
  CHECK(admission.Active() == 1 && !admission.TryAcquire() && !control->Heights.expired(),
        "cleanup keeps the input owner and shared admission until scratch destruction ends");
  control->ReleaseScratch.release();
  frame.join();
  CHECK(accepted && emptied, "retirement empties a ready facade without joining cleanup");
  Drain(pool);
  Released(admission, *control);
}

void ActiveCleanup(bool queued) {
  Tasks pool(1);
  const JobAdmission admission(1);
  const auto control = std::make_shared<Control>();
  control->Frame = std::this_thread::get_id();
  std::binary_semaphore blocked{0};
  std::binary_semaphore release{0};
  if (queued) {
    CHECK(pool.PostDetached([&] {
      blocked.release();
      (void)release.try_acquire_for(5s);
    }),
          "the existing worker accepts its queue blocker");
    CHECK(blocked.try_acquire_for(2s), "the worker is held before the queued bake");
  }
  auto mesher = std::make_shared<BlockingMesher>(control);
  const std::weak_ptr<const StructureMesher> owner = mesher;
  {
    auto task = Task(admission, control);
    task.Start(pool, mesher);
    if (!queued) {
      CHECK(control->MeshEntered.try_acquire_for(2s), "the real bake holds its mesher and scratch");
    }
    CHECK(task.Retire(pool), "queued and running facades retire without waiting for the worker");
  }
  mesher.reset();
  CHECK(!owner.expired() && admission.Active() == 1 && !admission.TryAcquire(),
        "a retired bake retains its mesher and refuses more admitted work while blocked");
  if (queued) {
    release.release();
  } else {
    control->ReleaseMesh.release();
  }
  Drain(pool);
  CHECK(owner.expired(), "all mesher owners end on completion and retirement");
  Released(admission, *control);
}
}

int main() {
  ReadyCleanup();
  ActiveCleanup(true);
  ActiveCleanup(false);
  return outshine::Test::Report();
}
