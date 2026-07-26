#include "FBF16FireControl.h"
#include "FBAtmosphere.h"
#include "FBGeodesy.h"
#include "FBUnits.h"
#include <cmath>

namespace FlightBox {

namespace {
constexpr float kMToFtF = 3.280839895f;
constexpr float kFtToNmF = 1.0f / 6076.12f;

} // namespace

FBLaunchZone FBF16FireControl::SolveLaunchZone(const FBWeaponPerf &perf, double ownSpeedMs, double altM,
                                               double rangeM, double closureMs, double ownLosMs,
                                               double tgtSpeedMs) {
  FBLaunchZone z;
  if (perf.LaunchMassKg <= 0.0 || perf.RefAreaM2 <= 0.0) return z;

  /* ONE density for the whole integration (the launch altitude's, core/FBAtmosphere.h): the round's
   * altitude band over an engagement is small next to its range, and a fire-control computer's stored
   * model is not a trajectory simulator. */
  const double rho = FBIsaDensity(altM);
  const double burnS = perf.BoostS + perf.SustainS;
  const double dt = 0.25;             /* coarse on purpose: a stored table's worth of fidelity */
  const double maxS = 240.0;

  /* The target's own closing component along the line of sight: the radar measures the TOTAL closure,
   * of which the launcher's own motion is the part the round inherits at launch. */
  const double tgtLosMs = closureMs - ownLosMs;

  double v = ownSpeedMs, s = 0.0, t = 0.0, r = rangeM;
  for (; t < maxS; t += dt) {
    double thrust = t < perf.BoostS ? perf.BoostThrustN : (t < burnS ? perf.SustainThrustN : 0.0);
    double frac = burnS > 0.0 ? t / burnS : 1.0;
    double mass = t >= burnS ? perf.BurnoutMassKg
                             : perf.LaunchMassKg - (perf.LaunchMassKg - perf.BurnoutMassKg) * frac;
    double drag = 0.5 * rho * v * v * perf.DragCoefA * perf.RefAreaM2;
    v += (thrust - drag) / mass * dt;
    if (v < 1.0) v = 1.0;
    s += v * dt;
    r -= (v + tgtLosMs) * dt;
    if (z.TimeToActiveS < 0.0 && r <= perf.ActivationRangeM) z.TimeToActiveS = t + dt;
    if (z.TimeToImpactS < 0.0 && r <= 0.0) z.TimeToImpactS = t + dt;
    /* Dead: no closure left to run an intercept with. Only tested after the boost, since the round
     * starts below MinSpeed whenever the launcher is subsonic. */
    if (t > perf.BoostS && v < perf.MinSpeedMs) { t += dt; break; }
  }

  z.Valid = true;
  z.RaeroM = s + tgtLosMs * t;
  z.RtrM = s - tgtSpeedMs * t;
  if (z.RtrM < 0.0) z.RtrM = 0.0;
  z.RminM = (closureMs > 0.0 ? closureMs : 0.0) * perf.ArmingS + kMinTurnM;
  return z;
}

/* Slant = sqrt(horizontal^2 + altDiff^2), the 'B' (baro/steerpoint-elevation) method — then the target
 * track, then the launch zone the SMS's interlock and a future intercept AI read. */
void FBF16FireControl::Run(FBState &state, const fb_fdm_state &own, const FBStoreSpec *selected,
                           double nowS, double dt) {
  (void)dt;
  if (!state.Nav.H.Readable() || !state.Platform.H.Readable()) {
    state.FireControl.H.Invalidate();
    return;
  }
  float horizFt = state.Nav.SteerDistNm * 6076.12f;
  float altDiffFt = state.Platform.AltM * kMToFtF - state.Nav.SteerElevFt;
  state.FireControl.SteerSlantNm = std::sqrt(horizFt * horizFt + altDiffFt * altDiffFt) * kFtToNmF;
  state.FireControl.RangeProvider = 'B';

  /* ---- the target track: the LOCKED contact only, aged against the module's own clock ---- */
  Track_.Update(state, own, nowS);
  const FBBfmBlock &trk = Track_.Block();
  Target_ = FBWeaponTargetState{};
  if (trk.H.Readable() && trk.Locked) {
    double coslat = std::cos(own.lat * kDeg2Rad);
    Target_.Valid = true;
    Target_.LatDeg = own.lat + trk.NorthM / kMPerDeg;
    Target_.LonDeg = own.lon + (coslat > 1e-6 ? trk.EastM / (kMPerDeg * coslat) : 0.0);
    Target_.AltM = own.elev + trk.UpM;
    Target_.VelE = trk.VelE; Target_.VelN = trk.VelN; Target_.VelU = trk.VelU;
    Target_.StampS = trk.H.StampS;
  }

  /* ---- the launch zone for the selected round ---- */
  FBFireControlBlock &b = state.FireControl;
  b.DlzValid = false;
  b.InZone = false;
  b.TargetRangeM = 0.0f; b.ClosureMs = 0.0f;
  b.RaeroM = 0.0f; b.RtrM = 0.0f; b.RminM = 0.0f;
  b.TimeToActiveS = -1.0f; b.TimeToImpactS = -1.0f;
  int lock = state.Radar.H.Readable() ? state.Radar.LockIndex : -1;
  if (selected && selected->Guided && lock >= 0 && lock < state.Radar.ContactCount) {
    const FBRadarContact &c = state.Radar.Contacts[lock];
    /* The launcher's own speed along the line of sight, out of the contact's body-referenced angles —
     * the same geometry the antenna reports, so splitting the measured closure into "mine" and "his"
     * uses no second notion of where the target is. */
    double ownLos = own.speed * std::cos(c.AzDeg * kDeg2Rad) * std::cos(c.ElDeg * kDeg2Rad);
    double tgtSpeed = std::sqrt(trk.VelE * trk.VelE + trk.VelN * trk.VelN + trk.VelU * trk.VelU);
    FBLaunchZone z = SolveLaunchZone(selected->Perf, own.speed, own.elev, c.RangeM, c.ClosureMs, ownLos,
                                     tgtSpeed);
    if (z.Valid) {
      b.DlzValid = true;
      b.TargetRangeM = c.RangeM;
      b.ClosureMs = c.ClosureMs;
      b.RaeroM = (float)z.RaeroM;
      b.RtrM = (float)z.RtrM;
      b.RminM = (float)z.RminM;
      b.TimeToActiveS = (float)z.TimeToActiveS;
      b.TimeToImpactS = (float)z.TimeToImpactS;
      b.InZone = c.RangeM >= z.RminM && c.RangeM <= z.RaeroM;
    }
  }
  b.H.Publish(state.NowS);
}

} // namespace FlightBox
