/* FlightBox — FBF16FireControl: the F-16's "B" range-provider — slant range to the active steerpoint
 * computed from horizontal distance + the altitude difference to the STEERPOINT'S OWN elevation (baro
 * method), not FCR ranging (that needs a real radar return, which FlightBox doesn't model). Matches the
 * DCS/real-jet HUD convention (doc/f16/hud-symbology.md: "B: Range computed using steerpoint elevation/
 * barometric elevation") and the FlightGear F-16 mod's `steerpoints.getCurrentSlantRange()` (same
 * Pythagorean horizontal-distance + altitude-diff method, cross-checked against its GPL-2.0 Nasal
 * source for the FORMULA only — no code copied).
 *
 * F-16-specific (not a generic systems/ slot): the 'B' provider letter and this exact computation are
 * this airframe's fire-control convention, not every module's. Reads FBState (steerDistNm, steerElevFt,
 * alt), writes FBState (steerSlantNm, rangeProvider) — never touches another system directly. */
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
