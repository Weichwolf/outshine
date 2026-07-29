#include "FBAirFireControl.h"

#include <cmath>

#include "FBGeodesy.h"
#include "FBGunBallistics.h"
#include "FBUnits.h"

namespace FlightBox::Modules {

/* THE LAUNCH ZONE, and every convention in it is the catalogue's: the round is the one on the SELECTED
 * rail (the pilot chooses a station, never a weapon), the target is the LOCKED contact and nothing else,
 * and the three limits come out of weapons/FBLaunchZone unchanged. TimeToActiveS is where the binding
 * lives: FBSeekerHandoverS gives -1 for every semi-active round, and pilot/FBEngagement then measures
 * the support window as the whole predicted time of flight instead of the run to the activation ring. */
void FBAirFireControl::SolveZone(FBState &state, const Fdm::fb_fdm_state &own,
                                 const FBStoreSpec *selected) {
  FBFireControlBlock &b = state.FireControl;
  b.DlzValid = false;
  b.InZone = false;
  b.TargetRangeM = 0.0f; b.ClosureMs = 0.0f;
  b.RaeroM = 0.0f; b.RtrM = 0.0f; b.RminM = 0.0f;
  b.TimeToActiveS = -1.0f; b.TimeToImpactS = -1.0f;
  if (!selected || !selected->Guided) return;
  if (!state.Radar.H.Readable()) return;
  int lock = state.Radar.LockIndex;
  if (lock < 0 || lock >= state.Radar.ContactCount) return;

  const FBRadarContact &c = state.Radar.Contacts[lock];
  const FBBfmBlock &trk = Track_.Block();
  /* Out of the contact's OWN body-referenced angles, so splitting the measured closure into "mine" and
   * "his" uses no second notion of where the target is — the F-16's convention, and it is the one every
   * launch-zone consumer in the tree already assumes. */
  double ownLos = own.speed * std::cos(c.AzDeg * kDeg2Rad) * std::cos(c.ElDeg * kDeg2Rad);
  double tgtSpeed = std::sqrt(trk.VelE * trk.VelE + trk.VelN * trk.VelN + trk.VelU * trk.VelU);
  Weapons::FBLaunchZone z = Weapons::FBSolveLaunchZone(selected->Perf, selected->Seeker, own.speed,
                                                       own.elev, c.RangeM, c.ClosureMs, ownLos,
                                                       tgtSpeed);
  if (!z.Valid) return;
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

/* The ballistics are core/FBGunBallistics' — the same arithmetic the rounds are then flown with. What
 * is FIRE CONTROL here is the three answers the funnel gives: in range, how big in angle, how far off
 * the nose. Two things are per-row rather than per-airframe-family: the funnel's two ranges (this gun's
 * muzzle velocity times the two declared flight times) and the assumed target span (this row's own,
 * because a like-type target is the only assumption a gunsight can make and span is the one sensor
 * input the catalogue can source for every row). */
void FBAirFireControl::SolveGun(FBState &state, const Fdm::fb_fdm_state &own, const FBGunSpec *gun) {
  FBFireControlBlock &b = state.FireControl;
  b.GunValid = false;
  b.GunRangeM = 0.0f; b.GunTofS = 0.0f; b.GunAimErrorDeg = 0.0f;
  b.GunLeadAzDeg = 0.0f; b.GunLeadElDeg = 0.0f; b.GunSpreadM = 0.0f;
  b.GunSpanMr = 0.0f; b.GunFunnelTopMr = 0.0f; b.GunFunnelBottomMr = 0.0f; b.GunTolDeg = 0.0f;
  b.GunInRange = false; b.GunInFunnel = false;
  const FBBfmBlock &trk = Track_.Block();
  if (!gun || !Spec_ || !trk.H.Readable()) return;

  /* fb_fdm_state's velocity is X-Plane local (+x east, +y up, +z south), see fdm/FBFdm.h. */
  FBGunAim aim = FBGunSolveLead(*gun, own.elev, own.vx, -own.vz, own.vy, trk.EastM, trk.NorthM,
                                trk.UpM, trk.VelE, trk.VelN, trk.VelU);
  if (!aim.Valid || aim.RangeM <= 1.0) return;

  /* The gun is boresighted to the nose, so the angle between the two IS the lead still owed. */
  double ne = 0.0, nn = 0.0, nu = 0.0;
  FBBodyLosToEnu(own.roll, own.pitch, own.yaw, 0.0, 0.0, ne, nn, nu);
  double dot = ne * aim.BoreE + nn * aim.BoreN + nu * aim.BoreU;
  if (dot > 1.0) dot = 1.0;
  if (dot < -1.0) dot = -1.0;
  double errDeg = std::acos(dot) * kRad2Deg;

  double azDeg = 0.0, elDeg = 0.0;
  FBEnuToBodyLos(own.roll, own.pitch, own.yaw, aim.BoreE, aim.BoreN, aim.BoreU, azDeg, elDeg);

  double spanM = 2.0 * Spec_->Layout.FrontalExtentM;
  double nearM = gun->MuzzleVelMs * kFunnelNearS;
  double farM = gun->MuzzleVelMs * kFunnelFarS;

  b.GunValid = true;
  b.GunRangeM = (float)aim.RangeM;
  b.GunTofS = (float)aim.TofS;
  b.GunAimErrorDeg = (float)errDeg;
  b.GunLeadAzDeg = (float)azDeg;
  b.GunLeadElDeg = (float)elDeg;
  b.GunSpreadM = (float)aim.SpreadM;
  b.GunSpanMr = (float)(spanM / aim.RangeM * 1000.0);
  b.GunFunnelTopMr = (float)(spanM / nearM * 1000.0);
  b.GunFunnelBottomMr = (float)(spanM / farM * 1000.0);
  b.GunInRange = aim.RangeM >= nearM && aim.RangeM <= farM;
  /* Tolerance DERIVED and not chosen, the F-16's rule verbatim: half the target's angular size plus
   * 1.5 sigma of THIS gun's round pattern is where the density model puts a meaningful number of rounds
   * on it, so the trigger gate and the damage arithmetic cannot drift apart. */
  double tolRad = 0.5 * spanM / aim.RangeM + 1.5 * gun->DispersionSigmaRad;
  b.GunTolDeg = (float)(tolRad * kRad2Deg);
  b.GunInFunnel = b.GunInRange && errDeg <= b.GunTolDeg;
}

void FBAirFireControl::Run(FBState &state, const Fdm::fb_fdm_state &own, const FBStoreSpec *selected,
                           const FBGunSpec *gun, double nowS) {
  /* ---- the target estimate: the LOCKED contact only, aged against the module's own clock ---- */
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

  SolveZone(state, own, selected);
  SolveGun(state, own, gun);
  state.FireControl.H.Publish(state.NowS);
}

} // namespace FlightBox::Modules
