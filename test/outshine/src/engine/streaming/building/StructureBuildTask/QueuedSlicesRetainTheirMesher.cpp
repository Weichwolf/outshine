#include "JobAdmissionFixture.h"
#include "Check.h"
#include "HeightField.h"
#include "StructureBake.h"
#include "StructureBuildTask.h"
#include "StructureMesher.h"
#include "Tasks.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <expected>
#include <memory>
#include <mutex>
#include <utility>

namespace {
using namespace outshine;

struct Scratch final : MeshScratch {
  std::weak_ptr<const StructureMesher> Owner;
  std::atomic<bool> *OwnerAlive = nullptr;
  std::weak_ptr<JobAdmission> Admission;
  std::atomic<bool> *AdmissionHeld = nullptr;

  ~Scratch() override {
    if (OwnerAlive != nullptr) { OwnerAlive->store(!Owner.expired(), std::memory_order_relaxed); }
    if (AdmissionHeld != nullptr) {
      const auto admission = Admission.lock();
      AdmissionHeld->store(admission && admission->Active() > 0, std::memory_order_relaxed);
    }
  }
};

class Mesher final : public StructureMesher {
public:
  explicit Mesher(std::atomic<unsigned> &calls) : Calls_(calls) {}

  [[nodiscard]] std::unique_ptr<MeshScratch> Scratch() const override {
    return std::make_unique<::Scratch>();
  }

  [[nodiscard]] std::expected<void, StructureMeshError>
  Mesh([[maybe_unused]] const StructurePlan &plan,
       [[maybe_unused]] MeshScratch &scratch,
       [[maybe_unused]] Raised &into) const noexcept override {
    Calls_.fetch_add(1, std::memory_order_relaxed);
    return std::unexpected(StructureMeshError::UnsupportedFootprint);
  }

private:
  std::atomic<unsigned> &Calls_;
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::atomic<unsigned> calls{0};
  std::atomic<bool> scratchOwnerAlive{false};
  std::atomic<bool> scratchAdmissionHeld{false};
  Tasks pool(1);
  const auto unrelated = pool.Post([] {});
  CHECK(pool.AwaitCompletion(unrelated, 2), "an unrelated completion remains in the pool");
  std::mutex mutex;
  std::condition_variable wake;
  bool entered = false;
  bool released = false;
  const auto blocker = pool.Post([&] {
    std::unique_lock lock(mutex);
    entered = true;
    wake.notify_all();
    (void)wake.wait_for(lock, std::chrono::seconds(5), [&] { return released; });
  });
  {
    std::unique_lock lock(mutex);
    CHECK(wake.wait_for(lock, std::chrono::seconds(2), [&] { return entered; }),
          "the bake cannot start before its creator releases its mesher");
  }
  std::weak_ptr<const StructureMesher> weak;
  {
    auto mesher = std::make_shared<Mesher>(calls);
    weak = mesher;
    auto raw = std::make_unique<Generators::RawTile>();
    raw->LatLon = {47, 9, 47, 9.0001, 47.0001, 9.0001, 47.0001, 9};
    raw->Structures = {{.PointCount = 4,
                        .Cell = {.Index = 1, .Footprint = {}},
                        .HeightM = 6,
                        .WallColour = {},
                        .Facade = {}}};
    raw->Projection.FocalPx = 1000;
    raw->Eye = {.LongitudeDeg = 9, .LatitudeDeg = 47};
    raw->TileSpanM = 1000;
    Ground::HeightField::Block block;
    block.At = {.Zoom = 0, .X = 0, .Y = 0};
    block.Raster = {.Side = 2, .Postings = 2};
    block.Nodes.assign(4, 0);
    auto scratch = mesher->Scratch();
    auto &tracked = static_cast<::Scratch &>(*scratch);
    tracked.Owner = mesher;
    tracked.OwnerAlive = &scratchOwnerAlive;
    tracked.Admission = JobAdmissions();
    tracked.AdmissionHeld = &scratchAdmissionHeld;
    StructureBuildTask task(outshine::Test::JobLease(),
                            1,
                            std::move(raw),
                            Ground::HeightField::Of(0, {std::move(block)}),
                            std::make_unique<StructureBuildTask::Output>(),
                            std::move(scratch));
    task.Start(pool, mesher);
    mesher.reset();
    CHECK(!weak.expired(), "the task owns the mesher while its worker is still queued");
    CHECK(!task.AwaitCompletion(0.01),
          "another job's completion cannot complete a queued building slice");
    {
      const std::scoped_lock lock(mutex);
      released = true;
    }
    wake.notify_all();
    pool.Wait(blocker);
    task.Join();
    CHECK(calls.load(std::memory_order_relaxed) > 0 && !weak.expired(),
          "real meshing runs after creator teardown and retains its mesher for continuation");
    const auto workerReleased = pool.Post([] {});
    pool.Wait(workerReleased);
  }
  const auto drained = pool.Post([] {});
  pool.Wait(drained);
  CHECK(weak.expired(), "completed task and worker release their last mesher owner");
  CHECK(scratchOwnerAlive.load(std::memory_order_relaxed),
        "the mesher also outlives destruction of the scratch that its factory created");
  CHECK(scratchAdmissionHeld.load(std::memory_order_relaxed),
        "scratch destruction completes before its job credit becomes reusable");
  CHECK(pool.TakeCompletion(unrelated), "the unrelated completion remains available to its owner");
  return Report();
}
