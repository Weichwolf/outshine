#ifndef OUTSHINE_WORLD_ENTITY_ENTITYREGISTRY_H
#define OUTSHINE_WORLD_ENTITY_ENTITYREGISTRY_H

#include "Entity.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace outshine::World {
struct RegistryError {
  std::string Message;
};

class EntityRegistry {
public:
  EntityRegistry() = default;
  EntityRegistry(const EntityRegistry &) = delete;
  EntityRegistry &operator=(const EntityRegistry &) = delete;
  EntityRegistry(EntityRegistry &&) = delete;
  EntityRegistry &operator=(EntityRegistry &&) = delete;
  ~EntityRegistry() = default;

  [[nodiscard]] std::expected<void, RegistryError> open(size_t capacity);
  [[nodiscard]] std::expected<Entity, RegistryError> addEntity();
  void remove(Entity entity) noexcept;
  [[nodiscard]] bool alive(Entity entity) const noexcept;

  [[nodiscard]] uint64_t LivenessRevision() const noexcept { return Revision_; }

  [[nodiscard]] size_t capacity() const noexcept { return Slots_.size(); }

private:
  struct Slot {
    uint32_t Generation = 0;
    bool Held = false;
  };

  std::vector<Slot> Slots_;
  std::vector<uint32_t> Free_;
  uint64_t Owner_ = 0;
  uint64_t Revision_ = 0;
};
}
#endif
