#ifndef OUTSHINE_WORLD_ENTITY_ENTITYSTATE_H
#define OUTSHINE_WORLD_ENTITY_ENTITYSTATE_H

#include "Column.h"
#include "Entity.h"
#include "EntityBodyState.h"
#include "EntityRegistry.h"
#include "RigidBodyState.h"
#include "Traits.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace outshine::World {

enum class EntityStateError { CapacityExceeded, InvalidEntity, DuplicateBody };

class EntityState {
public:
  EntityState() = default;
  EntityState(const EntityState &) = delete;
  EntityState &operator=(const EntityState &) = delete;
  EntityState(EntityState &&) = delete;
  EntityState &operator=(EntityState &&) = delete;

  [[nodiscard]] bool Open(size_t capacity);
  [[nodiscard]] std::expected<void, EntityStateError>
  SetBodies(std::vector<EntityBodyState> bodies);
  [[nodiscard]] const RigidBodyState *Body(Entity entity) const noexcept;

  [[nodiscard]] std::span<const EntityBodyState> Bodies() const noexcept { return Bodies_; }

  [[nodiscard]] std::span<EntityBodyState> MutableBodies() noexcept { return Bodies_; }

  [[nodiscard]] EntityRegistry &Registry() noexcept { return Registry_; }

  [[nodiscard]] const EntityRegistry &Registry() const noexcept { return Registry_; }

  [[nodiscard]] Column<::outshine::Traits> &Traits() noexcept { return Traits_; }

  [[nodiscard]] const Column<::outshine::Traits> &Traits() const noexcept { return Traits_; }

private:
  EntityRegistry Registry_;
  Column<::outshine::Traits> Traits_;
  std::vector<EntityBodyState> Bodies_;
  std::vector<uint32_t> BodyOfEntity_;
};

}

#endif
