/* FlightBox — FBMig29Cmds: the BVP-30-26, an override of sensors/FBCountermeasureSystem carrying only
 * the magazine, the programmes and the briefed BINGO quantities — the mirror of FBF16Cmds one airframe
 * over, and the piece that makes the campaign's last one-way asymmetry (doc/duels.md D5,
 * doc/modules/mig29/module.md gap 4g) two-sided: the F-16 answers the R-73 with flares, and now the MiG
 * answers the AIM-9 the same way, through the same deterministic seduction model
 * (sensors/FBIrstSystem::SelectFlare) that reads a dispensed cloud out of the unit signature.
 *
 * WHAT IS DOCUMENTED AND WHAT IS SET. The blocks and the calibre are [DOC]
 * (doc/modules/mig29/defence-rwr-cm.md §3): two BVP-30-26 blocks, 30 PPI-26 cartridges each = 60
 * combined, 26 mm. Everything a programme is made of — burst/salvo counts and intervals, and the
 * chaff/flare split of the 60 — is a NAMED GAP in that file (§3, §7 gaps 1-2): no source states it.
 * So the programme values are [SET], flagged exactly as the F-16's ALE-47 values are, and the split is
 * [SET] too. The real jet has THREE geometry-selected programmes (GROUND/FHS/RHS) rather than the
 * F-16's six-slot mode machine; FlightBox keeps the generic slot machine (so the pilot's SEMI/AUTO on
 * the RWR block works identically to the F-16's) and maps the doctrine onto it — the simplification note
 * is about the real hardware, not this class. */
#ifndef FBMIG29CMDS_H
#define FBMIG29CMDS_H

#include "FBCountermeasureSystem.h"

namespace FlightBox::Modules {

class FBMig29Cmds : public Sensors::FBCountermeasureSystem {
public:
  /* [DOC] defence-rwr-cm.md §3: 2 x BVP-30-26, 30 PPI-26 cartridges each = 60 combined, 26 mm. */
  static constexpr int kMaxCombined = 60;
  /* [SET] The chaff/flare split of the 60 is a named gap (§3, §7 gap 2 — no source states it, nor
   * whether it is a ground-crew choice). An even division is the neutral assumption, and 30/30 leaves
   * this jet able to answer a radar shot and an infrared shot alike. */
  static constexpr int kTypicalChaff = 30;
  static constexpr int kTypicalFlare = 30;

  /* [SET] Where AUTOMATIC dispensing stops and the rest is the pilot's — half the F-16's 10/10, in
   * proportion to a magazine half the size. No source states a BINGO for the BVP-30-26 (it has no DED
   * page; the panel is a simple counter, §3), so this is FlightBox's own, flagged. */
  static constexpr int kBingoChaff = 5;
  static constexpr int kBingoFlare = 5;

  FBMig29Cmds();
};

} // namespace FlightBox::Modules
#endif
