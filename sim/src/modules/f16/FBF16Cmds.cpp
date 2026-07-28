#include "FBF16Cmds.h"

namespace FlightBox::Modules {

/* The six programs [SET], each for a stated job the automatic mapping picks between — full table with
 * those jobs: doc/modules-f16.md §7. In order: BREAK LOCK, MIXED, FLARE, SUSTAINED, SLAP,
 * BYPASS. */
FBF16Cmds::FBF16Cmds() {
  SetLoadout(kTypicalChaff, kTypicalFlare);
  SetBingo(kBingoChaff, kBingoFlare);

  FBCmProgram p{};
  p.Chaff = {2, 0.10, 2, 1.00};
  InstallProgram(1, p);

  p = FBCmProgram{};
  p.Chaff = {2, 0.10, 2, 2.00};
  p.Flare = {1, 0.10, 2, 2.00};
  InstallProgram(2, p);

  p = FBCmProgram{};
  p.Flare = {2, 0.10, 4, 1.00};
  InstallProgram(3, p);

  p = FBCmProgram{};
  p.Chaff = {2, 0.10, 4, 4.00};
  InstallProgram(4, p);

  p = FBCmProgram{};
  p.Chaff = {1, 0.10, 1, 1.00};
  p.Flare = {1, 0.10, 1, 1.00};
  InstallProgram(5, p);

  p = FBCmProgram{};
  p.Chaff = {1, 0.10, 1, 1.00};
  p.Flare = {1, 0.10, 1, 1.00};
  InstallProgram(6, p);
}

} // namespace FlightBox::Modules
