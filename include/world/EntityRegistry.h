#ifndef OUTSHINE_WORLD_ENTITYREGISTRY_H
#define OUTSHINE_WORLD_ENTITYREGISTRY_H

#include "world/Entity.h"
#include <span>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace outshine {

/// Every bit of a tag word set, which is what a tag with no parent matches against.
constexpr uint32_t kEveryTagBit = 0xFFFFFFFFu;

/// How many ordinals a tag family holds before it needs a wider word.
constexpr uint32_t kOrdinalMask = 0xFFu;

enum class Role : uint8_t { Body, Mind, Tool, Assignment };

class Tag {
public:
  constexpr Tag() = default;

  [[nodiscard]] constexpr uint32_t value() const { return Value_; }

  [[nodiscard]] constexpr bool within(Tag parent) const {
    uint32_t mask = kEveryTagBit;
    for (uint32_t held = parent.Value_; held != 0 && (held & kOrdinalMask) == 0; held >>= 8u) {
      mask <<= 8u;
    }
    return parent.Value_ != 0 && (Value_ & mask) == parent.Value_;
  }

  [[nodiscard]] constexpr bool operator==(Tag other) const { return Value_ == other.Value_; }

private:
  constexpr explicit Tag(uint32_t value) : Value_(value) {}

  uint32_t Value_ = 0;
  friend struct TagCatalogue;
};

struct TagCatalogue {
  static constexpr Tag Does{0x01000000};
  static constexpr Tag Offers{0x02000000};

  [[nodiscard]] static constexpr Tag under(Tag family, uint32_t ordinal) {
    return Tag(family.Value_ | ((ordinal & kOrdinalMask) << 16u));
  }
};

namespace tags {
inline constexpr Tag Does = TagCatalogue::Does;
inline constexpr Tag Offers = TagCatalogue::Offers;
}

static_assert(TagCatalogue::under(tags::Does, 1).within(tags::Does) &&
                  !tags::Does.within(TagCatalogue::under(tags::Does, 1)),
              "a tag is within its family and never the other way round");
static_assert(!TagCatalogue::under(tags::Does, 1).within(tags::Offers),
              "and a family holds only its own");

enum class Relation : uint8_t { IsA, ChildOf, DrivenBy, Uses, Assigned, HeldBy };

enum class Seat : uint8_t { Free, Claimed, Occupied };

/// WHO sits WHERE -- the claimant and the entity whose seats were offered.
///
/// The two are one argument rather than two because both are an @ref Entity and nothing in the
/// type system would catch them the wrong way round. Written with designated initialisers the
/// order stops mattering: `takeSeat({.By = walker, .At = bench})`.
struct Seating {
  /// The entity that claims, takes or releases the seat.
  Entity By = kNoEntity;
  /// The entity that offered the seats, and where the seat stands.
  Entity At = kNoEntity;
};

/// A child of a PREFAB and the INSTANCE to find that child's copy in.
///
/// Both ends are an @ref Entity, so they are one argument for the same reason @ref Seating is:
/// `copyOf({.Instance = house, .PrefabChild = door})` cannot be written backwards by accident.
struct Instanced {
  /// The entity @ref EntityRegistry::instantiate returned.
  Entity Instance = kNoEntity;
  /// The entity inside the prefab whose copy is wanted.
  Entity PrefabChild = kNoEntity;
};

/// Stable owner of entity slots and relations; neither copyable nor movable.
/// Serialize access. Entity handles do not retain this owner or its storage.
class EntityRegistry {
public:
  EntityRegistry();
  ~EntityRegistry();
  /// Moving would invalidate borrowed registry addresses and is forbidden.
  EntityRegistry(EntityRegistry &&) = delete;
  /// Replacing ownership through move assignment is forbidden.
  EntityRegistry &operator=(EntityRegistry &&) = delete;
  EntityRegistry(const EntityRegistry &) = delete;
  EntityRegistry &operator=(const EntityRegistry &) = delete;

  /// Reinitialize storage with a fresh identity epoch; successful reopening invalidates all
  /// handles. Existing component columns must be repopulated; previous values never bind to new
  /// entities. Allocates on the calling thread; serialize with all access to this registry.
  /// @param capacity Positive number of entity slots to allocate.
  /// @return False for zero/unaddressable capacity or exhausted identity space, preserving the
  /// previous epoch. Allocation failure remains fatal/separate from these validation errors.
  [[nodiscard]] bool open(size_t capacity);

  /// Allocate a slot from the prepared pool; no growth or allocation on success.
  /// @param role Supported entity role.
  /// @return Registry-owned handle, or kNoEntity for invalid role/full pool; rejection
  /// preserves existing entities and available slots and records error().
  [[nodiscard]] Entity addEntity(Role role);
  void remove(Entity of);
  [[nodiscard]] bool alive(Entity of) const;
  [[nodiscard]] Role roleOf(Entity of) const;

  [[nodiscard]] bool giveTag(Entity to, Tag tag);
  [[nodiscard]] bool hasTag(Entity of, Tag tag) const;

  /// Add a relation under the registry's role, exclusivity and acyclicity rules.
  /// @param from Live source belonging to this registry epoch.
  /// @param how Supported relation; invalid enum values are rejected.
  /// @param to Live target belonging to this registry epoch.
  /// @return Success; false preserves relations and records error().
  [[nodiscard]] bool link(Entity from, Relation how, Entity to);
  /// Replace the target of an existing exclusive relation after validating the replacement.
  /// @param from Live source with an existing relation of the requested kind.
  /// @param how Supported exclusive relation; invalid enum values are rejected.
  /// @param to Live replacement target satisfying the relation rules.
  /// @return Success; false preserves the original target and records error().
  [[nodiscard]] bool relink(Entity from, Relation how, Entity to);
  [[nodiscard]] Entity targetOf(Entity of, Relation how) const;
  [[nodiscard]] size_t targets(Entity of, Relation how, std::span<Entity> into) const;

  /// Enumerate incoming sources without allocating; ordering is unspecified.
  /// @param to Live target; stale/foreign handles produce no results.
  /// @param how Supported relation; invalid enum values produce no results.
  /// @param into Borrowed output span receiving a prefix; untouched when there are no results.
  /// @return Total matching count, possibly greater than span capacity. No error-state change.
  [[nodiscard]] size_t sources(Entity to, Relation how, std::span<Entity> into) const;
  /// Enumerate live entities of one role without allocating; ordering is unspecified.
  /// @param role Supported role; invalid enum values produce no results.
  /// @param into Borrowed output receiving a prefix; remaining elements stay unchanged.
  /// @return Total matches, possibly greater than span capacity; no error-state change.
  [[nodiscard]] size_t entitiesWithRole(Role role, std::span<Entity> into) const;
  /// Enumerate relation edges without allocating; ordering is unspecified.
  /// @param how Supported relation; invalid enum values produce no results.
  /// @param from Borrowed source output; receives the prefix fitting its own capacity.
  /// @param to Borrowed target output; receives the corresponding prefix fitting its capacity.
  /// @return Total edges; either span may be smaller. No results leave both spans unchanged.
  [[nodiscard]] size_t
  linkedPairs(Relation how, std::span<Entity> from, std::span<Entity> to) const;
  /// Enumerate live entities matching both role and tag family without allocating.
  /// @param tag Required tag family.
  /// @param role Supported role; invalid enum values produce no results.
  /// @param into Borrowed output receiving a prefix; unused elements remain unchanged.
  /// @return Total matches, possibly greater than span capacity; no error-state change.
  [[nodiscard]] size_t entitiesWithTagAndRole(Tag tag, Role role, std::span<Entity> into) const;

  [[nodiscard]] Entity instantiate(Entity prefab);
  [[nodiscard]] Entity copyOf(Instanced which) const;

  [[nodiscard]] bool offerSeats(Entity at, Tag activity, size_t seats);
  [[nodiscard]] size_t entitiesOffering(Tag activity, std::span<Entity> into) const;
  [[nodiscard]] bool claimSeat(Seating who);
  [[nodiscard]] bool takeSeat(Seating who);
  [[nodiscard]] bool releaseSeat(Seating who);
  [[nodiscard]] Seat seatOf(Seating who) const;

  [[nodiscard]] size_t capacity() const;
  [[nodiscard]] std::string_view error() const;

  [[nodiscard]] size_t touched() const;
  void resetTouched();

private:
  struct Kept;
  std::unique_ptr<Kept> Kept_;
};

}
#endif
