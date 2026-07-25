/* FlightBox — FBDisplaySystem: cockpit displays, the DEFAULT implementation of a module's Displays
 * system slot. Interface + a REAL default (not NoOp, unlike the rest of FBSystemSlots.h): BuildHud is
 * the generic MIL-STD-1787-ish HUD every airframe starts with — waterline, conformal horizon,
 * heading/groundspeed/altitude tapes with a steerpoint marker, "NO TELEMETRY" fallback. A module whose
 * HUD genuinely differs (real MFD pages, F-16-specific symbology) subclasses and overrides BuildHud,
 * same override-point pattern as FBAutopilot::Run/FBFlightControl::Run; FBF16Module composes this
 * default unmodified until it does.
 *
 * Run() stays the periodic display-logic slot (MFD pages, warning lights, ...) FBF16Module already
 * throttles at 20 Hz — separate from BuildHud, which FBHudStage calls once per rendered frame. */
#ifndef FBDISPLAYSYSTEM_H
#define FBDISPLAYSYSTEM_H

#include "FBHudGeometry.h"
#include "FBMasterMode.h"
#include "FBState.h"

namespace FlightBox {

/* What BuildHud needs beyond FBState: viewport size and AGL are render/telemetry plumbing, not sim
 * state, so they travel separately rather than bloating FBState for one consumer. */
struct FBHudEnv {
  int Width, Height;
  float Agl;   /* m, ASL - DEM ground under the aircraft; AGL readout + horizon-dip fallback (alt<=1) */
  bool Have;   /* telemetry present; false -> BuildHud draws only the NO TELEMETRY fallback */
};

class FBDisplaySystem {
public:
  virtual ~FBDisplaySystem() = default;

  virtual void Run(const FBState &state, FBMasterMode mode, double dt) { (void)state; (void)mode; (void)dt; }

  /* The HUD override point (see the class banner). Regenerates the full symbology into `out` for one
   * frame; FBHudStage uploads it verbatim. Const: BuildHud reads state, it does not own any of it. */
  virtual void BuildHud(const FBState &state, const FBHudEnv &env, FBHudGeometry &out) const;
};

} // namespace FlightBox
#endif
