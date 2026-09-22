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

ShapeCookJob::ShapeCookJob(ShapeStore &into, std::span<const Material> surfaces) noexcept
    : Into_(&into), Surfaces_(surfaces) {}

bool ShapeCookJob::Ready() const noexcept {
  return Stage_ == Stage::Complete;
}

ClusterError ShapeCookJob::Fail(ClusterError error) noexcept {
  Failure_ = error;
  Stage_ = Stage::Failed;
  return error;
}

void ShapeCookJob::Completes() noexcept {
  Into_->Clustering.RootClusters = RootClusters_;
  Into_->Clustering.Clusters = Into_->Clusters.size();
  Stage_ = Stage::Complete;
}

std::expected<ShapeCookJob::Flow, ClusterError> ShapeCookJob::BeginsPart() {
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
  LocalIndices_.resize(part.IndexCount);
  Cursor_ = 0;
  Stage_ = Stage::Rebase;
  return Flow::Continue;
}

std::expected<ShapeCookJob::Flow, ClusterError> ShapeCookJob::AdvancesRebase(size_t &itemsLeft) {
  const ShapePart &part = Into_->Parts[Part_];
  const size_t count = std::min(itemsLeft, LocalIndices_.size() - Cursor_);
  const size_t end = Cursor_ + count;
  for (; Cursor_ < end; ++Cursor_) {
    const uint32_t index = Into_->Indices[part.FirstIndex + Cursor_];
    if (index < part.FirstVertex || index - part.FirstVertex >= part.VertexCount) {
      return std::unexpected(Fail(ClusterError::InvalidIndex));
    }
    LocalIndices_[Cursor_] = index - static_cast<uint32_t>(part.FirstVertex);
  }
  itemsLeft -= count;
  if (Cursor_ != LocalIndices_.size()) { return Flow::Yield; }
  if (LocalIndices_.empty()) {
    ++Part_;
    Stage_ = Stage::Part;
    return Flow::Continue;
  }
  KeepsClusters_ = part.Material < 0 || static_cast<size_t>(part.Material) >= Surfaces_.size();
  if (!KeepsClusters_) {
    const SurfaceKind kind = StateOf(Surfaces_[static_cast<size_t>(part.Material)]).Kind();
    KeepsClusters_ = kind == SurfaceKind::Opaque || kind == SurfaceKind::Masked;
  }
  const auto triangleLimit =
      KeepsClusters_ ? kClusterTriangles
                     : static_cast<uint32_t>(std::max(size_t{1}, LocalIndices_.size() / 3));
  Cook_.emplace(ClusterMeshInput{.PositionsM = part.PositionsM, .Indices = LocalIndices_},
                triangleLimit);
  Stage_ = Stage::Cook;
  return Flow::Continue;
}

void ShapeCookJob::KeepsCooked(ClusteredMesh cooked) {
  if (!KeepsClusters_) { return; }
  ShapePart &part = Into_->Parts[Part_];
  for (size_t at = 0; at < cooked.Index.size(); ++at) {
    Into_->Indices[part.FirstIndex + at] =
        cooked.Index[at] + static_cast<uint32_t>(part.FirstVertex);
  }
  for (DagCluster held : cooked.Clusters) {
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
  part.ClusterCount = static_cast<uint32_t>(cooked.Clusters.size());
}

std::expected<ShapeCookJob::Flow, ClusterError> ShapeCookJob::AdvancesCook(size_t &itemsLeft) {
  if (!Cook_) { return std::unexpected(Fail(ClusterError::InvalidLayout)); }
  ClusterCookJob &cook = *Cook_;
  const auto advanced = cook.AdvanceWithin(itemsLeft);
  if (!advanced) { return std::unexpected(Fail(advanced.error())); }
  if (!*advanced) { return Flow::Yield; }
  KeepsCooked(cook.Take());
  Cook_.reset();
  ++Part_;
  Stage_ = Stage::Part;
  return Flow::Continue;
}

std::expected<ShapeCookJob::Flow, ClusterError> ShapeCookJob::AdvanceWork(size_t &itemsLeft) {
  for (;;) {
    switch (Stage_) {
      case Stage::Layout:
        Into_->Clustering = {};
        Into_->Clusters.clear();
        Into_->ClusterSpheres.clear();
        if (Into_->Indices.size() > std::numeric_limits<uint32_t>::max()) {
          return std::unexpected(Fail(ClusterError::CapacityExceeded));
        }
        Stage_ = Stage::Part;
        break;
      case Stage::Part: {
        const auto began = BeginsPart();
        if (!began) { return std::unexpected(began.error()); }
        if (*began == Flow::Complete) { return Flow::Complete; }
        break;
      }
      case Stage::Rebase: {
        const auto rebased = AdvancesRebase(itemsLeft);
        if (!rebased) { return std::unexpected(rebased.error()); }
        if (*rebased != Flow::Continue) { return *rebased; }
        break;
      }
      case Stage::Cook: {
        const auto cooked = AdvancesCook(itemsLeft);
        if (!cooked) { return std::unexpected(cooked.error()); }
        if (*cooked != Flow::Continue) { return *cooked; }
        break;
      }
      case Stage::Complete: return Flow::Complete;
      case Stage::Failed: return std::unexpected(Failure_);
    }
  }
}

std::expected<bool, ClusterError> ShapeCookJob::Advance(size_t itemsMost) {
  if (Stage_ == Stage::Failed) { return std::unexpected(Failure_); }
  if (Ready()) { return true; }
  const auto began = std::chrono::steady_clock::now();
  size_t itemsLeft = itemsMost;
  const auto advanced = AdvanceWork(itemsLeft);
  WorkMs_ +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  Into_->Clustering.BuildMs = WorkMs_;
  if (!advanced) { return std::unexpected(advanced.error()); }
  return *advanced == Flow::Complete;
}

std::expected<void, ClusterError> CookShape(ShapeStore &into, std::span<const Material> surfaces) {
  ShapeCookJob job(into, surfaces);
  for (;;) {
    const auto advanced = job.Advance(65536);
    if (!advanced) { return std::unexpected(advanced.error()); }
    if (*advanced) { return {}; }
  }
}
}
