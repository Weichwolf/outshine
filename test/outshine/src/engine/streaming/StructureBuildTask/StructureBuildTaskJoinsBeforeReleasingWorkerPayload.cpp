#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include "Check.h"
#include "StructureBuildTask.h"

namespace {

struct BlockingScratch final : outshine::MeshScratch {
  std::atomic_bool Held{false};
};

class BlockingMesher final : public outshine::StructureMesher {
public:
  std::unique_ptr<outshine::MeshScratch> Scratch() const override {
    return std::make_unique<BlockingScratch>();
  }

  std::expected<void, outshine::StructureMeshError>
  Mesh(const outshine::StructurePlan &,
       outshine::MeshScratch &scratch,
       outshine::Raised &) const noexcept override {
    auto &blocking = static_cast<BlockingScratch &>(scratch);
    blocking.Held.store(true, std::memory_order_release);
    std::unique_lock lock(Mutex_);
    Entered_ = true;
    EnteredCv_.notify_one();
    ReleasedCv_.wait(lock, [this] { return Released_; });
    blocking.Held.store(false, std::memory_order_release);
    return std::unexpected(outshine::StructureMeshError::UnsupportedFootprint);
  }

  [[nodiscard]] bool WaitsInMesh() const {
    std::unique_lock lock(Mutex_);
    return EnteredCv_.wait_for(lock, std::chrono::seconds(1), [this] { return Entered_; });
  }

  void ReleasesMesh() const {
    std::lock_guard lock(Mutex_);
    Released_ = true;
    ReleasedCv_.notify_one();
  }

private:
  mutable std::mutex Mutex_;
  mutable std::condition_variable EnteredCv_;
  mutable std::condition_variable ReleasedCv_;
  mutable bool Entered_ = false;
  mutable bool Released_ = false;
};

struct Scratch final : outshine::MeshScratch {};

class UnsupportedMesher final : public outshine::StructureMesher {
public:
  std::unique_ptr<outshine::MeshScratch> Scratch() const override {
    return std::make_unique<::Scratch>();
  }

  std::expected<void, outshine::StructureMeshError>
  Mesh(const outshine::StructurePlan &,
       outshine::MeshScratch &,
       outshine::Raised &) const noexcept override {
    return std::unexpected(outshine::StructureMeshError::UnsupportedFootprint);
  }
};

std::shared_ptr<const outshine::Ground::HeightField> Heights() {
  outshine::Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 0);
  return outshine::Ground::HeightField::Of(0, {std::move(block)});
}

std::unique_ptr<outshine::Generators::RawTile> Raw(size_t structures = 1) {
  auto raw = std::make_unique<outshine::Generators::RawTile>();
  raw->LatLon = {47, 9, 47, 9.0001, 47.0001, 9.0001, 47.0001, 9};
  raw->Structures.assign(structures, {.PointCount = 4, .Cell = {.Index = 1}, .HeightM = 6});
  raw->Projection.FocalPx = 1000;
  raw->Eye = {.LongitudeDeg = 9, .LatitudeDeg = 47};
  raw->TileSpanM = 1000;
  return raw;
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Tasks pool(1);
  BlockingMesher mesher;
  StructureBuildTask task(
      3, Raw(), Heights(), std::make_unique<StructureBuildTask::Output>(), mesher.Scratch());
  task.Start(pool, mesher);
  CHECK(mesher.WaitsInMesh(), "the real bake worker holds its scratch during meshing");
  StructureBuildTask stopped(std::move(task));
  CHECK(stopped.Running() && !task.Running(),
        "cancellation fixture transfers a running owner before shutdown");
  if (!task.Running()) { task.RequestStop(); }
  task = StructureBuildTask(
      5, Raw(), Heights(), std::make_unique<StructureBuildTask::Output>(), mesher.Scratch());
  std::atomic_bool joined{false};
  std::atomic_bool stopRequested{false};
  std::thread clear([&] {
    stopped.RequestStop();
    stopRequested.store(true, std::memory_order_release);
    stopRequested.notify_one();
    stopped.Join(pool);
    joined.store(true, std::memory_order_release);
  });
  stopRequested.wait(false, std::memory_order_acquire);
  CHECK(!joined.load(std::memory_order_acquire),
        "joining cannot release a running task while its mesher holds worker payload");
  mesher.ReleasesMesh();
  clear.join();
  CHECK(joined.load(std::memory_order_acquire) && !stopped.Running(),
        "joining returns only after the stopped worker has completed");
  CHECK(!stopped.Result().Status,
        "a stop requested during meshing reaches the task output as an explicit failure");
  BlockingMesher movingMesher;
  auto movedHeights = Heights();
  const std::weak_ptr<const Ground::HeightField> releasedHeights = movedHeights;
  StructureBuildTask moving(7,
                            Raw(),
                            std::move(movedHeights),
                            std::make_unique<StructureBuildTask::Output>(),
                            movingMesher.Scratch());
  const auto *raw = &moving.Raw();
  const auto *heights = &moving.Heights();
  const auto *output = &moving.Result();
  const auto *progress = &moving.Progress();
  moving.Start(pool, movingMesher);
  CHECK(movingMesher.WaitsInMesh(), "move fixture holds a real worker in its mesher");
  StructureBuildTask constructed(std::move(moving));
  CHECK(constructed.Running() && !moving.Running() && constructed.Tile() == 7 &&
            &constructed.Raw() == raw && &constructed.Heights() == heights &&
            &constructed.Result() == output && &constructed.Progress() == progress,
        "running move construction transfers the sole handle owner and stable payload");
  if (!moving.Running()) {
    moving.RequestStop();
    moving.Join(pool);
    CHECK(!moving.TakeCompletion(pool), "empty source cannot consume a live completion");
  }
  moving = StructureBuildTask(
      8, Raw(), Heights(), std::make_unique<StructureBuildTask::Output>(), movingMesher.Scratch());
  auto overwritten = Heights();
  const std::weak_ptr<const Ground::HeightField> releasedDestination = overwritten;
  StructureBuildTask assigned(9,
                              Raw(),
                              std::move(overwritten),
                              std::make_unique<StructureBuildTask::Output>(),
                              movingMesher.Scratch());
  assigned = std::move(constructed);
  CHECK(assigned.Running() && !constructed.Running() && assigned.Tile() == 7 &&
            &assigned.Raw() == raw && &assigned.Heights() == heights &&
            &assigned.Result() == output && &assigned.Progress() == progress &&
            releasedDestination.expired(),
        "assignment releases a ready destination and transfers running payload exactly once");
  if (!constructed.Running()) {
    constructed.RequestStop();
    constructed.Join(pool);
    CHECK(!constructed.TakeCompletion(pool), "assigned-from source is safe to stop/join/poll");
  }
  constructed = StructureBuildTask(
      10, Raw(), Heights(), std::make_unique<StructureBuildTask::Output>(), movingMesher.Scratch());
  auto &self = assigned;
  assigned = std::move(self);
  CHECK(assigned.Running() && &assigned.Raw() == raw,
        "self move preserves the running owner and worker payload");
  movingMesher.ReleasesMesh();
  assigned.Join(pool);
  CHECK(!assigned.Running() && assigned.Result().Status && assigned.Result().Tile,
        "stopping moved-from owners does not cancel the transferred worker");
  CHECK(!assigned.TakeCompletion(pool) && !releasedHeights.expired(),
        "joined completion is consumed once while its owner retains height input");
  assigned = StructureBuildTask(
      11, Raw(), Heights(), std::make_unique<StructureBuildTask::Output>(), movingMesher.Scratch());
  CHECK(releasedHeights.expired(), "completed payload releases after its sole owner replaces it");
  UnsupportedMesher unsupported;
  StructureBuildTask ranged(4,
                            Raw(257),
                            Heights(),
                            std::make_unique<StructureBuildTask::Output>(),
                            unsupported.Scratch());
  ranged.Start(pool, unsupported);
  while (!ranged.TakeCompletion(pool)) { CHECK(pool.AwaitCompletion(1), "first task completes"); }
  CHECK(ranged.Result().Status && !ranged.Result().Tile &&
            ranged.Result().LastRanges == StructureBuildTask::RangesPerTask &&
            ranged.Progress().BakedStructures() ==
                StructureBuildTask::StructuresPerRange * StructureBuildTask::RangesPerTask,
        "one worker task exposes exactly four completed 64-structure ranges");
  const auto *partial = &ranged.Progress();
  StructureBuildTask resumed(std::move(ranged));
  CHECK(!ranged.Running() && ranged.Tile() == 0 && &resumed.Progress() == partial &&
            resumed.Progress().BakedStructures() == 256 && !resumed.Result().Tile,
        "partial completion moves without resetting progress or publishing final output");
  if (ranged.Tile() == 0) {
    ranged.RequestStop();
    ranged.Join(pool);
  }
  resumed.Resume(pool, unsupported);
  while (!resumed.TakeCompletion(pool)) { CHECK(pool.AwaitCompletion(1), "final task completes"); }
  CHECK(resumed.Result().Status && resumed.Result().Tile && resumed.Result().LastRanges == 1 &&
            resumed.Progress().BakedStructures() == 257 && resumed.Result().FinalizationMs >= 0.0,
        "the final task reports its short range and separately timed finalization");
  StructureBuildTask completed(std::move(resumed));
  CHECK(!completed.Running() && !resumed.Running() && resumed.Tile() == 0 &&
            completed.Result().Tile && &completed.Progress() == partial &&
            completed.Progress().BakedStructures() == 257,
        "final completion transfers its output and progress without inventing a live handle");
  if (resumed.Tile() == 0) {
    resumed.RequestStop();
    resumed.Join(pool);
  }
  return Report();
}
