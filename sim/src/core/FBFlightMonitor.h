/* The ONE incorruptible K.O. authority for a flight's PHYSICAL state — model-derived and
 * airframe-agnostic by construction: no module type, no module-declared numbers, only generic
 * FDM/ground-reaction quantities. Deliberately physics-only; "did the MISSION succeed" is
 * FBMissionMonitor's separate question. Instantiated and fed exclusively by the app-level driver.
 * Checks, thresholds and their derivation: doc/core.md, Abschnitt 4.1. */
#ifndef FBFLIGHTMONITOR_H
#define FBFLIGHTMONITOR_H
#include <string>

namespace FlightBox {

/* One tick's FDM truth — deliberately NOT fb_fdm_state, because core/ does not depend on fdm/. Every
 * field is a generic quantity every JSBSim model exposes. */
struct FBFlightMonitorSample {
  double LatDeg = 0.0, LonDeg = 0.0;
  double ElevM = 0.0;        /* m ASL */
  double GroundAslM = 0.0;   /* m ASL, the terrain under the aircraft this tick */
  double RollDeg = 0.0, PitchDeg = 0.0;
  double PDegS = 0.0, QDegS = 0.0, RDegS = 0.0;   /* body rates, deg/s */
  double VsMs = 0.0;         /* vertical speed, + = climb */
  double TasMs = 0.0;        /* used ONLY to derive the flight-path angle, never as an aero/AoA surrogate */
  double GearPosNorm = 0.0;  /* 0=up..1=down, the model's own lagged gear position */
  double GearForceLbs = 0.0; /* peak wheeled-gear strut compression force this tick (FBFdm::GetMaxGearForceLbs) */
  double WeightLbs = 0.0;    /* the model's own current static weight (FBFdm::GetWeightLbs) */
  bool   AnyWow = false;             /* any BOGEY (wheeled) contact compressed */
  bool   StructureContact = false;   /* any declared STRUCTURE contact point compressed */
  bool   FdmFault = false;           /* the integrator gave up this tick — a plain bool so the monitor
                                      * stays fdm-decoupled */
};

/* NumericalDivergence is checked FIRST in Tick(): every other check is a COMPARISON, and every
 * comparison against NaN is false — without it a diverged FDM sails past all of them into a TIMEOUT. */
enum class FBKoReason { None, NumericalDivergence, StructureContact, CfitPenetration, GearUpContact,
                        HardLanding, AttitudeContact, Loc };
const char *FBKoReasonStr(FBKoReason r);

class FBFlightMonitor {
public:
  FBFlightMonitor() = default;

  /* Returns true on the ONE tick a K.O. first trips, then LATCHES (every later call is a no-op), so a
   * caller's `if (Tick(...))` fires exactly once. Emits its own FBLog::Error with the measured values. */
  bool Tick(const FBFlightMonitorSample &s, double simTimeS);

  bool               Tripped() const { return Reason_ != FBKoReason::None; }
  FBKoReason         Reason() const { return Reason_; }
  const std::string &Detail() const { return Detail_; }

private:
  bool Trip(FBKoReason reason, const std::string &detail, const FBFlightMonitorSample &s);

  FBKoReason  Reason_ = FBKoReason::None;
  std::string Detail_;

  double LastSimTimeS_ = -1.0;     /* -1 = no previous tick yet (first Tick computes dt=0) */
  double LocTimerS_ = 0.0;
  double StallTimerS_ = 0.0;
  /* Only the two BINARY, WOW-driven signals are confirmed over kContactConfirmS — a single-tick
   * ground-elevation feed step can flip them (doc/core.md, Abschnitt 4.1). */
  double StructureTimerS_ = 0.0, GearUpTimerS_ = 0.0;
};

} // namespace FlightBox
#endif
