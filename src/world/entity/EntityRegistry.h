#ifndef OUTSHINE_WORLD_ENTITY_ENTITYREGISTRY_H
#define OUTSHINE_WORLD_ENTITY_ENTITYREGISTRY_H

#include "Entity.h"
#include <span>
#include <expected>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace outshine {

constexpr uint32_t kEveryTagBit = 0xFFFFFFFFu;

constexpr uint32_t kOrdinalMask = 0x00FFFFFFu;

enum class Role : uint8_t { Body, Mind, Tool, Assignment };

class Tag {
public:
  constexpr Tag() noexcept = default;

  [[nodiscard]] constexpr uint32_t value() const noexcept { return Value_; }

  [[nodiscard]] constexpr bool within(Tag parent) const noexcept {
    if (parent.Value_ == 0) { return false; }
    if ((parent.Value_ & kOrdinalMask) != 0) { return Value_ == parent.Value_; }
    return (Value_ & ~kOrdinalMask) == parent.Value_;
  }

  [[nodiscard]] constexpr bool operator==(Tag other) const noexcept {
    return Value_ == other.Value_;
  }

private:
  constexpr explicit Tag(uint32_t value) : Value_(value) {}

  uint32_t Value_ = 0;
  friend struct TagCatalogue;
};

enum class TagError { InvalidFamily, InvalidOrdinal };

struct RegistryError {

  std::string Message;
};

struct TagCatalogue {
  static constexpr Tag Does{0x01000000};
  static constexpr Tag Offers{0x02000000};

  [[nodiscard]] static constexpr std::expected<Tag, TagError> under(Tag family,
                                                                    uint32_t ordinal) noexcept {
    if (family.Value_ == 0 || (family.Value_ & kOrdinalMask) != 0) {
      return std::unexpected(TagError::InvalidFamily);
    }
    if (ordinal == 0 || ordinal > kOrdinalMask) {
      return std::unexpected(TagError::InvalidOrdinal);
    }
    return Tag(family.Value_ | ordinal);
  }
};

namespace tags {
inline constexpr Tag Does = TagCatalogue::Does;
inline constexpr Tag Offers = TagCatalogue::Offers;
}

static_assert(TagCatalogue::under(tags::Does, 1)->within(tags::Does) &&
                  !tags::Does.within(*TagCatalogue::under(tags::Does, 1)),
              "a tag is within its family and never the other way round");
static_assert(!TagCatalogue::under(tags::Does, 1)->within(tags::Offers),
              "and a family holds only its own");

enum class Relation : uint8_t { IsA, ChildOf, DrivenBy, Uses, Assigned, HeldBy };

enum class Seat : uint8_t { Free, Claimed, Occupied };

struct Seating {

  Entity By = kNoEntity;

  Entity At = kNoEntity;
};

struct Instanced {

  Entity Instance = kNoEntity;

  Entity PrefabChild = kNoEntity;
};

class EntityRegistry {
public:
  EntityRegistry();

  ~EntityRegistry();

  EntityRegistry(EntityRegistry &&) = delete;

  EntityRegistry &operator=(EntityRegistry &&) = delete;
  EntityRegistry(const EntityRegistry &) = delete;
  EntityRegistry &operator=(const EntityRegistry &) = delete;

  [[nodiscard]] std::expected<void, RegistryError> open(size_t capacity);

  [[nodiscard]] std::expected<Entity, RegistryError> addEntity(Role role);

  void remove(Entity of);

  [[nodiscard]] bool alive(Entity of) const;

  [[nodiscard]] std::optional<Role> roleOf(Entity of) const;

  [[nodiscard]] std::expected<void, RegistryError> giveTag(Entity to, Tag tag);

  [[nodiscard]] bool hasTag(Entity of, Tag tag) const;

  [[nodiscard]] std::expected<void, RegistryError> link(Entity from, Relation how, Entity to);

  [[nodiscard]] std::expected<void, RegistryError> relink(Entity from, Relation how, Entity to);

  [[nodiscard]] Entity targetOf(Entity of, Relation how) const;

  [[nodiscard]] size_t targets(Entity of, Relation how, std::span<Entity> into) const;

  [[nodiscard]] size_t sources(Entity to, Relation how, std::span<Entity> into) const;

  [[nodiscard]] size_t entitiesWithRole(Role role, std::span<Entity> into) const;

  [[nodiscard]] size_t
  linkedPairs(Relation how, std::span<Entity> from, std::span<Entity> to) const;

  [[nodiscard]] size_t entitiesWithTagAndRole(Tag tag, Role role, std::span<Entity> into) const;

  [[nodiscard]] std::expected<Entity, RegistryError> instantiate(Entity prefab);

  [[nodiscard]] Entity copyOf(Instanced which) const;

  [[nodiscard]] std::expected<void, RegistryError>
  offerSeats(Entity at, Tag activity, size_t seats);

  [[nodiscard]] size_t entitiesOffering(Tag activity, std::span<Entity> into) const;

  [[nodiscard]] std::expected<void, RegistryError> claimSeat(Seating who);

  [[nodiscard]] std::expected<void, RegistryError> takeSeat(Seating who);

  [[nodiscard]] std::expected<void, RegistryError> releaseSeat(Seating who);

  [[nodiscard]] Seat seatOf(Seating who) const;

  [[nodiscard]] size_t capacity() const;

  [[nodiscard]] size_t touched() const;

  void resetTouched();

private:
  struct Kept;
  std::unique_ptr<Kept> Kept_;
};

}
#endif
