#include "FBF16FireControl.h"
#include "FBAtmosphere.h"
#include "FBBallistics.h"
#include "FBGeodesy.h"
#include "FBGunBallistics.h"
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

  /* ONE density for the whole integration: the round's altitude band over an engagement is small next
   * to its range, and a stored fire-control model is not a trajectory simulator. */
  const double rho = FBIsaDensity(altM);
  const double burnS = perf.BoostS + perf.SustainS;
  const double dt = 0.25;             /* coarse on purpose: a stored table's worth of fidelity */
  const double maxS = 240.0;

  /* The radar measures TOTAL closure; the launcher's own part is what the round inherits at launch. */
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

/* Everything ballistic is core/FBGunBallistics' — the same arithmetic the rounds are then flown with,
 * which for an unguided projectile is what a fire-control computer genuinely solves. What is FIRE
 * CONTROL is the three answers the funnel gives: in range, how big in angle, how far off the nose. */
void FBF16FireControl::SolveGun(FBState &state, const fb_fdm_state &own, const FBGunSpec *gun,
                                const FBBfmBlock &trk) {
  FBFireControlBlock &b = state.FireControl;
  b.GunValid = false;
  b.GunRangeM = 0.0f; b.GunTofS = 0.0f; b.GunAimErrorDeg = 0.0f;
  b.GunLeadAzDeg = 0.0f; b.GunLeadElDeg = 0.0f; b.GunSpreadM = 0.0f;
  b.GunSpanMr = 0.0f; b.GunFunnelTopMr = 0.0f; b.GunFunnelBottomMr = 0.0f; b.GunTolDeg = 0.0f;
  b.GunInRange = false; b.GunInFunnel = false;
  if (!gun || !trk.H.Readable()) return;

  /* fb_fdm_state's velocity is X-Plane local (+x east, +y up, +z south), see fdm/FBFdm.h. */
  FBGunAim aim = FBGunSolveLead(*gun, own.elev, own.vx, -own.vz, own.vy, trk.EastM, trk.NorthM,
                                trk.UpM, trk.VelE, trk.VelN, trk.VelU);
  if (!aim.Valid) return;

  /* The gun is boresighted to the nose, so the angle between the two IS the lead still owed. */
  double ne = 0.0, nn = 0.0, nu = 0.0;
  FBBodyLosToEnu(own.roll, own.pitch, own.yaw, 0.0, 0.0, ne, nn, nu);
  double dot = ne * aim.BoreE + nn * aim.BoreN + nu * aim.BoreU;
  if (dot > 1.0) dot = 1.0;
  if (dot < -1.0) dot = -1.0;
  double errDeg = std::acos(dot) * kRad2Deg;

  double azDeg = 0.0, elDeg = 0.0;
  FBEnuToBodyLos(own.roll, own.pitch, own.yaw, aim.BoreE, aim.BoreN, aim.BoreU, azDeg, elDeg);

  b.GunValid = true;
  b.GunRangeM = (float)aim.RangeM;
  b.GunTofS = (float)aim.TofS;
  b.GunAimErrorDeg = (float)errDeg;
  b.GunLeadAzDeg = (float)azDeg;
  b.GunLeadElDeg = (float)elDeg;
  b.GunSpreadM = (float)aim.SpreadM;
  b.GunSpanMr = (float)(kTargetSpanM / aim.RangeM * 1000.0);
  b.GunFunnelTopMr = (float)(kTargetSpanM / kFunnelMinRangeM * 1000.0);
  b.GunFunnelBottomMr = (float)(kTargetSpanM / kFunnelMaxRangeM * 1000.0);
  b.GunInRange = aim.RangeM >= kFunnelMinRangeM && aim.RangeM <= kFunnelMaxRangeM;
  /* Tolerance DERIVED, not chosen: half the target's angular size plus 1.5 sigma of the round pattern
   * is exactly where the density model puts a meaningful number of rounds on it, so the trigger gate
   * and the damage arithmetic cannot drift apart. */
  double tolRad = 0.5 * kTargetSpanM / aim.RangeM + 1.5 * gun->DispersionSigmaRad;
  b.GunTolDeg = (float)(tolRad * kRad2Deg);
  b.GunInFunnel = b.GunInRange && errDeg <= b.GunTolDeg;
}

/* One integration, both delivery modes, so the pipper and the release countdown can never disagree.
 * The three inputs are jet CONVENTIONS rather than arithmetic (doc/flightbox/modules-f16.md §8.4):
 * release state from the CG, aim point = the active steerpoint, impact plane = its own elevation. A
 * guided round or an empty station leaves the solution invalid, which is not an impact point of zero. */
void FBF16FireControl::SolveGroundAttack(FBState &state, const fb_fdm_state &own,
                                         const FBStoreSpec *selected) {
  FBFireControlBlock &b = state.FireControl;
  b.AgValid = false;
  b.AgImpactLatDeg = 0.0; b.AgImpactLonDeg = 0.0;
  b.AgImpactElevM = 0.0f; b.AgTofS = 0.0f; b.AgRangeM = 0.0f;
  b.AgAlongErrM = 0.0f; b.AgCrossErrM = 0.0f; b.AgMissM = 0.0f;
  b.AgTimeToReleaseS = 0.0f; b.AgArmMarginS = 0.0f;
  b.AgInRange = false;
  Solution_ = FBReleaseSolution{};
  if (!selected || selected->Guided) return;

  FBReleaseState rel;
  rel.LatDeg = own.lat; rel.LonDeg = own.lon; rel.AltM = own.elev;
  /* fb_fdm_state's velocity is X-Plane local (+x east, +y up, +z south), see fdm/FBFdm.h. */
  rel.VelE = own.vx; rel.VelN = -own.vz; rel.VelU = own.vy;
  double planeM = state.Nav.SteerElevFt * kFtToM;
  FBImpactPrediction pred = FBSolveImpactPoint(selected->Perf, rel, planeM);
  if (!pred.Valid) return;

  /* Reconstructed from the nav block's own bearing/distance rather than a second copy of the
   * steerpoint: this box reads the bus, like every other consumer. */
  double distM = state.Nav.SteerDistNm * kNmToM;
  double brg = state.Nav.SteerBearingDeg * kDeg2Rad;
  double coslat = std::cos(own.lat * kDeg2Rad);
  double aimLat = own.lat + distM * std::cos(brg) / kMPerDeg;
  double aimLon = own.lon + (coslat > 1e-6 ? distM * std::sin(brg) / (kMPerDeg * coslat) : 0.0);
  /* The ground TRACK, not the heading: the release cue lives on the axis the aircraft is actually
   * moving along, and the round inherits. With no air data there is an impact point but no release
   * cue — a real distinction, not a missing number. */
  FBAimSolution aim{};
  if (state.AirData.H.Readable())
    aim = FBSolveAim(pred, own.lat, own.lon, aimLat, aimLon, state.AirData.TrackDeg, own.gs);

  b.AgValid = true;
  b.AgImpactLatDeg = pred.LatDeg;
  b.AgImpactLonDeg = pred.LonDeg;
  b.AgImpactElevM = (float)pred.ElevM;
  b.AgTofS = (float)pred.TofS;
  b.AgRangeM = (float)pred.RangeM;
  b.AgArmMarginS = (float)pred.ArmMarginS;
  if (aim.Valid) {
    b.AgAlongErrM = (float)aim.AlongErrM;
    b.AgCrossErrM = (float)aim.CrossErrM;
    b.AgMissM = (float)aim.MissM;
    b.AgTimeToReleaseS = (float)aim.TimeToGoS;
    b.AgInRange = aim.AlongErrM >= 0.0 && pred.ArmMarginS > 0.0;
  }

  Solution_.Valid = true;
  Solution_.Mode = Mode_;
  Solution_.ImpactLatDeg = pred.LatDeg;
  Solution_.ImpactLonDeg = pred.LonDeg;
  Solution_.ImpactElevM = pred.ElevM;
  Solution_.TofS = pred.TofS;
  Solution_.AimLatDeg = aimLat;
  Solution_.AimLonDeg = aimLon;
  Solution_.AimMissM = aim.Valid ? aim.MissM : 0.0;
  Solution_.ArmMarginS = pred.ArmMarginS;
  Solution_.StampS = state.NowS;
}

/* Slant = sqrt(horizontal^2 + altDiff^2), the 'B' (baro/steerpoint-elevation) method. */
void FBF16FireControl::Run(FBState &state, const fb_fdm_state &own, const FBStoreSpec *selected,
                           const FBGunSpec *gun, double nowS, double dt) {
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
    /* Out of the contact's own body-referenced angles, so splitting the measured closure into "mine"
     * and "his" uses no second notion of where the target is. */
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

  /* ---- the gun solution for the same target estimate ---- */
  SolveGun(state, own, gun, trk);
  /* ---- and the air-to-ground one, against the ground rather than against a contact ---- */
  SolveGroundAttack(state, own, selected);
  b.H.Publish(state.NowS);
}

} // namespace FlightBox
