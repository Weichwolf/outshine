#include "MeshClusterBuildJob.h"
#include "Shape.h"
#include "SurfaceState.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <ratio>
#include <span>
#include <utility>

namespace outshine::Render {

MeshClusterBuildJob::MeshClusterBuildJob(ShapeStore &into,
                                         std::span<const Material> surfaces) noexcept
    : Into_(&into), Surfaces_(surfaces) {}

bool MeshClusterBuildJob::Ready() const noexcept {
  return Stage_ == Stage::Complete;
}

ClusterError MeshClusterBuildJob::Fail(ClusterError error) noexcept {
  Failure_ = error;
  Stage_ = Stage::Failed;
  return error;
}

void MeshClusterBuildJob::Completes() noexcept {
  Into_->Clustering.RootClusters = RootClusters_;
  Into_->Clustering.Clusters = Into_->Clusters.size();
  Stage_ = Stage::Complete;
}

bool MeshClusterBuildJob::KeepsClusters(const ShapePart &part) const {
  if (part.Material < 0 || static_cast<size_t>(part.Material) >= Surfaces_.size()) { return true; }
  const SurfaceKind kind = StateOf(Surfaces_[static_cast<size_t>(part.Material)]).Kind();
  return kind == SurfaceKind::Opaque || kind == SurfaceKind::Masked;
}

std::expected<MeshClusterBuildJob::Flow, ClusterError>
MeshClusterBuildJob::PlanParts(size_t &itemsLeft) {
  while (Part_ < Into_->Parts.size() && itemsLeft > 0) {
    const ShapePart &part = Into_->Parts[Part_];
    if (part.FirstIndex > Into_->Indices.size() ||
        part.IndexCount > Into_->Indices.size() - part.FirstIndex) {
      return std::unexpected(Fail(ClusterError::InvalidLayout));
    }
    if (KeepsClusters(part)) {
      const size_t triangles = part.IndexCount / 3;
      const size_t clusters =
          triangles / kClusterTriangles + (triangles % kClusterTriangles != 0 ? 1u : 0u);
      if (clusters > std::numeric_limits<uint32_t>::max() - PlannedClusters_) {
        return std::unexpected(Fail(ClusterError::CapacityExceeded));
      }
      PlannedClusters_ += clusters;
    }
    ++Part_;
    --itemsLeft;
  }
  if (Part_ < Into_->Parts.size()) { return Flow::Yield; }
  constexpr size_t kSphereFloats = 12;
  if (PlannedClusters_ > Into_->ClusterSpheres.max_size() / kSphereFloats) {
    return std::unexpected(Fail(ClusterError::CapacityExceeded));
  }
  Into_->Clusters.reserve(PlannedClusters_);
  Into_->ClusterSpheres.reserve(PlannedClusters_ * kSphereFloats);
  Part_ = 0;
  Stage_ = Stage::Part;
  return Flow::Continue;
}

std::expected<MeshClusterBuildJob::Flow, ClusterError> MeshClusterBuildJob::BeginsPart() {
  if (Part_ == Into_->Parts.size()) {
    Completes();
    return Flow::Complete;
  }
  ShapePart &part = Into_->Parts[Part_];
  part.FirstCluster = static_cast<uint32_t>(Into_->Clusters.size());
  part.ClusterCount = 0;
  if (part.FirstIndex > Into_->Indices.size() ||
      part.IndexCount > Into_->Indices.size() - part.FirstIndex ||
      part.PositionsM.size() % 3 != 0 || part.VertexCount != part.PositionsM.size() / 3 ||
      part.FirstVertex > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(Fail(ClusterError::InvalidLayout));
  }
  LocalIndices_.clear();
  LocalIndices_.reserve(part.IndexCount);
  Cursor_ = 0;
  Stage_ = Stage::Rebase;
  return Flow::Continue;
}

std::expected<MeshClusterBuildJob::Flow, ClusterError>
MeshClusterBuildJob::AdvancesRebase(size_t &itemsLeft) {
  const ShapePart &part = Into_->Parts[Part_];
  const size_t count = std::min(itemsLeft, part.IndexCount - Cursor_);
  const size_t end = Cursor_ + count;
  for (; Cursor_ < end; ++Cursor_) {
    const uint32_t index = Into_->Indices[part.FirstIndex + Cursor_];
    if (index < part.FirstVertex || index - part.FirstVertex >= part.VertexCount) {
      return std::unexpected(Fail(ClusterError::InvalidIndex));
    }
    LocalIndices_.push_back(index - static_cast<uint32_t>(part.FirstVertex));
  }
  itemsLeft -= count;
  if (Cursor_ != part.IndexCount) { return Flow::Yield; }
  if (LocalIndices_.empty()) {
    ++Part_;
    Stage_ = Stage::Part;
    return Flow::Continue;
  }
  KeepsClusters_ = KeepsClusters(part);
  const auto triangleLimit =
      KeepsClusters_ ? kClusterTriangles
                     : static_cast<uint32_t>(std::max(size_t{1}, LocalIndices_.size() / 3));
  Cook_.emplace(ClusterMeshInput{.PositionsM = part.PositionsM, .Indices = LocalIndices_},
                triangleLimit);
  Stage_ = Stage::Cook;
  return Flow::Continue;
}

MeshClusterBuildJob::Flow MeshClusterBuildJob::PublishIndices(size_t &itemsLeft) {
  const ShapePart &part = Into_->Parts[Part_];
  const size_t count = std::min(itemsLeft, Built_.Index.size() - Cursor_);
  const size_t end = Cursor_ + count;
  for (; Cursor_ < end; ++Cursor_) {
    Into_->Indices[part.FirstIndex + Cursor_] =
        Built_.Index[Cursor_] + static_cast<uint32_t>(part.FirstVertex);
  }
  itemsLeft -= count;
  if (Cursor_ != Built_.Index.size()) { return Flow::Yield; }
  Cursor_ = 0;
  Stage_ = Stage::Clusters;
  return Flow::Continue;
}

MeshClusterBuildJob::Flow MeshClusterBuildJob::PublishClusters(size_t &itemsLeft) {
  ShapePart &part = Into_->Parts[Part_];
  const size_t count = std::min(itemsLeft, Built_.Clusters.size() - Cursor_);
  const size_t end = Cursor_ + count;
  for (; Cursor_ < end; ++Cursor_) {
    DagCluster held = Built_.Clusters[Cursor_];
    held.First += static_cast<uint32_t>(part.FirstIndex);
    if (held.ParentErr >= kDagRootErr) { ++RootClusters_; }
    Into_->Clusters.push_back(held);
    Into_->ClusterSpheres.insert(Into_->ClusterSpheres.end(),
                                 {held.SelfCenter[0],
                                  held.SelfCenter[1],
                                  held.SelfCenter[2],
                                  held.SelfRadius,
                                  held.ParentCenter[0],
                                  held.ParentCenter[1],
                                  held.ParentCenter[2],
                                  held.ParentRadius,
                                  held.SelfErr,
                                  held.ParentErr,
                                  0.0f,
                                  0.0f});
  }
  itemsLeft -= count;
  if (Cursor_ != Built_.Clusters.size()) { return Flow::Yield; }
  part.ClusterCount = static_cast<uint32_t>(Built_.Clusters.size());
  Built_ = {};
  ++Part_;
  Stage_ = Stage::Part;
  return Flow::Continue;
}

std::expected<MeshClusterBuildJob::Flow, ClusterError>
MeshClusterBuildJob::AdvancesCook(size_t &itemsLeft) {
  if (!Cook_) { return std::unexpected(Fail(ClusterError::InvalidLayout)); }
  ClusterCookJob &cook = *Cook_;
  const auto advanced = cook.AdvanceWithin(itemsLeft);
  if (!advanced) { return std::unexpected(Fail(advanced.error())); }
  if (!*advanced) { return Flow::Yield; }
  Built_ = cook.Take();
  Cook_.reset();
  Cursor_ = 0;
  if (KeepsClusters_) {
    Stage_ = Stage::Indices;
    return Flow::Continue;
  }
  Built_ = {};
  ++Part_;
  Stage_ = Stage::Part;
  return Flow::Continue;
}

std::expected<MeshClusterBuildJob::Flow, ClusterError>
MeshClusterBuildJob::AdvanceStage(size_t &itemsLeft) {
  switch (Stage_) {
    case Stage::Layout:
      Into_->Clustering = {};
      Into_->Clusters.clear();
      Into_->ClusterSpheres.clear();
      if (Into_->Indices.size() > std::numeric_limits<uint32_t>::max()) {
        return std::unexpected(Fail(ClusterError::CapacityExceeded));
      }
      Stage_ = Stage::Plan;
      return Flow::Continue;
    case Stage::Plan: return PlanParts(itemsLeft);
    case Stage::Part:
      if (itemsLeft == 0) { return Flow::Yield; }
      --itemsLeft;
      return BeginsPart();
    case Stage::Rebase: return AdvancesRebase(itemsLeft);
    case Stage::Cook: return AdvancesCook(itemsLeft);
    case Stage::Indices: return PublishIndices(itemsLeft);
    case Stage::Clusters: return PublishClusters(itemsLeft);
    case Stage::Complete: return Flow::Complete;
    case Stage::Failed: return std::unexpected(Failure_);
  }
  return std::unexpected(Fail(ClusterError::InvalidLayout));
}

std::expected<MeshClusterBuildJob::Flow, ClusterError>
MeshClusterBuildJob::AdvanceWork(size_t &itemsLeft) {
  for (;;) {
    const auto advanced = AdvanceStage(itemsLeft);
    if (!advanced || *advanced != Flow::Continue) { return advanced; }
  }
}

std::expected<bool, ClusterError> MeshClusterBuildJob::Advance(size_t itemsMost) {
  if (Stage_ == Stage::Failed) { return std::unexpected(Failure_); }
  if (Ready()) { return true; }
  if (itemsMost == 0) { return false; }
  const auto began = std::chrono::steady_clock::now();
  size_t itemsLeft = itemsMost;
  const auto advanced = AdvanceWork(itemsLeft);
  WorkMs_ +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  Into_->Clustering.BuildMs = WorkMs_;
  if (!advanced) { return std::unexpected(advanced.error()); }
  return *advanced == Flow::Complete;
}

std::expected<void, ClusterError> BuildMeshClusters(ShapeStore &into,
                                                    std::span<const Material> surfaces) {
  MeshClusterBuildJob job(into, surfaces);
  for (;;) {
    const auto advanced = job.Advance(65536);
    if (!advanced) { return std::unexpected(advanced.error()); }
    if (*advanced) { return {}; }
  }
}
}
