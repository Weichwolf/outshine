/* FlightBox — FBFlightMonitor: the ONE, unbestechliche (incorruptible) K.O. authority for a flight's
 * PHYSICAL state. Fully model-derived, airframe-agnostic by construction: this class knows NOTHING
 * about which module/aircraft is flying — no module type, no per-module declared numbers — only the
 * generic FDM/ground-reaction quantities every JSBSim model exposes (attitude, rates, gear position,
 * gear strut force, ground-reaction contact flags, static weight). The truth comes from the pinned,
 * read-only model itself; the small number of thresholds this file DOES hardcode (see FBFlightMonitor.cpp)
 * are core-owned, generic aviation-engineering bounds, not sourced from or tunable by any one airframe's
 * documentation — the same class watches an F-16 today and whatever module flies next, unchanged.
 * JSBSim itself decides what a K.O. flight state IS — a hard sink, a departure, a structural contact are
 * outputs of the already-simulated model, not something this class invents; FBFlightMonitor only APPLIES
 * its fixed, generic thresholds to that simulated truth. It never writes into the FDM and never sees a
 * module/pilot beyond the read-only sample handed to Tick().
 *
 * Deliberately PHYSICS-ONLY: this class knows nothing about runways, missions, or where a landing was
 * "supposed" to happen — a gentle gear-down touchdown in a field is not a crash to the aircraft, only to
 * a mission's scoring. Whether a ground contact happened somewhere a mission DIDN'T expect (off the
 * assigned runway, outside the takeoff-roll/landing phase) is a MISSION judgement, not a physical one —
 * that verdict belongs to the caller that actually knows the mission (FBMissionRunner.cpp scores it as
 * FAIL, a separate, still incorruptible, Runner-owned check; see its own banner). Two instances, two
 * questions ("did the airframe survive" vs. "did the mission succeed"), never conflated into one.
 *
 * Anti-cheat structure: this class is instantiated and fed EXCLUSIVELY by the app-level driver (the
 * mission runner for gym/native, the WASM frame loop for the browser client) — nothing under systems/ or
 * modules/ includes this header or holds a reference to an instance (grep-verified; see the accompanying
 * audit report). A pilot/module cannot see the monitor, cannot query whether it is about to trip, and
 * cannot influence its verdict — it can only act on the FDM through the normal simulated control surface
 * (fcs cmd-norm channels, gear/flap/speedbrake, throttle), which is what the monitor is watching in the
 * first place. There is no module-facing declaration channel for K.O. thresholds at all — a module
 * cannot claim its way to a more lenient verdict. One instance, one definition of physical "K.O." —
 * every client (fb-gym/gpu_native via FBMissionRunner, the WASM app's own frame loop) runs the SAME
 * class with the SAME thresholds; there is no second, parallel crash test anywhere else in the tree. */
#ifndef FBFLIGHTMONITOR_H
#define FBFLIGHTMONITOR_H
#include <string>

namespace FlightBox {

/* One tick's worth of FDM truth the monitor needs — deliberately NOT fb_fdm_state (core/ does not
 * depend on fdm/, the JSBSim seam; see CLAUDE.md's module-architecture banner): the caller already has
 * an fb_fdm_state each tick and fills this narrow, monitor-specific view from it (FBMissionRunner.cpp
 * and FBAppWasm.cpp both do this identically via FBBuildFlightMonitorSample, FBMissionBoot.h). Every
 * field here is a generic JSBSim/FDM quantity every model exposes, not an airframe-specific one.
 * GroundAslM is the SAME per-tick sample the caller already pulled from its injected
 * FBElevationProvider (the value it also feeds JSBSim's ground floor, fb_jsbsim_set_ground) — passed as
 * a plain double rather than the provider itself so the monitor stays a flat, dependency-free value
 * consumer. */
struct FBFlightMonitorSample {
  double LatDeg = 0.0, LonDeg = 0.0;
  double ElevM = 0.0;        /* m ASL */
  double GroundAslM = 0.0;   /* m ASL, the terrain under the aircraft this tick */
  double RollDeg = 0.0, PitchDeg = 0.0;
  double PDegS = 0.0, QDegS = 0.0, RDegS = 0.0;   /* body rates, deg/s */
  double VsMs = 0.0;         /* vertical speed, + = climb */
  double TasMs = 0.0;        /* true airspeed, m/s — a generic kinematic quantity (velocity magnitude),
                              * used ONLY to derive the flight-path angle below, never as an aero
                              * coefficient/AoA-limit surrogate */
  double GearPosNorm = 0.0;  /* 0=up..1=down, the model's own lagged gear position */
  double GearForceLbs = 0.0; /* peak wheeled-gear strut compression force this tick (fb_jsbsim_get_max_gear_force_lbs) */
  double WeightLbs = 0.0;    /* the model's own current static weight (fb_jsbsim_get_weight_lbs) */
  bool   AnyWow = false;             /* any BOGEY (wheeled) contact compressed */
  bool   StructureContact = false;   /* any non-wheeled ground-reaction contact point compressed (a
                                      * declared STRUCTURE point — airframe geometry, e.g. wingtip/tail/
                                      * intake/radome on today's F-16 model; whatever an aircraft.xml
                                      * declares beyond its wheeled gear) */
};

enum class FBKoReason { None, StructureContact, CfitPenetration, GearUpContact, HardLanding,
                        AttitudeContact, Loc };
const char *FBKoReasonStr(FBKoReason r);

class FBFlightMonitor {
public:
  FBFlightMonitor() = default;

  /* Feeds one tick's sample (simTimeS = cumulative sim seconds, for the LOC sustain timers). Returns
   * true on the ONE tick a K.O. condition first trips — Reason()/Detail() are valid from then on. Once
   * tripped the monitor LATCHES: every later call is a no-op returning false, so a caller's
   * `if (monitor.Tick(...))` fires exactly once no matter how many more ticks run before the caller
   * reacts (e.g. before the engine-cutoff/mission-end actually lands). Emits its own FBLog::Error event
   * with the measured values the instant it trips — the caller does not need to re-derive them. */
  bool Tick(const FBFlightMonitorSample &s, double simTimeS);

  bool               Tripped() const { return Reason_ != FBKoReason::None; }
  FBKoReason         Reason() const { return Reason_; }
  const std::string &Detail() const { return Detail_; }

private:
  bool Trip(FBKoReason reason, const std::string &detail, const FBFlightMonitorSample &s);

  FBKoReason  Reason_ = FBKoReason::None;
  std::string Detail_;

  double LastSimTimeS_ = -1.0;     /* -1 = no previous tick yet (first Tick computes dt=0) */
  double LocTimerS_ = 0.0;         /* consecutive seconds the departure (high-rate) condition has held */
  double StallTimerS_ = 0.0;       /* consecutive seconds the stall/mush condition has held */
  /* Ground-contact confirmation timers (see kContactConfirmS in the .cpp): StructureContact/GearUpContact
   * need the SAME (binary, WOW-driven) condition true for a short run of consecutive ticks, not just
   * one, before they trip — guards against a single-tick ground-elevation FEED discontinuity (the caller
   * pushes a fresh terrain sample into JSBSim once per tick, not a continuous surface; a live DEM's
   * real, if small, along-track grade can step the reference height enough in one tick to read as a
   * transient contact). HardLanding/AttitudeContact are smoothly-varying physical quantities and trip
   * on a single tick — see the .cpp's kContactConfirmS banner for why. */
  double StructureTimerS_ = 0.0, GearUpTimerS_ = 0.0;
};

} // namespace FlightBox
#endif
