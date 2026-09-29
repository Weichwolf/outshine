#include "StructureBuildTask.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <ratio>
#include <utility>

#include "Heap.h"

namespace outshine {

struct StructureBuildTask::Comparison {
  enum class Phase { Reference, Surface, Complete };

  explicit Comparison(ProofRequest request) : Request(request) {}

  void Finish(Output &output, std::optional<Generators::StructureSurfaceErrorFailure> failure) {
    Variant->SurfaceFailure = failure;
    output.Tile = std::move(Variant);
    Phase_ = Phase::Complete;
  }

  [[nodiscard]] static bool CompleteGeometry(const Generators::BakedTile &tile) {
    return tile.UnsupportedMeshes == 0 && tile.SkippedRings == 0 && tile.NoGround == 0 &&
           !tile.FallbackHeights;
  }

  void Begin(Generators::BakedTile variant, const Generators::RawTile &raw, Output &output) {
    Variant = std::move(variant);
    if (Request.SourceKey == 0 || !raw.RequestedCell || !raw.RequestedDetail ||
        *raw.RequestedDetail == LevelOfDetail::Fine) {
      Finish(output, Generators::StructureSurfaceErrorFailure::InvalidSource);
      return;
    }
    if (!CompleteGeometry(*Variant)) {
      Finish(output, Generators::StructureSurfaceErrorFailure::InvalidGeometry);
      return;
    }
    ReferenceRaw = raw;
    ReferenceRaw.RequestedDetail = LevelOfDetail::Fine;
  }

  void Advance(const Ground::HeightField &heights,
               const StructureMesher &mesher,
               MeshScratch &scratch,
               Output &output,
               const std::atomic_bool &stopping) {
    using namespace Generators;
    if (stopping.load(std::memory_order_relaxed)) {
      Surface.Cancel();
      output.Status = std::unexpected(StructureBakeErrorKind::Cancelled);
      return;
    }
    if (Phase_ == Phase::Reference) {
      for (size_t range = 0; range < RangesPerTask; ++range) {
        const auto began = std::chrono::steady_clock::now();
        const auto advanced = ReferenceProgress.AdvanceStructures(
            ReferenceRaw, heights, mesher, scratch, StructuresPerRange, &stopping);
        output.LastRangeMs = std::max(
            output.LastRangeMs,
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
                .count());
        if (!advanced) {
          if (stopping.load(std::memory_order_relaxed)) {
            output.Status = std::unexpected(StructureBakeErrorKind::Cancelled);
          } else {
            Finish(output, StructureSurfaceErrorFailure::InvalidGeometry);
          }
          return;
        }
        ++output.LastRanges;
        if (!*advanced) { continue; }
        const auto finalizedAt = std::chrono::steady_clock::now();
        auto reference = ReferenceProgress.Finalize(ReferenceRaw, mesher, scratch, &stopping);
        output.FinalizationMs = std::chrono::duration<double, std::milli>(
                                    std::chrono::steady_clock::now() - finalizedAt)
                                    .count();
        if (stopping.load(std::memory_order_relaxed)) {
          output.Status = std::unexpected(StructureBakeErrorKind::Cancelled);
          return;
        }
        if (!reference || !CompleteGeometry(*reference)) {
          Finish(output, StructureSurfaceErrorFailure::InvalidGeometry);
          return;
        }
        Reference = std::move(*reference);
        const auto reset = Surface.Reset({.Reference = Reference->Built,
                                          .Variant = Variant->Built,
                                          .SourceKey = Request.SourceKey},
                                         Request.Limits);
        if (!reset) {
          Finish(output, reset.error());
          return;
        }
        Phase_ = Phase::Surface;
        return;
      }
      return;
    }
    assert(Phase_ == Phase::Surface);
    const auto began = std::chrono::steady_clock::now();
    constexpr size_t kWorkPerPost = 8192;
    constexpr size_t kWorkPerSlice = 128;
    for (size_t work = 0; work < kWorkPerPost; work += kWorkPerSlice) {
      if (stopping.load(std::memory_order_relaxed)) {
        Surface.Cancel();
        output.Status = std::unexpected(StructureBakeErrorKind::Cancelled);
        return;
      }
      const size_t before = Surface.WorkUnits();
      const auto advanced = Surface.Step(Request.SourceKey, {.MaxWorkUnits = kWorkPerSlice});
      output.LastProofWork += Surface.WorkUnits() - before;
      if (!advanced) {
        Finish(output, advanced.error());
        return;
      }
      if (*advanced == StructureSurfaceErrorProgress::Complete) {
        if (stopping.load(std::memory_order_relaxed)) {
          output.Status = std::unexpected(StructureBakeErrorKind::Cancelled);
          return;
        }
        Variant->SurfaceError = Surface.Bound(Request.SourceKey);
        Finish(output,
               Variant->SurfaceError
                   ? std::nullopt
                   : std::optional{StructureSurfaceErrorFailure::InvalidGeometry});
        return;
      }
      if (std::chrono::steady_clock::now() - began >= std::chrono::milliseconds(2)) { return; }
    }
  }

  ProofRequest Request;
  Generators::RawTile ReferenceRaw;
  Generators::StructureBakeProgress ReferenceProgress;
  std::optional<Generators::BakedTile> Variant;
  std::optional<Generators::BakedTile> Reference;
  Generators::StructureSurfaceRefinementTask Surface;
  Phase Phase_ = Phase::Reference;
};

StructureBuildTask::StructureBuildTask(uint32_t tile,
                                       std::unique_ptr<Generators::RawTile> raw,
                                       std::shared_ptr<const Ground::HeightField> heights,
                                       std::unique_ptr<Output> output,
                                       std::unique_ptr<MeshScratch> scratch,
                                       std::optional<ProofRequest> proof)
    : Tile_(tile),
      Raw_(std::move(raw)),
      Heights_(std::move(heights)),
      Output_(std::move(output)),
      Scratch_(std::move(scratch)),
      Progress_(std::make_unique<Generators::StructureBakeProgress>()),
      Stopping_(std::make_shared<std::atomic_bool>(false)),
      Comparison_(proof ? std::make_unique<Comparison>(*proof) : nullptr) {
  assert(Raw_ != nullptr && Heights_ != nullptr && Output_ != nullptr && Scratch_ != nullptr);
}

StructureBuildTask::~StructureBuildTask() {
  assert(State_ != State::Running);
}

StructureBuildTask::StructureBuildTask(StructureBuildTask &&other) noexcept : State_(State::Empty) {
  *this = std::move(other);
}

StructureBuildTask &StructureBuildTask::operator=(StructureBuildTask &&other) noexcept {
  if (this == &other) { return *this; }
  assert(State_ != State::Running);
  Tile_ = std::exchange(other.Tile_, 0);
  Raw_ = std::move(other.Raw_);
  Heights_ = std::move(other.Heights_);
  Output_ = std::move(other.Output_);
  Scratch_ = std::move(other.Scratch_);
  Progress_ = std::move(other.Progress_);
  Stopping_ = std::move(other.Stopping_);
  Comparison_ = std::move(other.Comparison_);
  Handle_ = std::exchange(other.Handle_, Tasks::kNoTask);
  State_ = std::exchange(other.State_, State::Empty);
  return *this;
}

bool StructureBuildTask::Running() const noexcept {
  return State_ == State::Running;
}

void StructureBuildTask::Posts(Tasks &pool, const StructureMesher &mesher) {
  assert(State_ != State::Running);
  const Generators::RawTile *const raw = Raw_.get();
  const Ground::HeightField *const heights = Heights_.get();
  MeshScratch *const scratch = Scratch_.get();
  Generators::StructureBakeProgress *const progress = Progress_.get();
  Output *const output = Output_.get();
  Comparison *const comparison = Comparison_.get();
  const std::shared_ptr<std::atomic_bool> stopping = Stopping_;
  const auto posted = std::chrono::steady_clock::now();
  State_ = State::Running;
  Handle_ =
      pool.Post([raw, heights, &mesher, scratch, progress, output, comparison, stopping, posted] {
        const auto began = std::chrono::steady_clock::now();
        output->LastQueueMs = std::chrono::duration<double, std::milli>(began - posted).count();
        static const Heap::Tag kBakingTag("structure-bake");
        const Heap::Tagged baking(kBakingTag);
        output->LastRanges = 0;
        output->LastProofWork = 0;
        output->LastRangeMs = 0.0;
        if (comparison && comparison->Variant) {
          comparison->Advance(*heights, mesher, *scratch, *output, *stopping);
        } else {
          for (size_t range = 0; range < RangesPerTask && !output->Tile; ++range) {
            const auto rangeBegan = std::chrono::steady_clock::now();
            const auto advanced = progress->AdvanceStructures(
                *raw, *heights, mesher, *scratch, StructuresPerRange, stopping.get());
            const double rangeMs = std::chrono::duration<double, std::milli>(
                                       std::chrono::steady_clock::now() - rangeBegan)
                                       .count();
            output->LastRangeMs = std::max(output->LastRangeMs, rangeMs);
            if (!advanced) {
              output->Status = std::unexpected(advanced.error());
              break;
            }
            ++output->LastRanges;
            if (*advanced) {
              const auto finalizationBegan = std::chrono::steady_clock::now();
              auto finalized = progress->Finalize(*raw, mesher, *scratch, stopping.get());
              output->FinalizationMs = std::chrono::duration<double, std::milli>(
                                           std::chrono::steady_clock::now() - finalizationBegan)
                                           .count();
              if (!finalized) {
                output->Status = std::unexpected(finalized.error());
                break;
              }
              if (comparison) {
                comparison->Begin(std::move(*finalized), *raw, *output);
                break;
              }
              output->Tile = std::move(*finalized);
            }
          }
        }
        if (comparison && stopping->load(std::memory_order_relaxed)) {
          output->Tile.reset();
          output->Status = std::unexpected(Generators::StructureBakeErrorKind::Cancelled);
        }
        const double taskMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
                .count();
        output->LastTaskMs = taskMs;
        output->BakeMs += taskMs;
      });
}

void StructureBuildTask::Start(Tasks &pool, const StructureMesher &mesher) {
  assert(State_ == State::Ready);
  Posts(pool, mesher);
}

void StructureBuildTask::Resume(Tasks &pool, const StructureMesher &mesher) {
  assert(State_ == State::Completed && Output_->Status && !Output_->Tile);
  Posts(pool, mesher);
}

void StructureBuildTask::RequestStop() noexcept {
  if (Stopping_) { Stopping_->store(true, std::memory_order_relaxed); }
}

bool StructureBuildTask::TakeCompletion(Tasks &pool) {
  if (State_ != State::Running || !pool.Done(Handle_)) { return false; }
  Handle_ = Tasks::kNoTask;
  State_ = State::Completed;
  return true;
}

void StructureBuildTask::Join(Tasks &pool) {
  if (State_ != State::Running) { return; }
  pool.Wait(Handle_);
  Handle_ = Tasks::kNoTask;
  State_ = State::Completed;
}

std::unique_ptr<Generators::RawTile> StructureBuildTask::TakeRaw() noexcept {
  assert(State_ != State::Running);
  return std::move(Raw_);
}

std::unique_ptr<StructureBuildTask::Output> StructureBuildTask::TakeOutput() noexcept {
  assert(State_ != State::Running);
  return std::move(Output_);
}

std::unique_ptr<MeshScratch> StructureBuildTask::TakeScratch() noexcept {
  assert(State_ != State::Running);
  return std::move(Scratch_);
}

}
