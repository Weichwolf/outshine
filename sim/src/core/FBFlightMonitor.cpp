/* Every constant below is a generic, core-owned aviation-engineering bound — none sourced from or
 * tunable by an airframe's documentation. Derivations: doc/core.md, Abschnitt 4.1. */
#include "FBFlightMonitor.h"
#include "FBLog.h"
#include <cmath>

namespace FlightBox {

namespace {
/* Gear/belly inside the ground mesh, not a soft touchdown. */
constexpr double kPenetrationMarginM = -3.0;

/* The generic midpoint of JSBSim's own normalized gear/gear-pos-norm range. */
constexpr double kGearDownThreshold = 0.5;

/* Peak SINGLE-strut force against the model's own static weight: above any normal-landing transient,
 * far short of a structural-failure impact — conservative towards NOT tripping a good landing. */
constexpr double kHardLandingForceFactor = 3.0;

/* Tailstrike geometry risk — complements (never replaces) the model-driven structure-contact check for
 * an airframe whose aircraft.xml declares no STRUCTURE points. */
constexpr double kMaxContactPitchDeg = 15.0;
constexpr double kMaxContactRollDeg = 15.0;

/* Purely BEHAVIOURAL, deliberately no AoA number: a sustained multi-axis rate of this magnitude is a
 * spin/tumble for any fixed-wing airframe, not a coordinated manoeuvre. */
constexpr double kLocRateDegS = 60.0;
constexpr double kLocSustainS = 3.0;

/* Stall/mush signature, purely KINEMATIC (deliberately not the model's alpha-deg): where the nose
 * POINTS against where the aircraft GOES. A fast shallow dive keeps this mismatch small, a mushing
 * aircraft does not. kMinTasMs excludes the near-zero-airspeed settle transient. */
constexpr double kMinTasMs = 15.0;
constexpr double kNoseFlightpathMismatchDeg = 30.0;
constexpr double kStallSustainS = 4.0;

/* Applied ONLY to the two BINARY signals a single-tick terrain-feed step can flip for one sample.
 * Deliberately NOT to HardLanding/AttitudeContact: those are smoothly-varying physical quantities, and a
 * genuine force peak itself decays inside such a window — sustaining them would miss the very impacts
 * the checks exist for. */
constexpr double kContactConfirmS = 0.2;

/* Checked on the RAW inputs, not on aglM or the derived FPA, so divergence is caught at its entry
 * point before it propagates. */
const char *FirstNonFinite(const FBFlightMonitorSample &s) {
  const struct { const char *Name; double Value; } fields[] = {
      {"lat", s.LatDeg}, {"lon", s.LonDeg}, {"elevM", s.ElevM}, {"groundAslM", s.GroundAslM},
      {"roll", s.RollDeg}, {"pitch", s.PitchDeg},
      {"p", s.PDegS}, {"q", s.QDegS}, {"r", s.RDegS},
      {"vsMs", s.VsMs}, {"tasMs", s.TasMs},
      {"gearPos", s.GearPosNorm}, {"gearForceLbs", s.GearForceLbs}, {"weightLbs", s.WeightLbs}};
  for (const auto &f : fields)
    if (!std::isfinite(f.Value)) return f.Name;
  return nullptr;
}
} // namespace

const char *FBKoReasonStr(FBKoReason r) {
  switch (r) {
    case FBKoReason::None: return "NONE";
    case FBKoReason::NumericalDivergence: return "NUMERICAL_DIVERGENCE";
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

  /* 0. Numerical divergence — BEFORE everything else (NaN loses every comparison); single-tick. */
  if (s.FdmFault)
    return Trip(FBKoReason::NumericalDivergence, "fdm integrator faulted", s);
  if (const char *bad = FirstNonFinite(s))
    return Trip(FBKoReason::NumericalDivergence, std::string("non-finite state: ") + bad, s);

  /* 1. CFIT — before the confirmation group: the -3 m margin already absorbs a terrain-feed step. */
  double aglM = s.ElevM - s.GroundAslM;
  if (aglM < kPenetrationMarginM)
    return Trip(FBKoReason::CfitPenetration, "ground penetration", s);

  /* 2, 3: the two BINARY checks — confirmation-gated over consecutive ticks. */
  bool structureNow = s.StructureContact;
  StructureTimerS_ = structureNow ? StructureTimerS_ + dt : 0.0;
  if (StructureTimerS_ >= kContactConfirmS)
    return Trip(FBKoReason::StructureContact, "structure contact", s);

  bool gearUpNow = s.AnyWow && s.GearPosNorm < kGearDownThreshold;
  GearUpTimerS_ = gearUpNow ? GearUpTimerS_ + dt : 0.0;
  if (GearUpTimerS_ >= kContactConfirmS)
    return Trip(FBKoReason::GearUpContact, "ground contact with gear not extended", s);

  /* 4. Hard landing — every tick a bogey is compressed, not just the touchdown edge, so the actual
   * force peak is caught wherever in the compression cycle it falls. */
  if (s.AnyWow && s.WeightLbs > 0.0 && s.GearForceLbs > kHardLandingForceFactor * s.WeightLbs)
    return Trip(FBKoReason::HardLanding, "hard landing: gear strut force exceeded the design load factor", s);

  /* 5. Extreme attitude on any contact — a geometry risk even at a benign gear load. */
  bool anyContact = s.AnyWow || s.StructureContact;
  if (anyContact && (std::fabs(s.RollDeg) > kMaxContactRollDeg || s.PitchDeg > kMaxContactPitchDeg))
    return Trip(FBKoReason::AttitudeContact, "extreme attitude at ground contact", s);

  /* 6. LOC/departure — airborne only. */
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
