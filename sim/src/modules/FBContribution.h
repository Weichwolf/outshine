/* FlightBox — the module CONTRIBUTION list: the closed set of things a module may put into the world.
 * Sibling of FBCapability.h in shape (ONE table, four expansions, include-free, runtime-readable) and
 * complementary in subject: that one says what a UNIT HAS, this one says what a MODULE OWNS and EMITS.
 * A word earns its row only by having a reader no other row's reader can be — so there is no `collision`
 * (that is `instances`/`surface` read by physics) and no `body` (that is `instances` backed by
 * `simulated`). doc/module-contract.md §Spec 4. */
#ifndef FBCONTRIBUTION_H
#define FBCONTRIBUTION_H

#include <cstdint>

namespace FlightBox::Modules {

/* The contract's two halves. A module owes ONE of each — FBContributionsSatisfyContract below. */
enum class FBSide : std::uint8_t { State, Geometry };

/* HOW ONE ITEM IN THE CHANNEL IS NAMED. `Address` is integer and ENUMERATED — a generator's loop
 * variable, a mission line, a (tile, cell, slot) — never a computed position: WGSL pins no rounding
 * mode, and at 10 km one f32 ULP is already 0.98 mm, so a float key loses objects
 * (doc/render/gpu-determinism.md GD2). `Point` carries no identity at all, which is why it is the one
 * key a State row may not use. */
enum class FBKey : std::uint8_t { Point, Address, Content, Global };

/* WHAT THIS WORD PUTS IN A WORLD SNAPSHOT, i.e. the row kind it owes the save file. `None` is not an
 * omission: geometry is derived and disposable, and the well-formedness check below is that sentence
 * said to the compiler. doc/persistent-world.md §2. */
enum class FBSnapshot : std::uint8_t { None, Hash, Seed, Bytes, Projection, Scalars };

/* Id, SIDE, WIRE NAME, key, snapshot row. The wire name is the word a `doc/modules/<id>/module.md`, a
 * mod file and a tool schema spell; it is lower case and single, because it is validated by string
 * equality against this table and not read by a human who would forgive a plural. */
#define FB_MODULE_CONTRIBUTIONS(X)                                            \
  X(Surface,   Geometry, "surface",   Point,   None)                          \
  X(Volume,    Geometry, "volume",    Point,   None)                          \
  X(Instances, Geometry, "instances", Address, None)                          \
  X(Dataset,   State,    "dataset",   Content, Hash)                          \
  X(Generated, State,    "generated", Address, Seed)                          \
  X(Delta,     State,    "delta",     Address, Bytes)                         \
  X(Simulated, State,    "simulated", Address, Projection)                    \
  X(Ambient,   State,    "ambient",   Global,  Scalars)

enum class FBContribution : std::uint8_t {
#define FB_CONTRIB_ENUM(Id, Side, Wire, Key, Snap) Id,
  FB_MODULE_CONTRIBUTIONS(FB_CONTRIB_ENUM)
#undef FB_CONTRIB_ENUM
};

inline constexpr int kFBContributionCount = 0
#define FB_CONTRIB_COUNT(Id, Side, Wire, Key, Snap) + 1
    FB_MODULE_CONTRIBUTIONS(FB_CONTRIB_COUNT)
#undef FB_CONTRIB_COUNT
    ;

using FBContributionMask = std::uint32_t;
static_assert(kFBContributionCount <= 32, "FBContributionMask is the declaration set — widen it");

constexpr FBContributionMask FBContributionBit(FBContribution c) {
  return (FBContributionMask)1u << (unsigned)c;
}

struct FBContributionDesc {
  FBContribution Id;
  const char *Name;
  FBSide Side;
  FBKey Key;
  FBSnapshot Snapshot;
};

inline constexpr FBContributionDesc kFBContributionTable[kFBContributionCount] = {
#define FB_CONTRIB_ROW(Id, Side, Wire, Key, Snap) \
  {FBContribution::Id, Wire, FBSide::Side, FBKey::Key, FBSnapshot::Snap},
    FB_MODULE_CONTRIBUTIONS(FB_CONTRIB_ROW)
#undef FB_CONTRIB_ROW
};

constexpr FBContributionMask FBContributionSideMask(FBSide side) {
  FBContributionMask m = 0;
  for (const FBContributionDesc &c : kFBContributionTable)
    if (c.Side == side) m |= FBContributionBit(c.Id);
  return m;
}

/* THE CONTRACT ITSELF, as one predicate: a module owns a piece of world state and derives geometry from
 * it. A declaration that names only one side describes something other than a module. */
constexpr bool FBContributionsSatisfyContract(FBContributionMask m) {
  return (m & FBContributionSideMask(FBSide::State)) != 0 &&
         (m & FBContributionSideMask(FBSide::Geometry)) != 0;
}

constexpr bool FBContributionTableWellFormed() {
  for (const FBContributionDesc &c : kFBContributionTable) {
    if ((c.Side == FBSide::Geometry) != (c.Snapshot == FBSnapshot::None)) return false;
    if (c.Side == FBSide::State && c.Key == FBKey::Point) return false;
  }
  return true;
}
static_assert(FBContributionTableWellFormed(),
              "state is authoritative and saved, geometry is derived and disposable — and state is "
              "never keyed by a float");

} // namespace FlightBox::Modules
#endif
