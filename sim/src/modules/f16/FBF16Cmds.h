/* FlightBox — FBF16Cmds: the F-16's AN/ALE-47 countermeasures dispenser, an override of the generic
 * dispenser (systems/FBCountermeasureSystem — read its banner first; the program machine, the mode state
 * machine and the consent rules live there, this file carries only what is ALE-47 about it).
 *
 * WHAT IS ALE-47 ABOUT IT:
 *   - THE MAGAZINE. "FCDs in the body fairing; ground crew sets loadout, max 120 combined (typical 60
 *     chaff / 60 flare)" (doc/f16/defence-rwr-cm.md §1). 60/60 is what this jet powers up with unless a
 *     mission declares otherwise, and 120 is the ceiling the mission line is checked against.
 *   - THE PROGRAM TABLE. The parameter SCHEMA is the source's (burst/salvo quantity and interval per
 *     type, §2.2); the VALUES are FlightBox's own [SET] and are marked as such, because the guides
 *     document the DED page and its ranges and never what the six programs are loaded with — that is a
 *     squadron's business, not a manual's. What the table is CHOSEN for is stated per program below, so
 *     a mission that wants a different pattern changes a number rather than the model.
 *   - THE BINGO QUANTITIES. §2.2's CMDS BINGO page is a pilot-set 0-99 per type; ten of each [SET] is what
 *     this jet is briefed with, i.e. the point at which automatic dispensing stops and what is left
 *     belongs to the pilot.
 * The threat->program mapping is NOT overridden: the generic doctrine (dense pattern against a missile,
 * economical repeating one against a track) is what this table is built around, and an override that
 * returned the same two numbers would be the empty derivation CLAUDE.md forbids. */
#ifndef FBF16CMDS_H
#define FBF16CMDS_H

#include "FBCountermeasureSystem.h"

namespace FlightBox {

class FBF16Cmds : public FBCountermeasureSystem {
public:
  /* doc/f16/defence-rwr-cm.md §1: typical 60/60, 120 combined maximum. */
  static constexpr int kTypicalChaff = 60;
  static constexpr int kTypicalFlare = 60;
  static constexpr int kMaxCombined = 120;

  /* The briefed BINGO quantities (class banner) [SET]. */
  static constexpr int kBingoChaff = 10;
  static constexpr int kBingoFlare = 10;

  FBF16Cmds();
};

} // namespace FlightBox
#endif
