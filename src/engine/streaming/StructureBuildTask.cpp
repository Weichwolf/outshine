#include "StructureBuildTask.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <ratio>
#include <utility>

#include "Heap.h"
#include "StructureArtifact.h"
#include "ArtifactBlocks.h"
#include <limits>
#include <string>
#include <string_view>
#include <span>
#include <vector>

namespace outshine {

namespace {
void BakeVariant(const Generators::RawTile *raw,
                 const Ground::HeightField *heights,
                 const StructureMesher &mesher,
                 MeshScratch *scratch,
                 Generators::StructureBakeProgress *progress,
                 StructureBuildTask::Output *output,
                 const std::shared_ptr<std::atomic_bool> &stopping) {
  for (size_t range = 0; range < StructureBuildTask::RangesPerTask && !output->Tile; ++range) {
    const auto rangeBegan = std::chrono::steady_clock::now();
    const auto advanced = progress->AdvanceStructures(
        *raw, *heights, mesher, *scratch, StructureBuildTask::StructuresPerRange, stopping.get());
    const double rangeMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - rangeBegan)
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
      output->Tile = std::move(*finalized);
    }
  }
}
}

struct StructureBuildTask::Comparison {
  enum class Phase { Reference, Surface, Complete };

  explicit Comparison(ProofRequest request) : Request(request) {}

  void Finish(Output &output, std::optional<Generators::StructureSurfaceErrorFailure> failure) {
    assert(Variant.has_value());
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
      AdvanceReference(heights, mesher, scratch, output, stopping);
    } else {
      AdvanceSurface(output, stopping);
    }
  }

  void AdvanceReference(const Ground::HeightField &heights,
                        const StructureMesher &mesher,
                        MeshScratch &scratch,
                        Output &output,
                        const std::atomic_bool &stopping) {
    using namespace Generators;
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
      output.FinalizationMs =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - finalizedAt)
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
      assert(Reference.has_value() && Variant.has_value());
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
  }

  void AdvanceSurface(Output &output, const std::atomic_bool &stopping) {
    using namespace Generators;
    assert(Variant.has_value());
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

struct StructureBuildTask::Artifact {
  enum class Phase { Identity, Read, Decode, Bake, Write, Done };

  explicit Artifact(CacheRequest request, std::optional<ProofRequest> proof)
      : Request(std::move(request)), Proof(proof) {}

  [[nodiscard]] bool UsesIo() const { return Stage == Phase::Read || Stage == Phase::Write; }

  bool BeforeBake(const Generators::RawTile &raw,
                  const Ground::HeightField &heights,
                  const StructureMesher &mesher,
                  Output &output,
                  const std::atomic_bool &stopping) {
    const auto began = std::chrono::steady_clock::now();
    switch (Stage) {
      case Phase::Identity: Identify(raw, heights, mesher, output); return true;
      case Phase::Read:
        Input = Request.Store->Read(Key, Limits);
        output.CacheReadMs +=
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
                .count();
        Stage = Phase::Decode;
        return true;
      case Phase::Decode:
        Decode(output, stopping);
        if (output.Tile) {
          output.CacheHit = true;
          Stage = Phase::Done;
          return true;
        }
        Stage = Phase::Bake;
        return false;
      case Phase::Write:
        if (!Pending || !Pending->Publish() || !Request.Store->Trim()) {
          Fail(output);
        } else {
          output.Tile = std::move(Product);
          Stage = Phase::Done;
        }
        Pending.reset();
        output.CacheWriteMs +=
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
                .count();
        return true;
      case Phase::Bake: return false;
      case Phase::Done: return true;
    }
    return true;
  }

  void Identify(const Generators::RawTile &raw,
                const Ground::HeightField &heights,
                const StructureMesher &mesher,
                Output &output) {
    std::string producer(mesher.ArtifactVersion());
    if (!producer.empty() && Proof) {
      producer += ":proof:" + std::to_string(Proof->Limits.MaxTriangleQueries) + ":" +
                  std::to_string(Proof->Limits.MaxRegions) + ":" +
                  std::to_string(std::bit_cast<uint64_t>(Proof->Limits.TargetUncertaintyM));
    }
    auto key = Generators::StructureArtifactKey(raw, heights, Request.Source, producer);
    if (!key || !Request.Store || !Request.Store->Enabled() || Request.Io == nullptr ||
        Request.ResidentBytesMost == 0 ||
        Request.ResidentBytesMost >
            std::numeric_limits<size_t>::max() - sizeof(Generators::BakedTile)) {
      Fail(output);
      return;
    }
    Key = std::move(*key);
    Limits.EncodedBytesMost = Request.ResidentBytesMost + sizeof(Generators::BakedTile);
    Stage = Phase::Read;
  }

  void CancelIfStopping(Output &output, const std::atomic_bool &stopping, Tasks *activePool) {
    if (!stopping.load(std::memory_order_relaxed)) { return; }
    Release(activePool);
    output.Tile.reset();
    output.Status = std::unexpected(Generators::StructureBakeErrorKind::Cancelled);
  }

  void Release(Tasks *activePool) {
    if (!Input && !Pending) { return; }
    const auto close = [this] {
      Input.reset();
      Pending.reset();
    };
    if (activePool == Request.Io) {
      close();
    } else {
      const auto job = Request.Io->Post(close);
      Request.Io->Wait(job);
    }
  }

  void Decode(Output &output, const std::atomic_bool &stopping) {
    if (Input) {
      const auto encodedBytes = static_cast<size_t>(Input->Manifest().Bytes);
      Data::ArtifactBlockReader reader(
          Input->Manifest(), [&](std::string_view blockKey, size_t mostBytes) {
            if (stopping.load(std::memory_order_relaxed)) {
              return std::optional<std::vector<uint8_t>>{};
            }
            const auto readBegan = std::chrono::steady_clock::now();
            std::optional<std::vector<uint8_t>> loaded;
            const auto job = Request.Io->Post(
                [&, blockKey, mostBytes] { loaded = Input->ReadBlock(blockKey, mostBytes); });
            Request.Io->Wait(job);
            output.CacheReadMs += std::chrono::duration<double, std::milli>(
                                      std::chrono::steady_clock::now() - readBegan)
                                      .count();
            return loaded;
          });
      output.Tile = Generators::ReadStructureProduct(
          [&](std::span<uint8_t> into) { return reader.Read(into); },
          {.EncodedBytes = encodedBytes, .ResidentBytesMost = Request.ResidentBytesMost},
          Request.SourceKey);
    }
    Release(nullptr);
  }

  void AfterBake(Output &output, const std::atomic_bool &stopping) {
    if (!output.Tile || !output.Status) { return; }
    Data::ArtifactBlockWriter writer(
        Limits, [&](std::string_view blockKey, std::span<const uint8_t> bytes) {
          if (stopping.load(std::memory_order_relaxed)) { return false; }
          const auto writeBegan = std::chrono::steady_clock::now();
          bool written = false;
          const auto job = Request.Io->Post([&, blockKey, bytes] {
            if (!Pending) { Pending = Request.Store->Begin(Key, Limits); }
            written = Pending && Pending->Append(blockKey, bytes);
          });
          Request.Io->Wait(job);
          output.CacheWriteMs += std::chrono::duration<double, std::milli>(
                                     std::chrono::steady_clock::now() - writeBegan)
                                     .count();
          return written;
        });
    const auto encoded = Generators::WriteStructureProduct(
        *output.Tile,
        [&](std::span<const uint8_t> bytes) { return writer.Append(bytes); },
        Limits.BlockBytes);
    if (!encoded) {
      Release(nullptr);
      output.Tile.reset();
      auto failure = Generators::StructureBakeErrorKind::ArtifactInvalidProduct;
      if (encoded.error() == Generators::StructureArtifactError::CapacityExceeded) {
        failure = Generators::StructureBakeErrorKind::ArtifactCapacityExceeded;
      } else if (encoded.error() == Generators::StructureArtifactError::WriteFailed) {
        failure = Generators::StructureBakeErrorKind::ArtifactFailure;
      }
      if (stopping.load(std::memory_order_relaxed)) {
        failure = Generators::StructureBakeErrorKind::Cancelled;
      }
      output.Status = std::unexpected(failure);
      return;
    }
    Product = std::move(output.Tile);
    output.Tile.reset();
    Stage = Phase::Write;
  }

  static void Fail(Output &output) {
    output.Tile.reset();
    output.Status = std::unexpected(Generators::StructureBakeErrorKind::ArtifactFailure);
  }

  CacheRequest Request;
  Data::ArtifactLimits Limits;
  std::optional<ProofRequest> Proof;
  Phase Stage = Phase::Identity;
  std::string Key;
  std::unique_ptr<Data::ArtifactStore::Reader> Input;
  std::unique_ptr<Data::ArtifactStore::Writer> Pending;
  std::optional<Generators::BakedTile> Product;
};

StructureBuildTask::StructureBuildTask(uint32_t tile,
                                       std::unique_ptr<Generators::RawTile> raw,
                                       std::shared_ptr<const Ground::HeightField> heights,
                                       std::unique_ptr<Output> output,
                                       std::unique_ptr<MeshScratch> scratch,
                                       std::optional<ProofRequest> proof,
                                       std::optional<CacheRequest> cache)
    : Tile_(tile),
      Raw_(std::move(raw)),
      Heights_(std::move(heights)),
      Output_(std::move(output)),
      Scratch_(std::move(scratch)),
      Progress_(std::make_unique<Generators::StructureBakeProgress>()),
      Stopping_(std::make_shared<std::atomic_bool>(false)),
      Comparison_(proof ? std::make_unique<Comparison>(*proof) : nullptr),
      Artifact_(cache ? std::make_unique<Artifact>(std::move(*cache), proof) : nullptr) {
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
  Artifact_ = std::move(other.Artifact_);
  ActivePool_ = std::exchange(other.ActivePool_, nullptr);
  Handle_ = std::exchange(other.Handle_, Tasks::kNoTask);
  State_ = std::exchange(other.State_, State::Empty);
  return *this;
}

bool StructureBuildTask::AwaitCompletion(double seconds) const {
  return Running() && ActivePool_->AwaitCompletion(seconds);
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
  Artifact *const artifact = Artifact_.get();
  const std::shared_ptr<std::atomic_bool> stopping = Stopping_;
  const auto posted = std::chrono::steady_clock::now();
  State_ = State::Running;
  ActivePool_ = artifact != nullptr && artifact->UsesIo() ? artifact->Request.Io : &pool;
  assert(ActivePool_ != nullptr);
  Tasks *const activePool = ActivePool_;
  Handle_ = ActivePool_->Post([raw,
                               heights,
                               &mesher,
                               scratch,
                               progress,
                               output,
                               comparison,
                               artifact,
                               stopping,
                               posted,
                               activePool] {
    const auto began = std::chrono::steady_clock::now();
    output->LastQueueMs = std::chrono::duration<double, std::milli>(began - posted).count();
    static const Heap::Tag kBakingTag("structure-bake");
    const Heap::Tagged baking(kBakingTag);
    output->LastRanges = 0;
    output->LastProofWork = 0;
    output->LastRangeMs = 0.0;
    if (stopping->load(std::memory_order_relaxed)) {
      if (artifact != nullptr) { artifact->Release(activePool); }
      output->Status = std::unexpected(Generators::StructureBakeErrorKind::Cancelled);
      return;
    }
    if (artifact && artifact->BeforeBake(*raw, *heights, mesher, *output, *stopping)) {
      artifact->CancelIfStopping(*output, *stopping, activePool);
      return;
    }
    if (comparison && comparison->Variant) {
      comparison->Advance(*heights, mesher, *scratch, *output, *stopping);
    } else {
      BakeVariant(raw, heights, mesher, scratch, progress, output, stopping);
      if (comparison && output->Tile) {
        auto variant = std::move(*output->Tile);
        output->Tile.reset();
        comparison->Begin(std::move(variant), *raw, *output);
      }
    }
    if (comparison && stopping->load(std::memory_order_relaxed)) {
      output->Tile.reset();
      output->Status = std::unexpected(Generators::StructureBakeErrorKind::Cancelled);
    }
    const double taskMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    if (artifact) { artifact->AfterBake(*output, *stopping); }
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
  (void)pool;
  if (State_ != State::Running || !ActivePool_->Done(Handle_)) { return false; }
  Handle_ = Tasks::kNoTask;
  State_ = State::Completed;
  return true;
}

void StructureBuildTask::Join(Tasks &pool) {
  if (State_ != State::Running) { return; }
  (void)pool;
  ActivePool_->Wait(Handle_);
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
