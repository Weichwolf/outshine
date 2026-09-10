#ifndef OUTSHINE_WORLD_ENTITYREGISTRY_H
#define OUTSHINE_WORLD_ENTITYREGISTRY_H

#include "world/Entity.h"
#include <span>
#include <expected>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

namespace outshine {

/// All bits of a 32-bit tag word.
constexpr uint32_t kEveryTagBit = 0xFFFFFFFFu;

/// Mask and maximum nonzero child ordinal: 24 bits within one family.
constexpr uint32_t kOrdinalMask = 0x00FFFFFFu;

/// Entity category used to validate relations; does not attach a component automatically.
enum class Role : uint8_t {
  Body,      ///< Physical or spatial participant.
  Mind,      ///< Controller eligible as a DrivenBy target.
  Tool,      ///< Resource eligible as a Uses target.
  Assignment ///< Task eligible as an Assigned target.
};

/// Two-level numeric tag: an 8-bit family and a 24-bit child ordinal.
/// Contains no owner or name mapping. Producers and queries must use the same catalogue;
/// numeric encodings are runtime identities, not persistent save identifiers.
class Tag {
public:
  /// Construct an invalid tag, which matches no parent.
  constexpr Tag() noexcept = default;

  /// @return Encoded numeric identity, or zero for the invalid tag; no allocation.
  [[nodiscard]] constexpr uint32_t value() const noexcept { return Value_; }

  /// Query family membership or exact child identity, including self-membership.
  /// @param parent Family or child from the same catalogue; invalid matches nothing.
  /// @return True for a matching family, or an identical child. Constant work, no allocation.
  [[nodiscard]] constexpr bool within(Tag parent) const noexcept {
    if (parent.Value_ == 0) { return false; }
    if ((parent.Value_ & kOrdinalMask) != 0) { return Value_ == parent.Value_; }
    return (Value_ & ~kOrdinalMask) == parent.Value_;
  }

  /// @param other Numeric tag to compare; no catalogue or owner lookup.
  /// @return Exact encoding equality; two invalid tags compare equal.
  [[nodiscard]] constexpr bool operator==(Tag other) const noexcept {
    return Value_ == other.Value_;
  }

private:
  constexpr explicit Tag(uint32_t value) : Value_(value) {}

  uint32_t Value_ = 0;
  friend struct TagCatalogue;
};

/// Reasons why a child tag cannot be constructed.
enum class TagError {
  InvalidFamily, ///< Invalid tag or an existing child used as a family.
  InvalidOrdinal ///< Zero or a value exceeding the 24-bit child field.
};

/// Fixed activity families and checked construction; does not intern names or own storage.
struct TagCatalogue {
  static constexpr Tag Does{0x01000000};   ///< Activities an entity can perform.
  static constexpr Tag Offers{0x02000000}; ///< Activities an entity offers to others.

  /// Construct a distinct child without truncating or wrapping its ordinal.
  /// @param family Nonzero family tag, not a child; checked before the ordinal.
  /// @param ordinal Child identifier in [1, kOrdinalMask], assigned by the caller's catalogue.
  /// @return Child or a typed validation error. Constant work, no allocation or mutation.
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

/// Directed relation from source to target. Exclusive relations allow one target per source.
enum class Relation : uint8_t {
  IsA,      ///< Exclusive, acyclic inheritance between entities of the same role.
  ChildOf,  ///< Exclusive, acyclic ownership; removing the target removes descendants.
  DrivenBy, ///< Exclusive link to a Mind; source must carry a Does-family tag.
  Uses,     ///< Nonexclusive link to a Tool.
  Assigned, ///< Exclusive link to an Assignment; requires a Uses relation on the source.
  HeldBy    ///< Exclusive, acyclic link to a Body; does not imply ChildOf ownership.
};

/// Reservation state for one claimant at one offering entity; no automatic timeout.
enum class Seat : uint8_t {
  Free,    ///< No claim or occupancy by the queried claimant.
  Claimed, ///< Reserved, awaiting takeSeat().
  Occupied ///< Claimed seat transitioned into use.
};

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
  /// The entity @ref outshine::EntityRegistry::instantiate returned.
  Entity Instance = kNoEntity;
  /// The entity inside the prefab whose copy is wanted.
  Entity PrefabChild = kNoEntity;
};

/// Stable owner of entity slots and relations; neither copyable nor movable.
/// Serialize access. Entity handles do not retain this owner or its storage.
class EntityRegistry {
public:
  /// Allocate the registry implementation; call open() to prepare entity storage.
  EntityRegistry();
  /// Release owned storage; all handles and borrowed diagnostic text become invalid.
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
  /// Remove an entity and its ChildOf descendants, invalidating their handles and relations.
  /// Foreign/stale handles are ignored. Work follows affected entities and incident edges;
  /// uses prepared traversal storage. Does not remove separately owned component storage.
  /// @param of Root to remove from this registry epoch.
  void remove(Entity of);
  /// Test ownership, slot occupancy and generation in constant time without allocation.
  /// @param of Handle to validate; kNoEntity, stale and foreign handles return false.
  /// @return Whether the handle currently belongs to a live entity in this registry.
  [[nodiscard]] bool alive(Entity of) const;
  /// Query a live entity's role in constant time without allocation or error-state change.
  /// @param of Handle to validate against this registry epoch.
  /// @return Role, or nullopt for kNoEntity, stale, removed or foreign handles.
  [[nodiscard]] std::optional<Role> roleOf(Entity of) const;

  /// Attach an exact nonempty tag using the entity's prepared, bounded tag storage.
  /// No allocation; serialize with registry access. A tag does not carry catalogue ownership.
  /// @param to Live target in this registry.
  /// @param tag Tag from the catalogue shared by this registry's producers and queries.
  /// @return False for invalid target/tag, exact duplicate or full storage, preserving tags
  ///         and recording error(). Family/child overlap is not an exact duplicate.
  [[nodiscard]] bool giveTag(Entity to, Tag tag);
  /// Test direct tags and IsA ancestors using Tag::within(); no allocation.
  /// Work follows the bounded inheritance chain and updates traversal diagnostics. Serialize
  /// with registry access; the error string is unchanged.
  /// @param of Live entity in this registry; stale/foreign handles match nothing.
  /// @param tag Family or exact child from the same catalogue; invalid matches nothing.
  /// @return Whether a matching tag exists on the entity or an ancestor.
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
  /// Query the first matching outgoing edge; no inherited lookup or allocation.
  /// @param of Source handle; stale/foreign handles yield no target.
  /// @param how Relation to query; invalid values yield no target.
  /// @return Target or kNoEntity. For nonexclusive relations, selection order is unspecified.
  /// Updates traversal diagnostics but leaves error() unchanged; serialize registry access.
  [[nodiscard]] Entity targetOf(Entity of, Relation how) const;
  /// Enumerate matching outgoing edges without allocation or inherited lookup.
  /// @param of Source handle; stale/foreign handles yield no targets.
  /// @param how Relation to query; invalid values yield no targets.
  /// @param into Borrowed output receiving a prefix; unused elements remain unchanged.
  /// @return Total matching count, possibly exceeding span capacity; order is unspecified.
  /// Work scans the source's bounded relation storage and updates traversal diagnostics.
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

  /// Create same-role copies of a live root and its ChildOf descendants using prepared storage.
  /// Each copy inherits its source through IsA; copied children link to their copied parent.
  /// Does not copy separately stored components, offers, seat state or arbitrary relations.
  /// Work follows the source subtree and relation validation; serialize registry access.
  /// Failure removes partially created copies and preserves existing entities, but consumes
  /// their temporary slot generations and may change allocation order and diagnostics.
  /// @param prefab Live source root in this registry.
  /// @return New root or kNoEntity with error() describing invalid input/capacity/relation failure.
  [[nodiscard]] Entity instantiate(Entity prefab);
  /// Find a direct ChildOf child whose IsA target is the specified prefab child.
  /// Does not search recursively; use the corresponding parent copy for a deeper child.
  /// @param which Borrowed handles in this registry; no ownership transfer.
  /// @return Matching child or kNoEntity; no allocation or error-state change.
  /// Work follows direct incoming ChildOf edges and updates traversal diagnostics.
  [[nodiscard]] Entity copyOf(Instanced which) const;

  /// Register one activity offer using prepared, bounded seat storage; no allocation.
  /// @param at Live offering entity; an existing offer cannot be replaced by this operation.
  /// @param activity Nonempty activity tag from the shared catalogue.
  /// @param seats Seat count in [1, 4], the current prepared per-offer capacity.
  /// @return Success or false with error(); validation failures preserve the previous offer.
  [[nodiscard]] bool offerSeats(Entity at, Tag activity, size_t seats);
  /// Enumerate registered offers matching Tag::within(), regardless of remaining free seats.
  /// @param activity Family or child to match; invalid tags match nothing.
  /// @param into Borrowed prefix output; unused elements remain unchanged.
  /// @return Total matching count, possibly exceeding capacity; order is unspecified.
  /// Scans offers without allocation; updates traversal diagnostics, not error().
  [[nodiscard]] size_t entitiesOffering(Tag activity, std::span<Entity> into) const;
  /// Reserve a free seat; the same claimant may hold at most one seat at this offer.
  /// Reclaims slots held by dead entities while searching. No role/capability or distance
  /// check is performed; higher-level scheduling must decide eligibility.
  /// @param who Live claimant and offering entity in this registry.
  /// @return Success or false with error() for invalid handles, missing offer, duplicate claim
  ///         or exhaustion. Uses bounded prepared storage without allocation.
  [[nodiscard]] bool claimSeat(Seating who);
  /// Change this claimant's existing reservation from Claimed to Occupied.
  /// @param who Live claimant and offering entity in this registry.
  /// @return Success or false with error(); invalid handles or a missing claim preserve seats.
  /// No allocation; calling again on an occupied seat is an error, not an idempotent success.
  [[nodiscard]] bool takeSeat(Seating who);
  /// Release this claimant's Claimed or Occupied seat.
  /// @param who Live claimant and offering entity in this registry.
  /// @return Success or false with error() for invalid handles or no held seat.
  /// No allocation. Dead claimants are reclaimed by later claimSeat() calls instead.
  [[nodiscard]] bool releaseSeat(Seating who);
  /// Query this claimant's reservation without allocation or error-state change.
  /// @param who Both handles must be live in this registry; neither is retained by this query.
  /// @return Claimed/Occupied for the reservation; Free when absent or either handle is invalid.
  /// Scans only the offering entity's fixed seat storage; serialize with registry mutations.
  [[nodiscard]] Seat seatOf(Seating who) const;

  /// @return Prepared slot capacity, including used and free slots; zero before open().
  /// Constant-time, nonallocating query; serialize with registry access.
  [[nodiscard]] size_t capacity() const;
  /// Borrow the most recently recorded diagnostic; successful operations need not clear it.
  /// @return Text valid until a later diagnostic change or registry destruction. Copy to retain.
  /// No allocation; serialize access, including use of the returned view, with mutations.
  [[nodiscard]] std::string_view error() const;

  /// @return Accumulated traversal-step counter, not elapsed time or total operation count.
  /// Const queries can increase it. Constant-time, nonallocating; serialize registry access.
  [[nodiscard]] size_t touched() const;
  /// Reset traversal diagnostics to zero without changing entities, relations or error().
  /// Constant work, no allocation; serialize registry access.
  void resetTouched();

private:
  struct Kept;
  std::unique_ptr<Kept> Kept_;
};

}
#endif
