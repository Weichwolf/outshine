#include "StructureSourceErrorTask.h"
#include "PreparedStructurePlan.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <utility>

namespace outshine::Generators {
std::expected<void, StructureSurfaceErrorFailure>
StructureSourceErrorTask::Reset(const PreparedStructureTile &base,
                                const RawTile &view,
                                uint64_t sourceKey,
                                StructureSurfaceRefinementLimits limits) {
  Base_ = nullptr;
  Result_ = {};
  if (sourceKey == 0 || view.RequestedDetail != LevelOfDetail::Shell ||
      base.Structures.size() > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(StructureSurfaceErrorFailure::InvalidSource);
  }
  Base_ = &base;
  View_ = {};
  View_.AnchorEcef = base.AnchorEcef;
  View_.TileSpanM = base.TileSpanM;
  View_.Extent = base.Extent;
  View_.Eye = view.Eye;
  View_.EyeEcef = view.EyeEcef;
  View_.Projection = view.Projection;
  View_.ClusterTriangles = view.ClusterTriangles;
  View_.RequestedCell = view.RequestedCell;
  Detail_ = *view.RequestedDetail;
  Limits_ = limits;
  Limits_.TargetUncertaintyM *= 0.5;
  Result_.Upper.SourceKey = sourceKey;
  Cursor_ = 0;
  Reference_.reset();
  Variant_.reset();
  Phase_ = Phase::Source;
  return {};
}

std::expected<void, StructureSurfaceErrorFailure> StructureSourceErrorTask::BeginMesh() {
  const std::array selected{Source_};
  auto progress = StructureBakeProgress::ForSources(selected);
  if (!progress) { return std::unexpected(StructureSurfaceErrorFailure::InvalidSource); }
  Progress_ = std::move(*progress);
  View_.RequestedDetail = Phase_ == Phase::Reference ? LevelOfDetail::Fine : Detail_;
  return {};
}

std::expected<void, StructureSurfaceErrorFailure> StructureSourceErrorTask::SelectSource() {
  while (Cursor_ < Base_->Structures.size()) {
    Source_ = static_cast<uint32_t>(Cursor_++);
    if (View_.RequestedCell &&
        Base_->Structures[Source_].Layout.Cell.Index != *View_.RequestedCell) {
      continue;
    }
    if (KnownEmptyPreparedStructure(*Base_, Source_)) { continue; }
    Phase_ = Phase::Reference;
    return BeginMesh();
  }
  Phase_ = Phase::Complete;
  return {};
}

std::expected<void, StructureSurfaceErrorFailure> StructureSourceErrorTask::AdvanceMesh(
    const StructureMesher &mesher, MeshScratch &scratch, const std::atomic_bool &stopping) {
  const auto advanced = Progress_.AdvancePrepared(*Base_, View_, mesher, scratch, 1, &stopping);
  if (!advanced) { return std::unexpected(StructureSurfaceErrorFailure::InvalidGeometry); }
  if (!*advanced) { return {}; }
  auto baked = Progress_.Finalize(View_, mesher, scratch, &stopping);
  if (!baked || baked->UnsupportedMeshes != 0 || baked->SkippedRings != 0 || baked->NoGround != 0 ||
      baked->FallbackHeights) {
    return std::unexpected(StructureSurfaceErrorFailure::InvalidGeometry);
  }
  if (Phase_ == Phase::Reference) {
    Reference_ = std::move(*baked);
    Phase_ = Phase::Variant;
    return BeginMesh();
  }
  if (!Reference_) { return std::unexpected(StructureSurfaceErrorFailure::InvalidGeometry); }
  const auto &variant = Variant_.emplace(std::move(*baked));
  const auto reset = Surface_.Reset({.Reference = Reference_->Built,
                                     .Variant = variant.Built,
                                     .SourceKey = Result_.Upper.SourceKey},
                                    Limits_);
  if (!reset) { return reset; }
  Phase_ = Phase::Surface;
  return {};
}

std::expected<void, StructureSurfaceErrorFailure> StructureSourceErrorTask::AdvanceSurface() {
  const auto advanced = Surface_.Step(Result_.Upper.SourceKey, {.MaxWorkUnits = 128});
  if (!advanced) { return std::unexpected(advanced.error()); }
  if (*advanced != StructureSurfaceErrorProgress::Complete) { return {}; }
  const auto bound = Surface_.Bound(Result_.Upper.SourceKey);
  if (!bound) { return std::unexpected(StructureSurfaceErrorFailure::InvalidGeometry); }
  Result_.Upper.ReferenceToVariantM =
      std::max(Result_.Upper.ReferenceToVariantM, bound->Upper.ReferenceToVariantM);
  Result_.Upper.VariantToReferenceM =
      std::max(Result_.Upper.VariantToReferenceM, bound->Upper.VariantToReferenceM);
  Reference_.reset();
  Variant_.reset();
  Phase_ = Phase::Source;
  return {};
}

std::expected<StructureSurfaceErrorProgress, StructureSurfaceErrorFailure>
StructureSourceErrorTask::Step(const StructureMesher &mesher,
                               MeshScratch &scratch,
                               const std::atomic_bool &stopping) {
  if (Base_ == nullptr) { return std::unexpected(StructureSurfaceErrorFailure::InvalidSource); }
  if (stopping.load(std::memory_order_relaxed)) {
    return std::unexpected(StructureSurfaceErrorFailure::Cancelled);
  }
  auto advanced = [&] -> std::expected<void, StructureSurfaceErrorFailure> {
    switch (Phase_) {
      case Phase::Source: return SelectSource();
      case Phase::Reference:
      case Phase::Variant: return AdvanceMesh(mesher, scratch, stopping);
      case Phase::Surface: return AdvanceSurface();
      case Phase::Complete: return {};
    }
    return std::unexpected(StructureSurfaceErrorFailure::InvalidSource);
  }();
  if (!advanced) { return std::unexpected(advanced.error()); }
  return Phase_ == Phase::Complete ? StructureSurfaceErrorProgress::Complete
                                   : StructureSurfaceErrorProgress::Pending;
}

std::optional<StructureSurfaceErrorInterval>
StructureSourceErrorTask::Bound(uint64_t sourceKey) const {
  if (sourceKey == 0 || Result_.Upper.SourceKey != sourceKey || Phase_ != Phase::Complete) {
    return std::nullopt;
  }
  return Result_;
}
}
