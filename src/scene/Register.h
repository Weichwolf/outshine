#ifndef OUTSHINE_SCENE_REGISTER_H
#define OUTSHINE_SCENE_REGISTER_H

#include <cstddef>
#include <cstdint>

namespace outshine {

enum class Kind : uint8_t { Body, Mind, Tool, Assignment };
inline constexpr size_t kKinds = 4;

[[nodiscard]] constexpr uint8_t KindBit(Kind kind) { return (uint8_t)(1u << (uint8_t)kind); }

struct Tag {
  uint32_t Value = 0;

  [[nodiscard]] constexpr bool Within(Tag parent) const {
    uint32_t mask = 0xFFFFFFFFu;
    for (uint32_t held = parent.Value; held != 0 && (held & 0xFFu) == 0; held >>= 8) { mask <<= 8; }
    return parent.Value != 0 && (Value & mask) == parent.Value;
  }
  [[nodiscard]] constexpr bool operator==(Tag other) const { return Value == other.Value; }
};

namespace tags {
inline constexpr Tag Does{0x01000000};
inline constexpr Tag DoesSteer{0x01010000};
inline constexpr Tag DoesDrive{0x01020000};
inline constexpr Tag DoesBrake{0x01030000};
inline constexpr Tag DoesLamp{0x01040000};
} // namespace tags

static_assert(tags::DoesSteer.Within(tags::Does) && !tags::Does.Within(tags::DoesSteer),
              "a tag is within its parent and never the other way round");

enum class Relation : uint8_t { IsA, ChildOf, DrivenBy, Uses, Assigned };
inline constexpr size_t kRelations = 5;

inline constexpr Relation kNoRelation = (Relation)0xFF;

struct Rule {
  Relation Named = kNoRelation;
  bool Exclusive = false;
  bool Acyclic = false;
  uint8_t TargetKinds = 0;
  Tag SourceDoes{};
  Relation Requires = kNoRelation;
};

inline constexpr Rule kRules[kRelations] = {
    {Relation::IsA, true, true, KindBit(Kind::Body), {}, kNoRelation},
    {Relation::ChildOf, true, true,
     (uint8_t)(KindBit(Kind::Body) | KindBit(Kind::Mind) | KindBit(Kind::Tool) |
               KindBit(Kind::Assignment)),
     {}, kNoRelation},
    {Relation::DrivenBy, true, false, KindBit(Kind::Mind), tags::Does, kNoRelation},
    {Relation::Uses, false, false, KindBit(Kind::Tool), {}, kNoRelation},
    {Relation::Assigned, true, false, KindBit(Kind::Assignment), {}, Relation::Uses},
};

[[nodiscard]] constexpr const Rule &RuleOf(Relation relation) {
  return kRules[(size_t)relation];
}

namespace scene_register_checked {
constexpr bool EachRuleStandsAtItsOwnRelation() {
  for (size_t at = 0; at < kRelations; ++at) {
    if ((size_t)kRules[at].Named != at) { return false; }
    if (kRules[at].TargetKinds == 0) { return false; }
  }
  return true;
}
static_assert(EachRuleStandsAtItsOwnRelation(),
              "every relation carries its rule, and no rule allows nothing");
} // namespace scene_register_checked

} // namespace outshine

#endif
