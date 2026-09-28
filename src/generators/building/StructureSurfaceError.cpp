#include "StructureSurfaceError.h"
#include "TriangleDistance.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <vector>

namespace outshine::Generators {
namespace {

constexpr size_t kMaximumTriangles = 65536;

[[nodiscard]] size_t IndexCount(const Raised &mesh) noexcept {
  return mesh.WallRun.size() + mesh.RoofRun.size();
}

[[nodiscard]] bool ValidShape(const Raised &mesh) noexcept {
  return mesh.WallRun.size() % 3 == 0 && mesh.RoofRun.size() % 3 == 0 &&
         mesh.WallRun.size() / 3 <= kMaximumTriangles &&
         mesh.RoofRun.size() / 3 <= kMaximumTriangles - mesh.WallRun.size() / 3;
}

[[nodiscard]] std::optional<Vec3> IndexedPoint(const Raised &mesh, size_t cursor) noexcept {
  const bool wall = cursor < mesh.WallRun.size();
  const auto &indices = wall ? mesh.WallRun : mesh.RoofRun;
  const auto &vertices = wall ? mesh.WallCorners : mesh.RoofCorners;
  const size_t at = wall ? cursor : cursor - mesh.WallRun.size();
  if (at >= indices.size() || indices[at] >= vertices.size()) { return std::nullopt; }
  const auto &p = vertices[indices[at]].pos;
  const Vec3 point{{p[0], p[1], p[2]}};
  for (size_t axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(point[axis])) { return std::nullopt; }
  }
  return point;
}

}

double StructureSurfaceErrorBound::UpperDistanceM() const noexcept {
  return std::max(ReferenceToVariantM, VariantToReferenceM);
}

std::expected<void, StructureSurfaceErrorFailure>
StructureSurfaceErrorTask::Reset(StructureSurfacePair inputs) noexcept {
  Reference_ = &inputs.Reference;
  Variant_ = &inputs.Variant;
  SourceKey_ = inputs.SourceKey;
  Cursor_ = 0;
  WorkUnits_ = 0;
  VertexDistanceQueries_ = 0;
  Bound_ = {.SourceKey = SourceKey_};
  Phase_ = Phase::Failed;
  Failure_ = StructureSurfaceErrorFailure::InvalidSource;
  if (SourceKey_ == 0) { return std::unexpected(Failure_); }
  Failure_ = StructureSurfaceErrorFailure::InvalidGeometry;
  if (!ValidShape(*Reference_) || !ValidShape(*Variant_)) { return std::unexpected(Failure_); }
  Failure_ = StructureSurfaceErrorFailure::EmptyMismatch;
  if ((IndexCount(*Reference_) == 0) != (IndexCount(*Variant_) == 0)) {
    return std::unexpected(Failure_);
  }
  Phase_ = IndexCount(*Reference_) == 0 ? Phase::Complete : Phase::ValidateReference;
  return {};
}

void StructureSurfaceErrorTask::AdvancePhase() noexcept {
  Cursor_ = 0;
  switch (Phase_) {
    case Phase::ValidateReference: Phase_ = Phase::ValidateVariant; break;
    case Phase::ValidateVariant: Phase_ = Phase::BoundReference; break;
    case Phase::BoundReference: Phase_ = Phase::BoundVariant; break;
    case Phase::BoundVariant: Phase_ = Phase::Complete; break;
    case Phase::Complete:
    case Phase::Failed: break;
  }
}

std::expected<void, StructureSurfaceErrorFailure>
StructureSurfaceErrorTask::ProcessCorner() noexcept {
  const bool reference = Phase_ == Phase::ValidateReference || Phase_ == Phase::BoundReference;
  const Raised &source = reference ? *Reference_ : *Variant_;
  const auto point = IndexedPoint(source, Cursor_);
  ++WorkUnits_;
  if (!point) { return std::unexpected(StructureSurfaceErrorFailure::InvalidGeometry); }
  if (Phase_ == Phase::BoundReference || Phase_ == Phase::BoundVariant) {
    const Raised &target = reference ? *Variant_ : *Reference_;
    const auto anchor = IndexedPoint(target, 0);
    const auto distance = anchor ? BoundPointDistance(*point, *anchor) : std::nullopt;
    ++VertexDistanceQueries_;
    if (!distance) { return std::unexpected(StructureSurfaceErrorFailure::NonfiniteDistance); }
    double &bound = reference ? Bound_.ReferenceToVariantM : Bound_.VariantToReferenceM;
    bound = std::max(bound, *distance);
  }
  ++Cursor_;
  if (Cursor_ == IndexCount(source)) { AdvancePhase(); }
  return {};
}

std::expected<StructureSurfaceErrorProgress, StructureSurfaceErrorFailure>
StructureSurfaceErrorTask::Step(uint64_t currentSourceKey,
                                StructureSurfaceWorkBudget budget) noexcept {
  if (Phase_ == Phase::Failed) { return std::unexpected(Failure_); }
  if (currentSourceKey != SourceKey_) {
    Phase_ = Phase::Failed;
    Failure_ = StructureSurfaceErrorFailure::SourceChanged;
    return std::unexpected(Failure_);
  }
  while (budget.MaxCorners > 0 && Phase_ != Phase::Complete) {
    --budget.MaxCorners;
    const auto processed = ProcessCorner();
    if (!processed) {
      Phase_ = Phase::Failed;
      Failure_ = processed.error();
      return std::unexpected(Failure_);
    }
  }
  return Phase_ == Phase::Complete ? StructureSurfaceErrorProgress::Complete
                                   : StructureSurfaceErrorProgress::Pending;
}

void StructureSurfaceErrorTask::Cancel() noexcept {
  Phase_ = Phase::Failed;
  Failure_ = StructureSurfaceErrorFailure::Cancelled;
}

std::optional<StructureSurfaceErrorBound>
StructureSurfaceErrorTask::Bound(uint64_t currentSourceKey) const noexcept {
  if (Phase_ != Phase::Complete || SourceKey_ == 0 || currentSourceKey != SourceKey_) {
    return std::nullopt;
  }
  return Bound_;
}

}
