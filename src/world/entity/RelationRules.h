#ifndef OUTSHINE_WORLD_ENTITY_RELATIONRULES_H
#define OUTSHINE_WORLD_ENTITY_RELATIONRULES_H

#include <algorithm>
#include <array>
#include "world/EntityRegistry.h"

namespace outshine {
inline constexpr size_t kRoles = 4;

[[nodiscard]] constexpr uint8_t RoleBit(Role kind) {
  return static_cast<uint8_t>(1u << static_cast<uint8_t>(kind));
}

inline constexpr size_t kRelations = 6;

inline constexpr Relation kNoRelation = static_cast<Relation>(0xFF);

struct RelationRule {
  Relation Named = kNoRelation;
  bool Exclusive = false;
  bool Acyclic = false;
  bool OwnedByTarget = false;
  bool SameRole = false;
  uint8_t TargetRoles = 0;
  Tag SourceDoes;
  Relation Requires = kNoRelation;
};

inline constexpr uint8_t kEveryRole =
    static_cast<uint8_t>(unsigned{RoleBit(Role::Body)} | unsigned{RoleBit(Role::Mind)} |
                         unsigned{RoleBit(Role::Tool)} | unsigned{RoleBit(Role::Assignment)});

inline constexpr std::array<RelationRule, kRelations> kRules = {{
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
}};

[[nodiscard]] constexpr const RelationRule &RuleOf(Relation relation) {
  return kRules[static_cast<size_t>(relation)];
}

namespace entity_registry_checked {
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
  return std::ranges::none_of(kRules,
                              [](const auto &kRule) { return kRule.Acyclic && !kRule.Exclusive; });
}

static_assert(EveryAcyclicRelationIsExclusive(),
              "the cycle walk follows one target per hop, so an acyclic relation must be "
              "exclusive -- widen the walk before you relax this");

constexpr size_t OwnedRelationCount() {
  size_t owned = 0;
  for (const auto kRule : kRules) {
    if (kRule.OwnedByTarget) { ++owned; }
  }
  return owned;
}

constexpr bool EveryOwnedRelationIsExclusive() {
  return std::ranges::none_of(
      kRules, [](const auto &kRule) { return kRule.OwnedByTarget && !kRule.Exclusive; });
}

static_assert(EveryOwnedRelationIsExclusive(),
              "the felling stack pushes one entry per owned in-edge, and its reserve is "
              "capacity x owned-relations ONLY while each entity has at most one owner "
              "per owned relation -- widen the reserve before you relax this");
}

inline constexpr size_t kOwnedRelations = entity_registry_checked::OwnedRelationCount();
static_assert(kOwnedRelations >= 1, "removal owns at least the ChildOf chain");

inline constexpr size_t kPairsPerEntity = 8;
inline constexpr size_t kTagsPerEntity = 8;
inline constexpr size_t kSeatsPerOffer = 4;

[[nodiscard]] constexpr bool IsValid(Role role) noexcept {
  return static_cast<size_t>(role) < kRoles;
}

[[nodiscard]] constexpr bool IsValid(Relation relation) noexcept {
  return static_cast<size_t>(relation) < kRelations;
}
}
#endif
