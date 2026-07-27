#include "FBF16Cmds.h"

namespace FlightBox {

/* The six programs (see the header on what status these numbers have). Each one exists for a stated job,
 * and the jobs are what the automatic mapping in systems/FBCountermeasureSystem::AutomaticProgram picks
 * between:
 *
 *   1 BREAK LOCK   chaff 2 x 0.10 s, 2 salvos 1.00 s apart   = 4 cartridges in ~1.1 s
 *                  The dense reflex, and the answer to a missile: four clouds within one second put
 *                  more than one competing return into the seeker's beam while the geometry is still
 *                  changing fast.
 *   2 MIXED        chaff 2 x 0.10 s / 2 salvos, flare 1 / 2 salvos, 2.00 s apart
 *                  The unknown-threat answer: something for a radar and something for an infrared
 *                  seeker, at the cost of using two magazines at once.
 *   3 FLARE        flare 2 x 0.10 s, 4 salvos 1.00 s apart
 *                  Infrared only. Dispensed and counted; it has nothing to fool yet and the generic
 *                  class says so plainly.
 *   4 SUSTAINED    chaff 2 x 0.10 s, 4 salvos 4.00 s apart   = 8 cartridges over ~12 s
 *                  The answer to a radar merely TRACKING you, and the one AUTO repeats: slow enough
 *                  that a long lock does not empty a 60-round magazine before the fight is decided,
 *                  dense enough that a cloud is always inside its useful life (core/
 *                  FBCountermeasure.h's kChaffLifeS) when the next one appears.
 *   5 SLAP         one chaff + one flare, single salvo — the left-wall dispense button.
 *   6 BYPASS       one chaff + one flare, exactly, per doc/f16/defence-rwr-cm.md §2.2. */
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

} // namespace FlightBox
