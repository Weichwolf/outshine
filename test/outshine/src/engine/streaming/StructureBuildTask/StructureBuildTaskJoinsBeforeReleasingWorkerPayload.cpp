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
  raw->Structures.assign(structures, {.PointCount = 4, .HeightM = 6});
  raw->FocalPx = 1000;
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
  std::atomic_bool joined{false};
  std::atomic_bool stopRequested{false};
  std::thread clear([&] {
    task.RequestStop();
    stopRequested.store(true, std::memory_order_release);
    stopRequested.notify_one();
    task.Join(pool);
    joined.store(true, std::memory_order_release);
  });
  stopRequested.wait(false, std::memory_order_acquire);
  CHECK(!joined.load(std::memory_order_acquire),
        "joining cannot release a running task while its mesher holds worker payload");
  mesher.ReleasesMesh();
  clear.join();
  CHECK(joined.load(std::memory_order_acquire) && !task.Running(),
        "joining returns only after the stopped worker has completed");
  CHECK(!task.Result().Status,
        "a stop requested during meshing reaches the task output as an explicit failure");
  UnsupportedMesher unsupported;
  StructureBuildTask ranged(4,
                            Raw(257),
                            Heights(),
                            std::make_unique<StructureBuildTask::Output>(),
                            unsupported.Scratch());
  ranged.Start(pool, unsupported);
  while (!ranged.TakeCompletion(pool)) { CHECK(pool.AwaitCompletion(1), "first task completes"); }
  CHECK(ranged.Result().Status && !ranged.Result().Complete &&
            ranged.Result().LastRanges == StructureBuildTask::RangesPerTask &&
            ranged.Progress().BakedStructures() ==
                StructureBuildTask::StructuresPerRange * StructureBuildTask::RangesPerTask,
        "one worker task exposes exactly four completed 64-structure ranges");
  ranged.Resume(pool, unsupported);
  while (!ranged.TakeCompletion(pool)) { CHECK(pool.AwaitCompletion(1), "final task completes"); }
  CHECK(ranged.Result().Status && ranged.Result().Complete && ranged.Result().LastRanges == 1 &&
            ranged.Progress().BakedStructures() == 257 && ranged.Result().FinalizationMs >= 0.0,
        "the final task reports its short range and separately timed finalization");
  return Report();
}
