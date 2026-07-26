/* Threshold sourcing: every constant below is a generic, core-owned aviation-engineering bound — none
 * are sourced from or tunable by any one airframe's documentation, and none are module-declared data
 * (there is no such channel; see FBFlightMonitor.h's banner). Numbers marked "regression" are
 * pre-existing FBMissionRunner constants moved here verbatim. */
#include "FBFlightMonitor.h"
#include "FBLog.h"
#include <cmath>

namespace FlightBox {

namespace {
/* Terrain penetration: aglM below this means the gear/belly is inside the ground mesh, not a soft
 * touchdown. Regression: FBMissionRunner's pre-existing `aglM < -3.0` gate, unchanged. */
constexpr double kPenetrationMarginM = -3.0;

/* Ground contact with the gear NOT extended: JSBSim's own gear/gear-pos-norm (0=up..1=down, the lagged
 * kinematic transit every retractable-gear model exposes). Any weight-on-wheels contact while
 * substantially retracted is a belly/gear-up touchdown regardless of where it happens — a purely
 * physical fact, and 0.5 is simply the generic midpoint of that normalized [0,1] range. */
constexpr double kGearDownThreshold = 0.5;

/* Hard landing, from the model's OWN gear physics rather than a declared sink-rate number: JSBSim's
 * strut spring/damper reaction (fb_jsbsim_get_max_gear_force_lbs) IS the aircraft's real, physically
 * simulated touchdown load — comparing it against the model's own static weight
 * (fb_jsbsim_get_weight_lbs) needs exactly one core-owned, airframe-agnostic bound: a load factor no
 * landing gear of this CLASS is designed past. Generic gear-design practice sizes a limit/hard landing
 * around a low-single-digit "g" reaction at the CG; concentrated on a SINGLE strut (this check takes the
 * peak, not a sum) a properly executed landing routinely spikes past 1x total weight on one main gear
 * without being remotely hard. kHardLandingForceFactor=3.0 (peak single-strut force > 3x the aircraft's
 * OWN total weight) sits comfortably above that normal-landing transient while staying far short of a
 * structural-failure-grade impact — conservative in the direction of NOT tripping a good landing.
 * Verified empirically: the reference takeoff run's static/rolling gear force never exceeds a small
 * fraction of 1x weight per strut (see the task report's measurement); a deliberately excessive
 * touchdown profile clears this bound by a wide margin. */
constexpr double kHardLandingForceFactor = 3.0;

/* Extreme attitude at ground contact — a geometry risk (tailstrike/structural strike) generic to any
 * airframe with finite ground clearance; complements (does not replace) the model-driven structural-
 * contact check above for an airframe whose aircraft.xml declares no STRUCTURE points. 15 deg is a
 * conservative, class-generic bound for both pitch and roll at wheels-down — not sourced from or tuned
 * to one airframe's numbers. */
constexpr double kMaxContactPitchDeg = 15.0;
constexpr double kMaxContactRollDeg = 15.0;

/* LOC/departure — purely behavioural, no aerodynamic-model-specific quantity (no AoA number, which
 * means something different per airframe class and isn't even meaningful for some): a sustained,
 * multi-axis body-rate magnitude this large, held for multiple seconds while airborne, is a spin/tumble
 * for any fixed-wing airframe, not a coordinated maneuver — generic engineering judgement, not a per-
 * airframe figure. */
constexpr double kLocRateDegS = 60.0;
constexpr double kLocSustainS = 3.0;

/* Stall/mush/deep-stall signature — purely KINEMATIC, deliberately not the model's own aero/alpha-deg
 * output (no aerodynamic-model quantity, no per-airframe stall-AoA number; the vanilla model exposes no
 * declared alpha limit to derive one from anyway): from attitude + the velocity vector alone, compute
 * where the nose POINTS (PitchDeg) vs. where the aircraft actually GOES (the flight-path angle, from
 * VsMs and TasMs) — a large, sustained mismatch between the two is flight that isn't attached/conventional
 * for ANY fixed-wing airframe, independent of its particular stall characteristics. A fast, shallow,
 * intentional dive (near-level attitude, high sink rate from sheer speed) keeps this mismatch small —
 * verified empirically against a diving test profile (see the task report) — while a stalled/mushing/
 * deep-stalled aircraft (nose not aligned with its actual path) does not. kMinTasMs excludes the
 * near-zero-airspeed settle transient (regression: FBMissionRunner's old `cas > 15.0` gate, m/s). */
constexpr double kMinTasMs = 15.0;
constexpr double kNoseFlightpathMismatchDeg = 30.0;
constexpr double kStallSustainS = 4.0;

/* Ground-contact confirmation window — applied ONLY to the two BINARY, threshold-crossing signals
 * (StructureContact/GearUpContact's AnyWow component: JSBSim's WOW flag, a hard true/false on which
 * side of the ground surface a contact point sits) that a single-tick terrain-feed step can flip for
 * exactly one sample: the caller pushes a FRESH terrain height into the FDM once per tick (a discrete
 * update, not a continuously-followed surface), and on a live DEM with real along-track grade that step
 * can momentarily read as contact even though the ground-reaction model settles a tick later. Confirmed
 * empirically (see the task report): a live-data run without this confirmation window false-tripped
 * STRUCTURE_CONTACT on an ordinary takeoff roll. Deliberately NOT applied to HardLanding/AttitudeContact
 * — those are smoothly-varying PHYSICAL quantities (gear strut force, attitude) computed from the
 * aircraft's own continuous dynamics, not a discrete height comparison, and (measured) a genuine hard-
 * landing force spike itself decays within a similarly short window — sustaining THOSE would make the
 * check miss the very impacts it exists to catch. 0.2 s is a couple of ticks at any of this codebase's
 * tick rates (10 Hz mission decision tick, 60+ Hz WASM frame loop) — long enough to reject a one-sample
 * artifact, far too short to matter for a genuine, sustained structural/gear-up contact (which stays
 * true for seconds, not a fraction of one). */
constexpr double kContactConfirmS = 0.2;
} // namespace

const char *FBKoReasonStr(FBKoReason r) {
  switch (r) {
    case FBKoReason::None: return "NONE";
    case FBKoReason::StructureContact: return "STRUCTURE_CONTACT";
    case FBKoReason::CfitPenetration: return "CFIT";
    case FBKoReason::GearUpContact: return "GEAR_UP_CONTACT";
    case FBKoReason::HardLanding: return "HARD_LANDING";
    case FBKoReason::AttitudeContact: return "ATTITUDE_CONTACT";
    case FBKoReason::Loc: return "LOC";
  }
  return "?";
}

bool FBFlightMonitor::Trip(FBKoReason reason, const std::string &detail, const FBFlightMonitorSample &s) {
  Reason_ = reason;
  Detail_ = detail;
  double aglM = s.ElevM - s.GroundAslM;
  FBLog::Error("monitor", "KO", {{"reason", FBKoReasonStr(reason)}, {"detail", detail},
      {"lat", s.LatDeg}, {"lon", s.LonDeg}, {"aglM", aglM}, {"vsMs", s.VsMs},
      {"roll", s.RollDeg}, {"pitch", s.PitchDeg}, {"p", s.PDegS}, {"q", s.QDegS}, {"r", s.RDegS},
      {"gearPos", s.GearPosNorm}, {"gearForceLbs", s.GearForceLbs}, {"weightLbs", s.WeightLbs}});
  return true;
}

bool FBFlightMonitor::Tick(const FBFlightMonitorSample &s, double simTimeS) {
  if (Tripped()) return false;   /* latched: verdict already reached, never re-evaluate/re-fire */

  double dt = LastSimTimeS_ < 0.0 ? 0.0 : simTimeS - LastSimTimeS_;
  LastSimTimeS_ = simTimeS;

  /* 1. Terrain penetration (CFIT) — checked before the contact-confirmation group below since its own
   * -3 m margin already absorbs a single-tick terrain-feed step; a genuine hole-through-the-mesh CFIT
   * should not wait a confirmation window. */
  double aglM = s.ElevM - s.GroundAslM;
  if (aglM < kPenetrationMarginM)
    return Trip(FBKoReason::CfitPenetration, "ground penetration", s);

  /* 2, 3: the two BINARY/threshold-crossing checks — confirmation-gated (see kContactConfirmS's
   * banner), the condition must hold for several CONSECUTIVE ticks, not just one. */
  bool structureNow = s.StructureContact;
  StructureTimerS_ = structureNow ? StructureTimerS_ + dt : 0.0;
  if (StructureTimerS_ >= kContactConfirmS)
    return Trip(FBKoReason::StructureContact, "structure contact", s);

  bool gearUpNow = s.AnyWow && s.GearPosNorm < kGearDownThreshold;
  GearUpTimerS_ = gearUpNow ? GearUpTimerS_ + dt : 0.0;
  if (GearUpTimerS_ >= kContactConfirmS)
    return Trip(FBKoReason::GearUpContact, "ground contact with gear not extended", s);

  /* 4. Hard landing — the model's own gear strut force vs. its own static weight (see the constant's
   * banner); checked every tick a bogey is compressed, not just the touchdown edge, so it catches the
   * actual force peak wherever in the compression cycle it occurs. Single-tick trigger deliberately (see
   * kContactConfirmS's banner: a genuine peak here is itself brief). */
  if (s.AnyWow && s.WeightLbs > 0.0 && s.GearForceLbs > kHardLandingForceFactor * s.WeightLbs)
    return Trip(FBKoReason::HardLanding, "hard landing: gear strut force exceeded the design load factor", s);

  /* 5. Extreme attitude with weight on any contact (bogey or structure) — tailstrike/structural-strike
   * geometry risk even at a benign gear load. Single-tick trigger (continuous quantity, see above). */
  bool anyContact = s.AnyWow || s.StructureContact;
  if (anyContact && (std::fabs(s.RollDeg) > kMaxContactRollDeg || s.PitchDeg > kMaxContactPitchDeg))
    return Trip(FBKoReason::AttitudeContact, "extreme attitude at ground contact", s);

  /* 6. LOC/departure — airborne only, purely behavioural (no AoA/model-specific quantity, see the
   * constants' banners above). */
  bool departing = !s.AnyWow &&
                   std::sqrt(s.PDegS * s.PDegS + s.QDegS * s.QDegS + s.RDegS * s.RDegS) > kLocRateDegS;
  LocTimerS_ = departing ? LocTimerS_ + dt : 0.0;
  if (LocTimerS_ >= kLocSustainS)
    return Trip(FBKoReason::Loc, "departure: sustained high multi-axis body rate", s);

  bool haveTas = s.TasMs > kMinTasMs;
  bool stalling = false;
  if (haveTas && !s.AnyWow) {
    double horiz = std::sqrt(std::fmax(s.TasMs * s.TasMs - s.VsMs * s.VsMs, 0.0));
    double fpaDeg = std::atan2(s.VsMs, horiz) * (180.0 / M_PI);
    stalling = std::fabs(s.PitchDeg - fpaDeg) > kNoseFlightpathMismatchDeg;
  }
  StallTimerS_ = stalling ? StallTimerS_ + dt : 0.0;
  if (StallTimerS_ >= kStallSustainS)
    return Trip(FBKoReason::Loc, "stall/mush: nose attitude sustained far off the actual flight path", s);

  return false;
}

} // namespace FlightBox
