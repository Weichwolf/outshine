#include "EntityState.h"
#include "BodySnapshotData.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <utility>
#include <vector>
#include <algorithm>
#include <memory>

namespace outshine::World {
namespace {
constexpr uint32_t kNoBody = std::numeric_limits<uint32_t>::max();
}

bool EntityState::Open(size_t capacity) {
  if (Registry_.capacity() != 0) { return false; }
  if (capacity == 0) { return true; }
  return Registry_.open(capacity) && Traits_.Open(Registry_);
}

std::expected<void, EntityStateError> EntityState::SetBodies(std::vector<EntityBodyState> bodies) {
  if (bodies.size() > Registry_.capacity() || bodies.size() >= kNoBody) {
    return std::unexpected(EntityStateError::CapacityExceeded);
  }
  std::vector<uint32_t> index;
  if (!bodies.empty()) { index.assign(Registry_.capacity(), kNoBody); }
  for (size_t row = 0; row < bodies.size(); ++row) {
    const Entity owner = bodies[row].Owner;
    if (!Registry_.alive(owner)) { return std::unexpected(EntityStateError::InvalidEntity); }
    if (index[owner.Index] != kNoBody) { return std::unexpected(EntityStateError::DuplicateBody); }
    index[owner.Index] = static_cast<uint32_t>(row);
  }
  Bodies_ = std::move(bodies);
  BodyOfEntity_ =
      index.empty() ? nullptr : std::make_shared<const std::vector<uint32_t>>(std::move(index));
  MarkAllBodiesDirty();
  return {};
}

const RigidBodyState *EntityState::Body(Entity entity) const noexcept {
  if (!Registry_.alive(entity) || !BodyOfEntity_ || entity.Index >= BodyOfEntity_->size()) {
    return nullptr;
  }
  const uint32_t row = (*BodyOfEntity_)[entity.Index];
  if (row >= Bodies_.size() || Bodies_[row].Owner != entity) { return nullptr; }
  return &Bodies_[row].Motion;
}

void EntityState::MarkAllBodiesDirty() {
  DirtyPages_.assign((Bodies_.size() + kBodiesPerSnapshotPage - 1) / kBodiesPerSnapshotPage,
                     ~uint64_t{0});
  const size_t tail = Bodies_.size() % kBodiesPerSnapshotPage;
  if (tail != 0) { DirtyPages_.back() = (uint64_t{1} << tail) - 1; }
}

void EntityState::MarkBodyEdited(size_t row) {
  if (!PublishedBodies_.Data_ || PublishedBodies_.Data_->RowOfEntity != BodyOfEntity_) { return; }
  const auto &now = Bodies_[row].Motion;
  const auto &old = PublishedBodies_.At(row).Motion;
  const bool same =
      now.MassKg == old.MassKg && now.InertiaKgM2 == old.InertiaKgM2 &&
      now.PositionM == old.PositionM && now.VelocityMs == old.VelocityMs &&
      now.SpinBodyRadS == old.SpinBodyRadS && now.OrientationQ.X == old.OrientationQ.X &&
      now.OrientationQ.Y == old.OrientationQ.Y && now.OrientationQ.Z == old.OrientationQ.Z &&
      now.OrientationQ.W == old.OrientationQ.W;
  const uint64_t bit = uint64_t{1} << (row % kBodiesPerSnapshotPage);
  auto &page = DirtyPages_[row / kBodiesPerSnapshotPage];
  if (same) {
    page &= ~bit;
  } else {
    page |= bit;
  }
}

void EntityState::PruneDeadBodies() {
  const uint64_t revision = Registry_.LivenessRevision();
  if (revision == PublishedLiveness_) { return; }
  PublishedLiveness_ = revision;
  const size_t removed = std::erase_if(
      Bodies_, [this](const EntityBodyState &body) { return !Registry_.alive(body.Owner); });
  if (removed == 0) { return; }
  std::vector<uint32_t> index;
  if (!Bodies_.empty()) { index.assign(Registry_.capacity(), kNoBody); }
  for (size_t row = 0; row < Bodies_.size(); ++row) {
    index[Bodies_[row].Owner.Index] = static_cast<uint32_t>(row);
  }
  BodyOfEntity_ =
      index.empty() ? nullptr : std::make_shared<const std::vector<uint32_t>>(std::move(index));
  MarkAllBodiesDirty();
}

BodySnapshot EntityState::PublishBodies() {
  PublicationCost_ = {};
  PruneDeadBodies();
  if (Bodies_.empty()) {
    if (PublishedBodies_.Count() != 0) {
      PublishedBodies_.Data_.reset();
      ++PublishedBodies_.Revision_;
    }
    return PublishedBodies_;
  }
  if (std::ranges::none_of(DirtyPages_, [](uint64_t dirty) { return dirty != 0; })) {
    return PublishedBodies_;
  }
  auto next = std::make_shared<BodySnapshotData>();
  next->RowOfEntity = BodyOfEntity_;
  next->Count = Bodies_.size();
  if (PublishedBodies_.Data_ && PublishedBodies_.Data_->RowOfEntity == BodyOfEntity_) {
    next->Pages = PublishedBodies_.Data_->Pages;
  } else {
    next->Pages.resize(DirtyPages_.size());
  }
  for (size_t page = 0; page < DirtyPages_.size(); ++page) {
    if (DirtyPages_[page] == 0) { continue; }
    const size_t first = page * kBodiesPerSnapshotPage;
    const size_t count = std::min(kBodiesPerSnapshotPage, Bodies_.size() - first);
    auto rows = std::make_shared<const std::vector<EntityBodyState>>(
        Bodies_.data() + first, Bodies_.data() + first + count);
    next->Pages[page] = std::move(rows);
    PublicationCost_.CopiedBytes += count * sizeof(EntityBodyState);
    ++PublicationCost_.NewPages;
  }
  PublicationCost_.DirectoryEntries = next->Pages.size();
  PublishedBodies_.Data_ = std::move(next);
  ++PublishedBodies_.Revision_;
  std::ranges::fill(DirtyPages_, uint64_t{0});
  return PublishedBodies_;
}

}
