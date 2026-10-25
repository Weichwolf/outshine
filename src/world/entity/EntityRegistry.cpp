#include "EntityRegistry.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <utility>
#include <vector>

namespace outshine::World {
namespace {
uint64_t NextRegistryOwner() noexcept {
  static std::atomic<uint64_t> next{1};
  uint64_t candidate = next.load(std::memory_order_relaxed);
  while (candidate != std::numeric_limits<uint64_t>::max()) {
    if (next.compare_exchange_weak(candidate, candidate + 1, std::memory_order_relaxed)) {
      return candidate;
    }
  }
  return 0;
}

std::unexpected<RegistryError> Error(const char *message) {
  return std::unexpected(RegistryError{.Message = message});
}
}

std::expected<void, RegistryError> EntityRegistry::open(size_t capacity) {
  if (capacity == 0 || std::cmp_greater(capacity, std::numeric_limits<uint32_t>::max()) ||
      capacity > Slots_.max_size() || capacity > Free_.max_size()) {
    return Error("invalid entity capacity");
  }
  const uint64_t owner = NextRegistryOwner();
  if (owner == 0) { return Error("entity identity space exhausted"); }
  std::vector<Slot> slots(capacity);
  std::vector<uint32_t> free;
  free.reserve(capacity);
  for (size_t at = capacity; at > 0; --at) { free.push_back(static_cast<uint32_t>(at - 1)); }
  Slots_ = std::move(slots);
  Free_ = std::move(free);
  Owner_ = owner;
  ++Revision_;
  return {};
}

std::expected<Entity, RegistryError> EntityRegistry::addEntity() {
  if (Free_.empty()) { return Error("entity capacity full"); }
  const uint32_t index = Free_.back();
  Free_.pop_back();
  Slot &slot = Slots_[index];
  slot.Held = true;
  ++Revision_;
  return Entity{.Index = index, .Generation = slot.Generation, .Owner = Owner_};
}

bool EntityRegistry::alive(Entity entity) const noexcept {
  return entity.Owner == Owner_ && entity.Index < Slots_.size() && Slots_[entity.Index].Held &&
         entity.Generation == Slots_[entity.Index].Generation;
}

void EntityRegistry::remove(Entity entity) noexcept {
  if (!alive(entity)) { return; }
  Slot &slot = Slots_[entity.Index];
  slot.Held = false;
  ++Revision_;
  if (slot.Generation != std::numeric_limits<uint32_t>::max()) {
    ++slot.Generation;
    Free_.push_back(entity.Index);
  }
}
}
