/* FlightBox — FBMfdSystem: the MFD bank as a simulated box. It owns which page stands on which of the
 * three bays, answers FBCommandTarget::MfdPageSelect, and publishes both facts into FBMfdBlock so that
 * every consumer — the drawing side, the pilot, a telemetry reader — sees the same reading.
 *
 * THE LAYERING (CLAUDE.md: FBCore -> Interface -> Default -> module override) runs through the
 * CATALOGUE, not through a subclass: the frame is generic, the module DECLARES which pages its
 * cockpit can ever show, and the current LOADOUT cuts that down again. Nothing here is virtual —
 * an aircraft differs in its catalogue, not in how a bezel button works.
 * doc/modules/f16/cockpit-displays.md, doc/systems.md. */
#ifndef FBMFDSYSTEM_H
#define FBMFDSYSTEM_H

#include "FBState.h"

namespace FlightBox::Systems {

class FBMfdSystem {
public:
  /* The module's own catalogue, once, before the first Run(): ordinal i is `pages[i]` for the whole
   * sortie, because that ordinal is what a command carries. Excess entries are dropped. */
  void DeclarePages(const FBMfdPage *pages, int n);

  /* Re-cuts the catalogue against what the aircraft currently HAS (published blocks + loadout), keeps
   * the bays consistent with it and publishes. Runs at the Displays slot's cadence. */
  void Run(FBState &state, double nowS);

  /* The command's effect. False = this cockpit cannot show that page right now, which the module
   * turns into a REJECT — a jet that accepted it would claim a page it does not have. */
  bool Select(int ordinal, double nowS);

  int Attention() const { return Bay_[kMfdAttentionBay]; }

private:
  /* The two flanking bays carry the remaining selectable pages in catalogue order, so the three bays
   * never show the same page twice and the middle one is always the pilot's own choice. */
  void PlaceBays();

  FBMfdPage Pages_[kMfdMaxPages]{};
  int       PageCount_ = 0;
  uint32_t  Available_ = 0;
  int       Bay_[kMfdBays] = {-1, -1, -1};
  int       LastSelectPage_ = -1;
  double    LastSelectS_ = -1.0;
};

} // namespace FlightBox::Systems
#endif
