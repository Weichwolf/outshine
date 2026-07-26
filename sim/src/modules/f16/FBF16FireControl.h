/* FlightBox — FBF16FireControl: the F-16's "B" range-provider — slant range to the active steerpoint
 * computed from horizontal distance + the altitude difference to the STEERPOINT'S OWN elevation (baro
 * method), not FCR ranging (that needs a real radar return, which FlightBox doesn't model). Matches the
 * DCS/real-jet HUD convention (doc/f16/hud-symbology.md: "B: Range computed using steerpoint elevation/
 * barometric elevation") and the FlightGear F-16 mod's `steerpoints.getCurrentSlantRange()` (same
 * Pythagorean horizontal-distance + altitude-diff method, cross-checked against its GPL-2.0 Nasal
 * source for the FORMULA only — no code copied).
 *
 * F-16-specific (not a generic systems/ slot): the 'B' provider letter and this exact computation are
 * this airframe's fire-control convention, not every module's. READS the nav block (steerpoint distance
 * and elevation) and the platform block (own altitude), WRITES the fire-control block — a documented
 * fusion of two source blocks, which is why it checks both heads: with either one Invalid there is no
 * slant range to publish, and 'B' with a garbage number next to it is worse than a decluttered readout.
 * (The flat FBState hid a real instance of exactly that: this class read an altitude field the module
 * never filled, so the published slant range was computed against 0 m ASL for every flight.) */
#ifndef FBF16FIRECONTROL_H
#define FBF16FIRECONTROL_H

#include "FBState.h"

namespace FlightBox {

class FBF16FireControl {
public:
  virtual ~FBF16FireControl() = default;

  virtual void Run(FBState &state, double dt);
};

} // namespace FlightBox
#endif
