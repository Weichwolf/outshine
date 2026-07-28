/* FlightBox — FBF16Cmds: the AN/ALE-47, an override of systems/FBCountermeasureSystem carrying only the
 * magazine, the six programs and the briefed BINGO quantities. The program SCHEMA is [DOC]; the VALUES
 * are [SET], because the sources document the DED page and never what the programs are loaded with.
 * The threat->program mapping is deliberately NOT overridden — the generic doctrine is what this table
 * is built around. Programs and their jobs: doc/modules-f16.md §7. */
#ifndef FBF16CMDS_H
#define FBF16CMDS_H

#include "FBCountermeasureSystem.h"

namespace FlightBox::Modules {

class FBF16Cmds : public Sensors::FBCountermeasureSystem {
public:
  /* [DOC] defence-rwr-cm.md §1: typical 60/60, 120 combined maximum. */
  static constexpr int kTypicalChaff = 60;
  static constexpr int kTypicalFlare = 60;
  static constexpr int kMaxCombined = 120;

  /* The briefed BINGO quantities [SET]: where AUTOMATIC dispensing stops and the rest is the pilot's. */
  static constexpr int kBingoChaff = 10;
  static constexpr int kBingoFlare = 10;

  FBF16Cmds();
};

} // namespace FlightBox::Modules
#endif
