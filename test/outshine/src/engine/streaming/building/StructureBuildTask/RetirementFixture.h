#ifndef OUTSHINE_TEST_STRUCTUREBUILDTASK_RETIREMENTFIXTURE_H
#define OUTSHINE_TEST_STRUCTUREBUILDTASK_RETIREMENTFIXTURE_H

#include "Check.h"
#include "StructureBuildTask.h"
#include "Tasks.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <expected>
#include <memory>
#include <semaphore>
#include <thread>
#include <utility>

namespace outshine::Test::Retirement {
using namespace std::chrono_literals;

struct Control {
  std::thread::id Frame;
  bool BlockScratch = false;
  std::binary_semaphore MeshEntered{0};
  std::binary_semaphore ReleaseMesh{0};
  std::binary_semaphore ScratchEntered{0};
  std::binary_semaphore ReleaseScratch{0};
  std::atomic<bool> FreedOnFrame{false};
  std::atomic<bool> Freed{false};
  std::weak_ptr<const Ground::HeightField> Heights;
};

struct Scratch final : MeshScratch {
  explicit Scratch(std::shared_ptr<Control> control) : State(std::move(control)) {}

  ~Scratch() override {
    State->FreedOnFrame.store(std::this_thread::get_id() == State->Frame);
    State->ScratchEntered.release();
    if (State->BlockScratch) { (void)State->ReleaseScratch.try_acquire_for(5s); }
    State->Freed.store(true);
  }

  std::shared_ptr<Control> State;
};

class BlockingMesher final : public StructureMesher {
public:
  explicit BlockingMesher(std::shared_ptr<Control> control) : Control_(std::move(control)) {}

  [[nodiscard]] std::unique_ptr<MeshScratch> Scratch() const override {
    return std::make_unique<Retirement::Scratch>(Control_);
  }

  [[nodiscard]] std::expected<void, StructureMeshError>
  Mesh([[maybe_unused]] const StructurePlan &plan,
       [[maybe_unused]] MeshScratch &scratch,
       [[maybe_unused]] Raised &into) const noexcept override {
    Control_->MeshEntered.release();
    (void)Control_->ReleaseMesh.try_acquire_for(5s);
    return std::unexpected(StructureMeshError::UnsupportedFootprint);
  }

private:
  std::shared_ptr<Control> Control_;
};

inline StructureBuildTask Task(const JobAdmission &admission,
                               const std::shared_ptr<Control> &control) {
  auto lease = admission.TryAcquire();
  CHECK(lease.has_value(), "the retirement fixture acquires a real job credit");
  if (!lease) { std::abort(); }
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
  auto heights = Ground::HeightField::Of(0, {std::move(block)});
  control->Heights = heights;
  return {std::move(*lease),
          1,
          std::move(raw),
          std::move(heights),
          std::make_unique<StructureBuildTask::Output>(),
          std::make_unique<Scratch>(control)};
}

inline void Drain(Tasks &pool) {
  const auto barrier = pool.Post([] {});
  CHECK(pool.AwaitCompletion(barrier, 2), "the existing worker passes its retirement barrier");
  pool.Wait(barrier);
}

inline void Released(const JobAdmission &admission, const Control &control) {
  CHECK(control.Freed.load() && !control.FreedOnFrame.load() && control.Heights.expired(),
        "the worker releases the last tracked scratch and height owners outside the frame");
  CHECK(admission.Active() == 0 && admission.TryAcquire().has_value(),
        "the original credit becomes reusable only after payload cleanup");
}
}
#endif
