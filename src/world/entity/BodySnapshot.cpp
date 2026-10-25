#include "BodySnapshot.h"
#include "BodySnapshotData.h"
#include <cassert>
#include <cstddef>

namespace outshine::World {
size_t BodySnapshot::Count() const noexcept {
  return Data_ ? Data_->Count : 0;
}

const EntityBodyState &BodySnapshot::At(size_t row) const noexcept {
  assert(row < Count());
  return (*Data_->Pages[row / kBodiesPerSnapshotPage])[row % kBodiesPerSnapshotPage];
}

const RigidBodyState *BodySnapshot::Body(Entity entity) const noexcept {
  if (!Data_ || entity.Index >= Data_->RowOfEntity->size()) { return nullptr; }
  const size_t row = (*Data_->RowOfEntity)[entity.Index];
  if (row >= Count() || At(row).Owner != entity) { return nullptr; }
  return &At(row).Motion;
}
}
