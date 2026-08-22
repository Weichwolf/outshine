#ifndef OUTSHINE_SCENE_REGISTER_H
#define OUTSHINE_SCENE_REGISTER_H

#include <cstddef>
#include <cstdint>

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
  static constexpr Tag DoesSteer{0x01010000};
  static constexpr Tag DoesDrive{0x01020000};
  static constexpr Tag DoesBrake{0x01030000};
  static constexpr Tag DoesLamp{0x01040000};
  static constexpr Tag Offers{0x02000000};
  static constexpr Tag OffersRefuel{0x02010000};
};

namespace tags {
inline constexpr Tag Does = TagCatalogue::Does;
inline constexpr Tag DoesSteer = TagCatalogue::DoesSteer;
inline constexpr Tag DoesDrive = TagCatalogue::DoesDrive;
inline constexpr Tag DoesBrake = TagCatalogue::DoesBrake;
inline constexpr Tag DoesLamp = TagCatalogue::DoesLamp;
inline constexpr Tag Offers = TagCatalogue::Offers;
inline constexpr Tag OffersRefuel = TagCatalogue::OffersRefuel;
} // namespace tags

static_assert(tags::DoesSteer.Within(tags::Does) && !tags::Does.Within(tags::DoesSteer),
              "a tag is within its parent and never the other way round");

enum class Relation : uint8_t { IsA, ChildOf, DrivenBy, Uses, Assigned, Holds };
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
    {Relation::Holds, true, true, false, false, RoleBit(Role::Body), {}, kNoRelation},
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
} // namespace scene_register_checked

} // namespace outshine

#endif
