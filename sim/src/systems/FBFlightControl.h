/* FlightBox — FBFlightControl: the thin fly-by-wire inner loop, the DEFAULT implementation of a
 * module's FCS system. Consumes FBAutopilot guidance and the JSBSim state, emits normalized
 * stick/throttle for the airframe. Two inner kinds, selected by the `Flcs` config flag (not a
 * subclass — this is tuning, not behavior):
 *   FLCS  — command a real FLCS like a pilot (F-16): bank error -> roll-rate stick, a g-command PI
 *           with 1/cos(bank) turn feedforward + vs-error bias/integral -> pitch stick, lateral-g
 *           nulling -> pedals, PI throttle. The airframe's FLCS is the stabilizer; we command it.
 *   Raw   — attitude-hold PD on the surfaces (models without an FLCS).
 * Port of flightctl.h's inner loop; numerics preserved verbatim (equivalence-tested).
 *
 * Run() is virtual: a module whose inner loop genuinely works differently (not just different
 * gains — a digital FBW law with no stick/rate abstraction in common with this one) overrides it;
 * FBF16Module composes this DEFAULT unmodified via its F16() gain preset. */
#ifndef FBFLIGHTCONTROL_H
#define FBFLIGHTCONTROL_H

#include "FBAutopilot.h"
#include "FBTelemetry.h"

namespace FlightBox {

struct FBControls { double Roll, Pitch, Yaw, Thr; };

class FBFlightControl : public FBTelemetrySource {
public:
  FBFlightControl();
  virtual ~FBFlightControl() = default;

  /* One 100 Hz step: guidance + state -> stick/throttle. Mutates the loop integrators. The one
   * override point (see the class banner). Caches the returned FBControls for SampleTelemetry — at
   * 100 Hz the telemetry bus (10 Hz mission cadence) always reads the latest one. */
  virtual FBControls Run(const FBGuidance &g, const fb_fdm_state &s);

  const char *TelemetryName() const override { return "flightcontrol"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

  void Reset(void);                      /* zero all integrator state (new flight) */
  static FBFlightControl F16(void);      /* the flown F-16 preset (FLCS-command inner) */

  /* gains — public config block; defaults via ctor, F-16 values via F16() */
  int    Flcs;                           /* 1 = FLCS-command inner, 0 = raw-surface PD */
  double RollStickMax;                   /* gentle pilot roll-in cap (see flightctl.h history) */
  double KRollRate, KG, KGi, KVs2g, KVsi, KNy, KNyi, KTi, KpSpd, ThrTrim;
  double KpRoll, KdRoll, KpPitch, KdPitch, KdYaw, KCoord, PitchMaxDeg, KAltRaw;
  double NzSlew;                         /* max commanded-g change per second */

  double GetGIterm(void) const { return GIterm; }
  double GetVsIterm(void) const { return VsIterm; }

private:
  double GIterm, VsIterm, NyIterm, ThrIterm, NzPrev;
  FBControls LastControls_{0.0, 0.0, 0.0, 0.0};
};

} // namespace FlightBox
#endif
