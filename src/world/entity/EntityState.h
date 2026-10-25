#ifndef OUTSHINE_WORLD_ENTITY_ENTITYSTATE_H
#define OUTSHINE_WORLD_ENTITY_ENTITYSTATE_H

#include "Column.h"
#include "BodySnapshot.h"
#include <memory>
#include <utility>
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

  template <typename Edit> [[nodiscard]] bool EditBody(Entity entity, Edit &&edit) {
    const auto *body = Body(entity);
    if (body == nullptr) { return false; }
    const size_t row = (*BodyOfEntity_)[entity.Index];
    std::forward<Edit>(edit)(Bodies_[row].Motion);
    MarkBodyEdited(row);
    return true;
  }

  [[nodiscard]] BodySnapshot PublishBodies();

  [[nodiscard]] const BodyPublicationCost &PublicationCost() const noexcept {
    return PublicationCost_;
  }

  [[nodiscard]] EntityRegistry &Registry() noexcept { return Registry_; }

  [[nodiscard]] const EntityRegistry &Registry() const noexcept { return Registry_; }

  [[nodiscard]] Column<::outshine::Traits> &Traits() noexcept { return Traits_; }

  [[nodiscard]] const Column<::outshine::Traits> &Traits() const noexcept { return Traits_; }

private:
  EntityRegistry Registry_;
  Column<::outshine::Traits> Traits_;
  std::vector<EntityBodyState> Bodies_;
  void PruneDeadBodies();
  std::shared_ptr<const std::vector<uint32_t>> BodyOfEntity_;
  void MarkBodyEdited(size_t row);
  void MarkAllBodiesDirty();
  std::vector<uint64_t> DirtyPages_;
  BodySnapshot PublishedBodies_;
  BodyPublicationCost PublicationCost_;
  uint64_t PublishedLiveness_ = 0;
};

}

#endif
