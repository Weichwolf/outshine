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

[[nodiscard]] constexpr uint8_t RoleBit(Role kind) { return (uint8_t)(1u << (uint8_t)kind); }

class Tag {
public:
  constexpr Tag() = default;

  [[nodiscard]] constexpr uint32_t Value() const { return Value_; }
  [[nodiscard]] constexpr bool Within(Tag parent) const {
    uint32_t mask = 0xFFFFFFFFu;
    for (uint32_t held = parent.Value_; held != 0 && (held & 0xFFu) == 0; held >>= 8) {
      mask <<= 8;
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

  [[nodiscard]] static constexpr Tag Under(Tag family, uint32_t ordinal) {
    return Tag(family.Value_ | ((ordinal & 0xFFu) << 16));
  }
};

namespace tags {
inline constexpr Tag Does = TagCatalogue::Does;
inline constexpr Tag Offers = TagCatalogue::Offers;
}

static_assert(TagCatalogue::Under(tags::Does, 1).Within(tags::Does) &&
                  !tags::Does.Within(TagCatalogue::Under(tags::Does, 1)),
              "a tag is within its family and never the other way round");
static_assert(!TagCatalogue::Under(tags::Does, 1).Within(tags::Offers),
              "and a family holds only its own");

enum class Relation : uint8_t { IsA, ChildOf, DrivenBy, Uses, Assigned, HeldBy };
inline constexpr size_t kRelations = 6;

inline constexpr Relation kNoRelation = (Relation)0xFF;

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
    (uint8_t)(RoleBit(Role::Body) | RoleBit(Role::Mind) | RoleBit(Role::Tool) |
              RoleBit(Role::Assignment));

inline constexpr RelationRule kRules[kRelations] = {
    {Relation::IsA, true, true, false, true, kEveryRole, {}, kNoRelation},
    {Relation::ChildOf, true, true, true, false, kEveryRole, {}, kNoRelation},
    {Relation::DrivenBy, true, false, false, false, RoleBit(Role::Mind), tags::Does, kNoRelation},
    {Relation::Uses, false, false, false, false, RoleBit(Role::Tool), {}, kNoRelation},
    {Relation::Assigned, true, false, false, false, RoleBit(Role::Assignment), {}, Relation::Uses},
    {Relation::HeldBy, true, true, false, false, RoleBit(Role::Body), {}, kNoRelation},
};

[[nodiscard]] constexpr const RelationRule &RuleOf(Relation relation) {
  return kRules[(size_t)relation];
}

namespace scene_register_checked {
constexpr bool EachRuleStandsAtItsOwnRelation() {
  for (size_t at = 0; at < kRelations; ++at) {
    if ((size_t)kRules[at].Named != at) { return false; }
    if (kRules[at].TargetRoles == 0) { return false; }
  }
  return true;
}
static_assert(EachRuleStandsAtItsOwnRelation(),
              "every relation carries its rule, and no rule allows nothing");
constexpr bool EveryAcyclicRelationIsExclusive() {
  for (size_t at = 0; at < kRelations; ++at) {
    if (kRules[at].Acyclic && !kRules[at].Exclusive) { return false; }
  }
  return true;
}
static_assert(EveryAcyclicRelationIsExclusive(),
              "the cycle walk follows one target per hop, so an acyclic relation must be "
              "exclusive -- widen the walk before you relax this");
constexpr size_t OwnedRelationCount() {
  size_t owned = 0;
  for (size_t at = 0; at < kRelations; ++at) {
    if (kRules[at].OwnedByTarget) { ++owned; }
  }
  return owned;
}
constexpr bool EveryOwnedRelationIsExclusive() {
  for (size_t at = 0; at < kRelations; ++at) {
    if (kRules[at].OwnedByTarget && !kRules[at].Exclusive) { return false; }
  }
  return true;
}
static_assert(EveryOwnedRelationIsExclusive(),
              "the felling stack pushes one entry per owned in-edge, and its reserve is "
              "capacity x owned-relations ONLY while each entity has at most one owner "
              "per owned relation -- widen the reserve before you relax this");
}

inline constexpr size_t kOwnedRelations = scene_register_checked::OwnedRelationCount();
static_assert(kOwnedRelations >= 1, "removal owns at least the ChildOf chain");


struct Entity {
  uint32_t Index = 0;
  uint32_t Generation = 0;

  [[nodiscard]] constexpr bool operator==(Entity other) const {
    return Index == other.Index && Generation == other.Generation;
  }
};

inline constexpr Entity kNoEntity{0xFFFFFFFFu, 0};
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

  [[nodiscard]] bool Open(size_t capacity);

  [[nodiscard]] Entity Add(Role role);
  void Remove(Entity of);
  [[nodiscard]] bool Alive(Entity of) const;
  [[nodiscard]] Role RoleOf(Entity of) const;

  [[nodiscard]] bool Give(Entity to, Tag tag);
  [[nodiscard]] bool Has(Entity of, Tag tag) const;

  [[nodiscard]] bool Link(Entity from, Relation how, Entity to);
  [[nodiscard]] bool Relink(Entity from, Relation how, Entity to);
  [[nodiscard]] Entity TargetOf(Entity of, Relation how) const;
  [[nodiscard]] size_t Targets(Entity of, Relation how, Entity into[], size_t room) const;

  [[nodiscard]] size_t Sources(Entity to, Relation how, Entity into[], size_t room) const;
  [[nodiscard]] size_t Cast(Role role, Entity into[], size_t room) const;
  [[nodiscard]] size_t Pairs(Relation how, Entity from[], Entity to[], size_t room) const;
  [[nodiscard]] size_t Bearing(Tag tag, Role role, Entity into[], size_t room) const;

  [[nodiscard]] Entity Instantiate(Entity prefab);
  [[nodiscard]] Entity CopyOf(Entity instance, Entity prefabChild) const;

  [[nodiscard]] bool Offer(Entity at, Tag activity, size_t seats);
  [[nodiscard]] size_t Offering(Tag activity, Entity into[], size_t room) const;
  [[nodiscard]] bool Claim(Entity by, Entity at);
  [[nodiscard]] bool Use(Entity by, Entity at);
  [[nodiscard]] bool Release(Entity by, Entity at);
  [[nodiscard]] Seat SeatOf(Entity by, Entity at) const;

  [[nodiscard]] size_t Capacity() const;
  [[nodiscard]] std::string_view Error() const;

  [[nodiscard]] size_t Touched() const;
  void ResetTouched();


private:
  struct Kept;
  std::unique_ptr<Kept> Kept_;
};

}
#endif
