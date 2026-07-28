#include "FBMig29Cmds.h"

namespace FlightBox::Modules {

/* The programmes [SET] — the BVP-30-26's three documented geometry programmes (GROUND/FHS/RHS,
 * defence-rwr-cm.md §3) mapped onto the generic slot machine, so the base class's AutomaticProgram
 * (1 = missile reflex, 4 = a track) keeps working unchanged and the pilot's SEMI/AUTO path is the
 * F-16's. The VALUES are FlightBox's own, because no source states a burst/salvo parameter for this
 * dispenser (§7 gap 1). They are lighter than the F-16's for the same reason the BINGO is: the magazine
 * is half the size, so a programme that empties it in a single lock would leave nothing for the next
 * threat. */
FBMig29Cmds::FBMig29Cmds() {
  SetLoadout(kTypicalChaff, kTypicalFlare);
  SetBingo(kBingoChaff, kBingoFlare);

  /* 1 — FHS break-lock: the dense chaff reflex against a forward-hemisphere radar shot, the programme
   * AutomaticProgram picks for a MISSILE. Four clouds inside a second, as on the F-16. */
  FBCmProgram p{};
  p.Chaff = {2, 0.10, 2, 1.00};
  InstallProgram(1, p);

  /* 2 — MIXED: something for a radar and something for an infrared seeker at once, for an unknown
   * threat, at the price of both magazines. */
  p = FBCmProgram{};
  p.Chaff = {1, 0.10, 2, 2.00};
  p.Flare = {1, 0.10, 2, 2.00};
  InstallProgram(2, p);

  /* 3 — FLARE only: the infrared answer, and the one this stage exists for. A cartridge lives 4 s
   * (core/FBCountermeasure.h), so a four-salvo train straddles an endgame; the pilot throws it manually
   * (no MAWS on the 9-12 — defence-rwr-cm.md §5 — so nothing auto-triggers it). */
  p = FBCmProgram{};
  p.Flare = {2, 0.10, 4, 1.00};
  InstallProgram(3, p);

  /* 4 — RHS sustained: the sparing, repeating chaff answer to a radar that is only TRACKING, the one
   * AUTO repeats. Slower than the F-16's so the smaller magazine survives a long lock. */
  p = FBCmProgram{};
  p.Chaff = {1, 0.10, 4, 4.00};
  InstallProgram(4, p);

  /* 5, 6 — one chaff + one flare each, the manual-dispense and bypass singletons, as on the F-16. */
  p = FBCmProgram{};
  p.Chaff = {1, 0.10, 1, 1.00};
  p.Flare = {1, 0.10, 1, 1.00};
  InstallProgram(5, p);
  InstallProgram(6, p);
}

} // namespace FlightBox::Modules
