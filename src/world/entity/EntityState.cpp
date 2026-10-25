#include "EntityState.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <utility>
#include <vector>

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
  BodyOfEntity_ = std::move(index);
  return {};
}

const RigidBodyState *EntityState::Body(Entity entity) const noexcept {
  if (!Registry_.alive(entity) || entity.Index >= BodyOfEntity_.size()) { return nullptr; }
  const uint32_t row = BodyOfEntity_[entity.Index];
  if (row >= Bodies_.size() || Bodies_[row].Owner != entity) { return nullptr; }
  return &Bodies_[row].Motion;
}

}
