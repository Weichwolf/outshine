#ifndef OUTSHINE_SCENE_H
#define OUTSHINE_SCENE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace outshine {

enum class Role : uint8_t { Body, Mind, Tool, Assignment };
inline constexpr size_t kRoles = 4;

[[nodiscard]] constexpr uint8_t RoleBit(Role kind) {
  return static_cast<uint8_t>(1u << static_cast<uint8_t>(kind));
}

class Tag {
public:
  constexpr Tag() = default;

  [[nodiscard]] constexpr uint32_t value() const { return Value_; }

  [[nodiscard]] constexpr bool within(Tag parent) const {
    uint32_t mask = 0xFFFFFFFFu;
    for (uint32_t held = parent.Value_; held != 0 && (held & 0xFFu) == 0; held >>= 8u) {
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
    return Tag(family.Value_ | ((ordinal & 0xFFu) << 16u));
  }
};

namespace tags {
inline constexpr Tag Does = TagCatalogue::Does;
inline constexpr Tag Offers = TagCatalogue::Offers;
} // namespace tags

static_assert(TagCatalogue::under(tags::Does, 1).within(tags::Does) &&
                  !tags::Does.within(TagCatalogue::under(tags::Does, 1)),
              "a tag is within its family and never the other way round");
static_assert(!TagCatalogue::under(tags::Does, 1).within(tags::Offers),
              "and a family holds only its own");

enum class Relation : uint8_t { IsA, ChildOf, DrivenBy, Uses, Assigned, HeldBy };
inline constexpr size_t kRelations = 6;

inline constexpr Relation kNoRelation = static_cast<Relation>(0xFF);

struct RelationRule {
  Relation Named = kNoRelation;
  bool Exclusive = false;
  bool Acyclic = false;
  bool OwnedByTarget = false;
  bool SameRole = false;
  uint8_t TargetRoles = 0;
  Tag SourceDoes{};
  Relation Requires = kNoRelation;
};

inline constexpr uint8_t kEveryRole =
    static_cast<uint8_t>(unsigned{RoleBit(Role::Body)} | unsigned{RoleBit(Role::Mind)} |
                         unsigned{RoleBit(Role::Tool)} | unsigned{RoleBit(Role::Assignment)});

inline constexpr RelationRule kRules[kRelations] = {
    {.Named = Relation::IsA,
     .Exclusive = true,
     .Acyclic = true,
     .OwnedByTarget = false,
     .SameRole = true,
     .TargetRoles = kEveryRole,
     .SourceDoes = {},
     .Requires = kNoRelation},
    {.Named = Relation::ChildOf,
     .Exclusive = true,
     .Acyclic = true,
     .OwnedByTarget = true,
     .SameRole = false,
     .TargetRoles = kEveryRole,
     .SourceDoes = {},
     .Requires = kNoRelation},
    {.Named = Relation::DrivenBy,
     .Exclusive = true,
     .Acyclic = false,
     .OwnedByTarget = false,
     .SameRole = false,
     .TargetRoles = RoleBit(Role::Mind),
     .SourceDoes = tags::Does,
     .Requires = kNoRelation},
    {.Named = Relation::Uses,
     .Exclusive = false,
     .Acyclic = false,
     .OwnedByTarget = false,
     .SameRole = false,
     .TargetRoles = RoleBit(Role::Tool),
     .SourceDoes = {},
     .Requires = kNoRelation},
    {.Named = Relation::Assigned,
     .Exclusive = true,
     .Acyclic = false,
     .OwnedByTarget = false,
     .SameRole = false,
     .TargetRoles = RoleBit(Role::Assignment),
     .SourceDoes = {},
     .Requires = Relation::Uses},
    {.Named = Relation::HeldBy,
     .Exclusive = true,
     .Acyclic = true,
     .OwnedByTarget = false,
     .SameRole = false,
     .TargetRoles = RoleBit(Role::Body),
     .SourceDoes = {},
     .Requires = kNoRelation},
};

[[nodiscard]] constexpr const RelationRule &RuleOf(Relation relation) {
  return kRules[static_cast<size_t>(relation)];
}

namespace scene_register_checked {
constexpr bool EachRuleStandsAtItsOwnRelation() {
  for (size_t at = 0; at < kRelations; ++at) {
    if (static_cast<size_t>(kRules[at].Named) != at) { return false; }
    if (kRules[at].TargetRoles == 0) { return false; }
  }
  return true;
}

static_assert(EachRuleStandsAtItsOwnRelation(),
              "every relation carries its rule, and no rule allows nothing");

constexpr bool EveryAcyclicRelationIsExclusive() {
  for (auto kRule : kRules) {
    if (kRule.Acyclic && !kRule.Exclusive) { return false; }
  }
  return true;
}

static_assert(EveryAcyclicRelationIsExclusive(),
              "the cycle walk follows one target per hop, so an acyclic relation must be "
              "exclusive -- widen the walk before you relax this");

constexpr size_t OwnedRelationCount() {
  size_t owned = 0;
  for (auto kRule : kRules) {
    if (kRule.OwnedByTarget) { ++owned; }
  }
  return owned;
}

constexpr bool EveryOwnedRelationIsExclusive() {
  for (auto kRule : kRules) {
    if (kRule.OwnedByTarget && !kRule.Exclusive) { return false; }
  }
  return true;
}

static_assert(EveryOwnedRelationIsExclusive(),
              "the felling stack pushes one entry per owned in-edge, and its reserve is "
              "capacity x owned-relations ONLY while each entity has at most one owner "
              "per owned relation -- widen the reserve before you relax this");
} // namespace scene_register_checked

inline constexpr size_t kOwnedRelations = scene_register_checked::OwnedRelationCount();
static_assert(kOwnedRelations >= 1, "removal owns at least the ChildOf chain");

struct Entity {
  uint32_t Index = 0;
  uint32_t Generation = 0;

  [[nodiscard]] constexpr bool operator==(Entity other) const {
    return Index == other.Index && Generation == other.Generation;
  }
};

inline constexpr Entity kNoEntity{.Index = 0xFFFFFFFFu, .Generation = 0};
inline constexpr size_t kPairsPerEntity = 8;
inline constexpr size_t kTagsPerEntity = 8;
inline constexpr size_t kSeatsPerOffer = 4;

enum class Seat : uint8_t { Free, Claimed, Occupied };

class Scene {
public:
  Scene();
  ~Scene();
  Scene(Scene &&) noexcept;
  Scene &operator=(Scene &&) noexcept;
  Scene(const Scene &) = delete;
  Scene &operator=(const Scene &) = delete;

  [[nodiscard]] bool open(size_t capacity);

  [[nodiscard]] Entity addEntity(Role role);
  void remove(Entity of);
  [[nodiscard]] bool alive(Entity of) const;
  [[nodiscard]] Role roleOf(Entity of) const;

  [[nodiscard]] bool giveTag(Entity to, Tag tag);
  [[nodiscard]] bool hasTag(Entity of, Tag tag) const;

  [[nodiscard]] bool link(Entity from, Relation how, Entity to);
  [[nodiscard]] bool relink(Entity from, Relation how, Entity to);
  [[nodiscard]] Entity targetOf(Entity of, Relation how) const;
  [[nodiscard]] size_t targets(Entity of, Relation how, Entity into[], size_t room) const;

  [[nodiscard]] size_t sources(Entity to, Relation how, Entity into[], size_t room) const;
  [[nodiscard]] size_t entitiesWithRole(Role role, Entity into[], size_t room) const;
  [[nodiscard]] size_t linkedPairs(Relation how, Entity from[], Entity to[], size_t room) const;
  [[nodiscard]] size_t entitiesWithTagAndRole(Tag tag, Role role, Entity into[], size_t room) const;

  [[nodiscard]] Entity instantiate(Entity prefab);
  [[nodiscard]] Entity copyOf(Entity instance, Entity prefabChild) const;

  [[nodiscard]] bool offerSeats(Entity at, Tag activity, size_t seats);
  [[nodiscard]] size_t entitiesOffering(Tag activity, Entity into[], size_t room) const;
  [[nodiscard]] bool claimSeat(Entity by, Entity at);
  [[nodiscard]] bool takeSeat(Entity by, Entity at);
  [[nodiscard]] bool releaseSeat(Entity by, Entity at);
  [[nodiscard]] Seat seatOf(Entity by, Entity at) const;

  [[nodiscard]] size_t capacity() const;
  [[nodiscard]] std::string_view error() const;

  [[nodiscard]] size_t touched() const;
  void resetTouched();

private:
  struct Kept;
  std::unique_ptr<Kept> Kept_;
};

} // namespace outshine
#endif
