#include "StructureBuildTask.h"
#include "PreparedStructureTile.h"
#include "PreparedBuildingAssets.h"
#include "Tasks.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <bit>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <ratio>
#include <utility>
#include <variant>
#include <string>

#include "Heap.h"
#include <string_view>
#include <span>

namespace outshine {

namespace {
void BakeVariant(const Generators::RawTile *raw,
                 const Ground::HeightField *heights,
                 const StructureMesher &mesher,
                 MeshScratch *scratch,
                 Generators::StructureBakeProgress *progress,
                 StructureBuildTask::Output *output,
                 const std::shared_ptr<std::atomic_bool> &stopping,
                 const Generators::PreparedStructureTile *base = nullptr) {
  for (size_t range = 0; range < StructureBuildTask::RangesPerTask && !output->Tile; ++range) {
    const auto rangeBegan = std::chrono::steady_clock::now();
    const auto advanced = base != nullptr
                              ? progress->AdvancePrepared(*base,
                                                          *raw,
                                                          mesher,
                                                          *scratch,
                                                          StructureBuildTask::StructuresPerRange,
                                                          stopping.get())
                              : progress->AdvanceStructures(*raw,
                                                            *heights,
                                                            mesher,
                                                            *scratch,
                                                            StructureBuildTask::StructuresPerRange,
                                                            stopping.get());
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

  static void CancelIfStopping(Output &output, const std::atomic_bool &stopping) {
    if (!stopping.load(std::memory_order_relaxed)) { return; }
    output.Tile.reset();
    output.Status = std::unexpected(Generators::StructureBakeErrorKind::Cancelled);
  }

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
    ReferenceRaw.Projection.AllowedErrorPx = 0.0;
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

  void Bake(const Generators::RawTile &raw,
            const Ground::HeightField &heights,
            const StructureMesher &mesher,
            MeshScratch &scratch,
            Generators::StructureBakeProgress &progress,
            Output &output,
            const std::shared_ptr<std::atomic_bool> &stopping) {
    if (Variant) {
      Advance(heights, mesher, scratch, output, *stopping);
      return;
    }
    BakeVariant(&raw, &heights, mesher, &scratch, &progress, &output, stopping);
    if (!output.Tile) { return; }
    auto variant = std::move(*output.Tile);
    output.Tile.reset();
    Begin(std::move(variant), raw, output);
  }

  ProofRequest Request;
  Generators::RawTile ReferenceRaw;
  Generators::StructureBakeProgress ReferenceProgress;
  std::optional<Generators::BakedTile> Variant;
  std::optional<Generators::BakedTile> Reference;
  Generators::StructureSurfaceRefinementTask Surface;
  Phase Phase_ = Phase::Reference;
};

struct StructureBuildTask::NativeProducts {
  enum class Phase : uint8_t { Lookup, Generating, Publishing, Complete };

  NativeProducts(std::shared_ptr<Generators::PreparedBuildingAssets> cache,
                 std::string key,
                 std::string requestKey)
      : Cache(std::move(cache)), Key(std::move(key)), RequestKey(std::move(requestKey)) {}

  [[nodiscard]] bool ReadyToResume() const noexcept {
    return std::chrono::steady_clock::now() >= RetryAt;
  }

  static bool Busy(const Generators::StructureBakeError &error) noexcept {
    if (const auto *mesh = std::get_if<StructureMeshError>(&error)) {
      return *mesh == StructureMeshError::PreparedSurfaceBusy;
    }
    const auto *kind = std::get_if<Generators::StructureBakeErrorKind>(&error);
    return kind != nullptr && *kind == Generators::StructureBakeErrorKind::ArtifactBusy;
  }

  void Defer() {
    const auto exponent = std::min(Retries, 4U);
    Retries = std::min(Retries + 1U, 4U);
    RetryAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(uint64_t{1} << exponent);
  }

  void Cancel(Output &output) {
    output.Tile.reset();
    PendingTile.reset();
    Geometry.reset();
    BasePublication.reset();
    output.PublicationBytes = 0;
    output.Status = std::unexpected(Generators::StructureBakeErrorKind::Cancelled);
    Current = Phase::Complete;
  }

  void Publish(Output &output, const std::atomic_bool &stopping) {
    if (stopping.load(std::memory_order_relaxed)) {
      Cancel(output);
      return;
    }
    if (!Geometry || !PendingTile) {
      output.Status = std::unexpected(Generators::StructureBakeErrorKind::ArtifactInvalidProduct);
      Geometry.reset();
      PendingTile.reset();
      output.PublicationBytes = 0;
      Current = Phase::Complete;
      return;
    }
    ++output.PublicationAttempts;
    const auto stored = Cache->PublishGeometry(*Geometry);
    if (!stored && Busy(stored.error())) {
      Defer();
      return;
    }
    if (stopping.load(std::memory_order_relaxed)) {
      Cancel(output);
      return;
    }
    if (stored) {
      PendingTile->GeometryAssetKey = std::move(Geometry->Key);
      output.Tile = std::move(PendingTile);
    } else {
      output.Status = std::unexpected(stored.error());
    }
    PendingTile.reset();
    Geometry.reset();
    output.PublicationBytes = 0;
    Retries = 0;
    Current = Phase::Complete;
  }

  bool EnsureBase(const Generators::RawTile &raw,
                  const Ground::HeightField *heights,
                  std::shared_ptr<const Generators::PreparedStructureTile> &base,
                  const Generators::PreparedBuildingBasis *basis,
                  Output &output,
                  const std::atomic_bool &stopping) {
    if (base) { return true; }
    assert(basis != nullptr || heights != nullptr);
    if (basis == nullptr && !BasePublication) {
      auto prepared = Cache->PrepareBase(Key, raw, *heights, stopping, RequestKey);
      if (!prepared) {
        if (Busy(prepared.error())) {
          Defer();
          return false;
        }
        output.Status = std::unexpected(prepared.error());
        return false;
      }
      BasePublication = std::move(*prepared);
    }
    const auto attempts = BasePublication ? BasePublication->PublicationAttempts() : 0;
    auto loaded = [&]
        -> std::expected<Generators::PreparedBuildingAssets::Base, Generators::StructureBakeError> {
      if (basis != nullptr) { return Cache->Load(Key); }
      if (BasePublication) { return Cache->ResumeBase(*BasePublication, stopping); }
      return std::unexpected(Generators::StructureBakeErrorKind::ArtifactInvalidProduct);
    }();
    if (BasePublication) {
      output.PublicationAttempts += BasePublication->PublicationAttempts() - attempts;
    }
    output.PublicationBytes = BasePublication ? BasePublication->PublicationBytes() : 0;
    if (!loaded) {
      if (Busy(loaded.error())) {
        Defer();
        return false;
      }
      BasePublication.reset();
      output.PublicationBytes = 0;
      output.Status = std::unexpected(loaded.error());
      return false;
    }
    if (!*loaded && BasePublication) { return false; }
    if (!*loaded) {
      const auto invalidated = Cache->InvalidateBasis(Key);
      output.Status = std::unexpected(
          invalidated ? Generators::StructureBakeError{Generators::StructureBakeErrorKind::
                                                           NativeInputsMissing}
                      : invalidated.error());
      return false;
    }
    base = std::move(*loaded);
    BasePublication.reset();
    output.PublicationBytes = 0;
    Retries = 0;
    return true;
  }

  bool Lookup(const Generators::RawTile &raw,
              const Generators::PreparedStructureTile *base,
              const Generators::PreparedBuildingAssets::Basis &basis,
              Output &output,
              const std::atomic_bool &stopping) {
    auto cached = basis != nullptr ? Cache->LoadGeometry(Key, basis, raw)
                                   : Cache->LoadGeometry(Key, *base, raw);
    if (!cached) {
      if (Busy(cached.error())) {
        Defer();
        return true;
      }
      output.Status = std::unexpected(cached.error());
      return true;
    }
    if (stopping.load(std::memory_order_relaxed)) {
      output.Status = std::unexpected(Generators::StructureBakeErrorKind::Cancelled);
      return true;
    }
    if (!*cached) { return false; }
    output.Tile = std::move(**cached);
    output.CacheHit = true;
    Current = Phase::Complete;
    return true;
  }

  bool LookupInitial(Generators::RawTile &raw,
                     const Generators::PreparedStructureTile *base,
                     const Generators::PreparedBuildingAssets::Basis &basis,
                     Output &output,
                     const std::atomic_bool &stopping) {
    uint64_t occupied = 0;
    if (basis != nullptr) {
      for (const auto &source : basis->Sources) { occupied |= uint64_t{1} << (source.Cell - 1u); }
    } else {
      for (const auto &source : base->Structures) {
        occupied |= uint64_t{1} << (source.Layout.Cell.Index - 1u);
      }
    }
    if ((occupied & Selection) == occupied && Lookup(raw, base, basis, output, stopping)) {
      return true;
    }
    const uint64_t selected = occupied & Selection;
    Cells = selected;
    if (selected == 0) {
      if (occupied == 0) { return false; }
      output.Status = std::unexpected(Generators::StructureBakeErrorKind::ArtifactInvalidProduct);
      Current = Phase::Complete;
      return true;
    }
    raw.RequestedCell = static_cast<uint32_t>(std::countr_zero(selected)) + 1u;
    return Lookup(raw, base, basis, output, stopping);
  }

  bool BeforeBake(Generators::RawTile &raw,
                  const Ground::HeightField *heights,
                  std::shared_ptr<const Generators::PreparedStructureTile> &base,
                  const Generators::PreparedBuildingAssets::Basis &basis,
                  Output &output,
                  const std::atomic_bool &stopping) {
    if (Current == Phase::Publishing) {
      Publish(output, stopping);
      return true;
    }
    if (Current != Phase::Lookup) { return false; }
    if (basis == nullptr && !EnsureBase(raw, heights, base, basis.get(), output, stopping)) {
      return true;
    }
    if (Cells && !raw.RequestedCell) {
      if (LookupInitial(raw, base.get(), basis, output, stopping)) { return true; }
    } else if (Lookup(raw, base.get(), basis, output, stopping)) {
      return true;
    }
    if (!EnsureBase(raw, heights, base, basis.get(), output, stopping)) { return true; }
    Current = Phase::Generating;
    return false;
  }

  void AfterBake(const Generators::RawTile &raw,
                 const Generators::PreparedStructureTile &base,
                 Output &output,
                 const std::atomic_bool &stopping) {
    if (stopping.load(std::memory_order_relaxed)) {
      Cancel(output);
      return;
    }
    if (!output.Status && Busy(output.Status.error())) {
      output.Status = {};
      Defer();
      return;
    }
    if (!output.Tile || Current != Phase::Generating) { return; }
    auto prepared = Cache->PrepareGeometry(Key, base, raw, *output.Tile);
    if (!prepared) {
      output.Status = std::unexpected(prepared.error());
      output.Tile.reset();
      Current = Phase::Complete;
      return;
    }
    Geometry = std::move(*prepared);
    PendingTile = std::exchange(output.Tile, std::nullopt);
    output.PublicationBytes = Geometry->Package.CapacityBytes();
    Current = Phase::Publishing;
    Publish(output, stopping);
  }

  std::shared_ptr<Generators::PreparedBuildingAssets> Cache;
  std::string Key;
  std::string RequestKey;
  Phase Current = Phase::Lookup;
  std::optional<uint64_t> Cells;
  uint64_t Selection = ~uint64_t{0};
  std::optional<Generators::PreparedBuildingAssets::BaseWrite> BasePublication;
  std::optional<Generators::PreparedBuildingAssets::GeometryWrite> Geometry;
  std::optional<Generators::BakedTile> PendingTile;
  unsigned Retries = 0;
  std::chrono::steady_clock::time_point RetryAt;
};

struct StructureBuildTask::Payload {
  explicit Payload(JobAdmission::Lease admission) : Admission(std::move(admission)) {
    assert(Admission);
  }

  struct Completion {
    explicit Completion(std::shared_ptr<Payload> value) : Value(std::move(value)) {}

    ~Completion() {
      if (!Started) {
        if (Value->Native) {
          Value->Native->Cancel(*Value->Result);
        } else {
          Value->Result->Tile.reset();
          Value->Result->Status = std::unexpected(Generators::StructureBakeErrorKind::Cancelled);
        }
      }
      {
        const std::scoped_lock lock(Value->Mutex);
        Value->Finished.store(true, std::memory_order_release);
      }
      Value->Changed.notify_all();
    }

    Completion(const Completion &) = delete;
    Completion &operator=(const Completion &) = delete;
    std::shared_ptr<Payload> Value;
    bool Started = false;
  };

  void CollectContacts() {
    if (Native && !Native->EnsureBase(*Raw, Heights.get(), Base, Basis.get(), *Result, *Stopping)) {
      return;
    }
    if (!Base) {
      auto prepared = Generators::PrepareStructureTile(*Raw, *Heights, Stopping.get());
      if (!prepared) {
        Result->Status = std::unexpected(prepared.error());
        return;
      }
      Base = std::make_shared<const Generators::PreparedStructureTile>(std::move(*prepared));
    }
    auto metadata = Generators::StructureTerrainContacts(*Base, Stopping.get());
    if (metadata) {
      Result->Tile = std::move(*metadata);
    } else {
      Result->Status = std::unexpected(metadata.error());
    }
  }

  std::shared_ptr<const void> Lifetime;
  JobAdmission::Lease Admission;
  std::shared_ptr<const StructureMesher> Mesher;
  std::unique_ptr<Generators::RawTile> Raw;
  std::shared_ptr<const Ground::HeightField> Heights;
  std::shared_ptr<const Generators::PreparedStructureTile> Base;
  std::shared_ptr<const Generators::PreparedBuildingBasis> Basis;
  std::unique_ptr<NativeProducts> Native;
  enum class Product { Geometry, TerrainContacts };
  Product Requested = Product::Geometry;
  std::unique_ptr<Output> Result;
  std::optional<Generators::BakedTile> DiscardedTile;
  std::unique_ptr<MeshScratch> Scratch;
  std::unique_ptr<Generators::StructureBakeProgress> Progress =
      std::make_unique<Generators::StructureBakeProgress>();
  std::shared_ptr<std::atomic_bool> Stopping = std::make_shared<std::atomic_bool>(false);
  std::unique_ptr<Comparison> SurfaceComparison;
  std::mutex Mutex;
  std::condition_variable Changed;
  std::atomic_bool Finished{false};
};

StructureBuildTask::StructureBuildTask(JobAdmission::Lease admission,
                                       uint32_t tile,
                                       std::unique_ptr<Generators::RawTile> raw,
                                       std::shared_ptr<const Ground::HeightField> heights,
                                       std::unique_ptr<Output> output,
                                       std::unique_ptr<MeshScratch> scratch,
                                       std::optional<ProofRequest> proof)
    : Tile_(tile), Payload_(std::make_shared<Payload>(std::move(admission))) {
  Payload_->Raw = std::move(raw);
  Payload_->Heights = std::move(heights);
  Payload_->Result = std::move(output);
  Payload_->Scratch = std::move(scratch);
  if (proof) { Payload_->SurfaceComparison = std::make_unique<Comparison>(*proof); }
  assert(Payload_->Raw && Payload_->Heights && Payload_->Result && Payload_->Scratch);
}

StructureBuildTask::StructureBuildTask(
    JobAdmission::Lease admission,
    uint32_t tile,
    std::shared_ptr<const Generators::PreparedStructureTile> base,
    std::unique_ptr<Generators::RawTile> view,
    std::unique_ptr<Output> output,
    std::unique_ptr<MeshScratch> scratch)
    : Tile_(tile), Payload_(std::make_shared<Payload>(std::move(admission))) {
  Payload_->Raw = std::move(view);
  Payload_->Base = std::move(base);
  Payload_->Result = std::move(output);
  Payload_->Scratch = std::move(scratch);
  assert(Payload_->Base && Payload_->Raw && Payload_->Result && Payload_->Scratch);
  Payload_->Raw->AnchorEcef = Payload_->Base->AnchorEcef;
  Payload_->Raw->TileSpanM = Payload_->Base->TileSpanM;
  Payload_->Raw->Extent = Payload_->Base->Extent;
  Payload_->Raw->SourceInputs.Origin = Payload_->Base->Origin;
}

StructureBuildTask::StructureBuildTask(
    JobAdmission::Lease admission,
    uint32_t tile,
    std::shared_ptr<const Generators::PreparedBuildingBasis> basis,
    std::unique_ptr<Generators::RawTile> view,
    std::unique_ptr<Output> output,
    std::unique_ptr<MeshScratch> scratch)
    : Tile_(tile), Payload_(std::make_shared<Payload>(std::move(admission))) {
  Payload_->Raw = std::move(view);
  Payload_->Basis = std::move(basis);
  Payload_->Result = std::move(output);
  Payload_->Scratch = std::move(scratch);
  assert(Payload_->Basis && Payload_->Raw && Payload_->Result && Payload_->Scratch);
  Payload_->Raw->AnchorEcef = Payload_->Basis->AnchorEcef;
  Payload_->Raw->TileSpanM = Payload_->Basis->TileSpanM;
  Payload_->Raw->Extent = Payload_->Basis->Extent;
  Payload_->Raw->SourceInputs.Origin = Payload_->Basis->Origin;
}

const Generators::RawTile &StructureBuildTask::Raw() const noexcept {
  return *Payload_->Raw;
}

Generators::RawTile &StructureBuildTask::Raw() noexcept {
  return *Payload_->Raw;
}

const Ground::HeightField &StructureBuildTask::Heights() const noexcept {
  assert(Payload_->Heights != nullptr);
  return *Payload_->Heights;
}

const Generators::PreparedStructureTile *StructureBuildTask::PreparedBase() const noexcept {
  return Payload_ ? Payload_->Base.get() : nullptr;
}

bool StructureBuildTask::FromPreparedInputs() const noexcept {
  return Payload_ && (Payload_->Base || Payload_->Basis);
}

StructureBuildTask::Output &StructureBuildTask::Result() noexcept {
  return *Payload_->Result;
}

const StructureBuildTask::Output &StructureBuildTask::Result() const noexcept {
  return *Payload_->Result;
}

Generators::StructureBakeProgress &StructureBuildTask::Progress() noexcept {
  return *Payload_->Progress;
}

size_t StructureBuildTask::PreparedStructureCount() const noexcept {
  if (Payload_->Basis) { return Payload_->Basis->Sources.size(); }
  return Payload_->Base ? Payload_->Base->Structures.size() : Payload_->Raw->Structures.size();
}

std::span<const Data::TileSourceIdentity> StructureBuildTask::HeightSources() const noexcept {
  if (Payload_->Basis) { return Payload_->Basis->HeightSources; }
  return Payload_->Base ? std::span(Payload_->Base->HeightSources) : Heights().Sources();
}

uint64_t StructureBuildTask::HeightRasterDigest() const noexcept {
  if (Payload_->Basis) { return Payload_->Basis->HeightRasterDigest; }
  return Payload_->Base ? Payload_->Base->HeightRasterDigest : Heights().RasterDigest();
}

bool StructureBuildTask::HeightQualified() const noexcept {
  if (Payload_->Basis) { return Payload_->Basis->HeightQualified; }
  return Payload_->Base ? Payload_->Base->HeightQualified : Heights().Qualified();
}

Ground::HeightField::Request StructureBuildTask::HeightRequest() const {
  if (Payload_->Basis) { return Payload_->Basis->HeightRequest; }
  return Payload_->Base ? Payload_->Base->HeightRequest : Heights().CaptureRequest();
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
  Payload_ = std::move(other.Payload_);
  State_ = std::exchange(other.State_, State::Empty);
  return *this;
}

bool StructureBuildTask::AwaitCompletion(double seconds) const {
  if (!(seconds > 0.0) || !Payload_) { return false; }
  if (Running()) {
    std::unique_lock lock(Payload_->Mutex);
    return Payload_->Changed.wait_for(lock, std::chrono::duration<double>(seconds), [this] {
      return Payload_->Finished.load(std::memory_order_acquire);
    });
  }
  const double delay = std::min(seconds, ResumeDelaySeconds());
  if (!(delay > 0.0)) { return false; }
  std::unique_lock lock(Payload_->Mutex);
  return Payload_->Changed.wait_for(
      lock, std::chrono::duration<double>(delay), [] { return false; });
}

bool StructureBuildTask::Running() const noexcept {
  return State_ == State::Running;
}

void StructureBuildTask::UseNativeAssets(std::shared_ptr<Generators::PreparedBuildingAssets> cache,
                                         std::string key,
                                         std::string requestKey) {
  assert(State_ == State::Ready && (Payload_->Base || Payload_->Basis || Payload_->Heights));
  Payload_->Native =
      std::make_unique<NativeProducts>(std::move(cache), std::move(key), std::move(requestKey));
}

std::optional<std::string_view> StructureBuildTask::PreparedAssetKey() const noexcept {
  return Payload_ && Payload_->Native ? std::optional(std::string_view(Payload_->Native->Key))
                                      : std::nullopt;
}

void StructureBuildTask::UseTerrainContacts() {
  assert(State_ == State::Ready && !Payload_->SurfaceComparison);
  Payload_->Requested = Payload::Product::TerrainContacts;
}

void StructureBuildTask::UseInitialCells(uint64_t selected) {
  assert(State_ == State::Ready && Payload_->Native && !Payload_->Raw->RequestedCell &&
         !Payload_->Raw->RequestedDetail);
  Payload_->Native->Cells = 0;
  Payload_->Native->Selection = selected;
}

uint64_t StructureBuildTask::InitialCells() const noexcept {
  assert(State_ != State::Running);
  return Payload_ && Payload_->Native ? Payload_->Native->Cells.value_or(0) : 0;
}

void StructureBuildTask::SelectNextCell(uint32_t cell) {
  assert(State_ == State::Completed && Payload_->Native && Payload_->Native->Cells &&
         !Payload_->SurfaceComparison && Payload_->Result->Status && Payload_->Result->Tile &&
         !Payload_->Stopping->load(std::memory_order_relaxed));
  assert(cell > 0 && cell <= Generators::kStructureCellsPerTile &&
         (InitialCells() & (uint64_t{1} << (cell - 1u))) != 0);
  Payload_->Raw->RequestedCell = cell;
  *Payload_->Result = {};
  Payload_->Progress = std::make_unique<Generators::StructureBakeProgress>();
  Payload_->Native->Current = NativeProducts::Phase::Lookup;
  State_ = State::Ready;
}

void StructureBuildTask::Posts(Tasks &pool) {
  assert(State_ != State::Running);
  const auto posted = std::chrono::steady_clock::now();
  Payload_->Finished.store(false, std::memory_order_relaxed);
  State_ = State::Running;
  const auto completion = std::make_shared<Payload::Completion>(Payload_);
  (void)pool.PostDetached([completion, posted] {
    completion->Started = true;
    const auto &payload = completion->Value;
    Generators::RawTile *const raw = payload->Raw.get();
    const Ground::HeightField *const heights = payload->Heights.get();
    auto *const base = &payload->Base;
    const auto &basis = payload->Basis;
    NativeProducts *const native = payload->Native.get();
    const auto &mesher = payload->Mesher;
    MeshScratch *const scratch = payload->Scratch.get();
    Generators::StructureBakeProgress *const progress = payload->Progress.get();
    Output *const output = payload->Result.get();
    Comparison *const comparison = payload->SurfaceComparison.get();
    const auto &stopping = payload->Stopping;
    const auto began = std::chrono::steady_clock::now();
    output->LastQueueMs = std::chrono::duration<double, std::milli>(began - posted).count();
    static const Heap::Tag kBakingTag("structure-bake");
    const Heap::Tagged baking(kBakingTag);
    output->LastRanges = 0;
    output->LastProofWork = 0;
    output->LastRangeMs = 0.0;
    if (stopping->load(std::memory_order_relaxed)) {
      if (native != nullptr) {
        native->Cancel(*output);
      } else {
        output->Status = std::unexpected(Generators::StructureBakeErrorKind::Cancelled);
      }
      return;
    }
    const bool contacts = payload->Requested == Payload::Product::TerrainContacts;
    if (contacts) {
      payload->CollectContacts();
    } else if (native && native->BeforeBake(*raw, heights, *base, basis, *output, *stopping)) {
      output->LastTaskMs =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
              .count();
      output->BakeMs += output->LastTaskMs;
      return;
    }
    if (!contacts && *base) {
      BakeVariant(raw, heights, *mesher, scratch, progress, output, stopping, base->get());
    } else if (!contacts && comparison) {
      comparison->Bake(*raw, *heights, *mesher, *scratch, *progress, *output, stopping);
    } else if (!contacts) {
      BakeVariant(raw, heights, *mesher, scratch, progress, output, stopping);
    }
    if (native != nullptr && !contacts) { native->AfterBake(*raw, **base, *output, *stopping); }
    if (comparison != nullptr) { Comparison::CancelIfStopping(*output, *stopping); }
    const double taskMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    output->LastTaskMs = taskMs;
    output->BakeMs += taskMs;
  });
}

void StructureBuildTask::Start(Tasks &pool,
                               std::shared_ptr<const StructureMesher> mesher,
                               std::shared_ptr<const void> lifetime) {
  assert(State_ == State::Ready);
  assert(mesher != nullptr);
  Payload_->Lifetime = std::move(lifetime);
  Payload_->Mesher = std::move(mesher);
  Posts(pool);
}

void StructureBuildTask::Resume(Tasks &pool) {
  assert(State_ == State::Completed && Payload_->Result->Status && !Payload_->Result->Tile);
  Posts(pool);
}

bool StructureBuildTask::CanResume() const noexcept {
  return State_ == State::Completed && Payload_->Result->Status && !Payload_->Result->Tile &&
         (Payload_->Stopping->load(std::memory_order_relaxed) || !Payload_->Native ||
          Payload_->Native->ReadyToResume());
}

double StructureBuildTask::ResumeDelaySeconds() const noexcept {
  assert(State_ != State::Running);
  if (State_ != State::Completed || !Payload_->Native || !Payload_->Result->Status ||
      Payload_->Result->Tile || Payload_->Stopping->load(std::memory_order_relaxed)) {
    return 0;
  }
  return std::max(
      0.0,
      std::chrono::duration<double>(Payload_->Native->RetryAt - std::chrono::steady_clock::now())
          .count());
}

void StructureBuildTask::RequestStop() noexcept {
  if (Payload_) { Payload_->Stopping->store(true, std::memory_order_relaxed); }
}

bool StructureBuildTask::TakeCompletion() {
  if (State_ != State::Running || !Payload_->Finished.load(std::memory_order_acquire)) {
    return false;
  }
  State_ = State::Completed;
  return true;
}

void StructureBuildTask::Join() {
  if (State_ != State::Running) { return; }
  std::unique_lock lock(Payload_->Mutex);
  Payload_->Changed.wait(lock,
                         [this] { return Payload_->Finished.load(std::memory_order_acquire); });
  State_ = State::Completed;
}

bool StructureBuildTask::Retire(Tasks &pool) {
  if (!Payload_) { return true; }
  RequestStop();

  struct Handoff {
    std::shared_ptr<Payload> Value;
  };

  const auto handoff = std::make_shared<Handoff>(std::move(Payload_));
  if (!pool.PostDetached([handoff] {
        auto retired = std::move(handoff->Value);
        retired.reset();
      })) {
    Payload_ = std::move(handoff->Value);
    return false;
  }
  Tile_ = 0;
  State_ = State::Empty;
  return true;
}

std::unique_ptr<Generators::RawTile> StructureBuildTask::TakeRaw() noexcept {
  assert(State_ != State::Running);
  return Payload_ ? std::move(Payload_->Raw) : nullptr;
}

std::unique_ptr<StructureBuildTask::Output> StructureBuildTask::TakeOutputForReuse() noexcept {
  assert(State_ != State::Running);
  if (!Payload_ || !Payload_->Result) { return nullptr; }
  Payload_->DiscardedTile = std::move(Payload_->Result->Tile);
  Payload_->Result->Tile.reset();
  return std::move(Payload_->Result);
}

std::unique_ptr<MeshScratch> StructureBuildTask::TakeScratch() noexcept {
  assert(State_ != State::Running);
  return Payload_ ? std::move(Payload_->Scratch) : nullptr;
}

}
