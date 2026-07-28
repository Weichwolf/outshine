#include "FBMig29FireControl.h"
#include "FBGeodesy.h"
#include "FBGunBallistics.h"
#include "FBUnits.h"
#include <cmath>

namespace FlightBox::Modules {

namespace {
constexpr float kMToFtF = 3.280839895f;
constexpr float kFtToNmF = 1.0f / 6076.12f;
} // namespace

/* The shared ballistics, exactly as the F-16's box uses them — an unguided projectile flies the arc a
 * fire-control computer solves, whichever aircraft fired it. What is THIS jet's is the range window
 * (§4.2's measured 790-200 m effective band) and the target span the sight scales with. */
void FBMig29FireControl::SolveGun(FBState &state, const Fdm::fb_fdm_state &own, const FBGunSpec *gun,
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
  b.GunFunnelTopMr = (float)(kTargetSpanM / kGunMinRangeM * 1000.0);
  b.GunFunnelBottomMr = (float)(kTargetSpanM / kGunMaxRangeM * 1000.0);
  b.GunInRange = aim.RangeM >= kGunMinRangeM && aim.RangeM <= kGunMaxRangeM;
  /* DERIVED, not chosen, and the same derivation the F-16's box makes: half the target's angular size
   * plus 1.5 sigma of this gun's own pattern is where the density model puts a meaningful number of
   * rounds on it. With a 6 mrad sigma against the M61A1's 2.2 that window is visibly wider — a
   * consequence of the gun, not a second decision. */
  double tolRad = 0.5 * kTargetSpanM / aim.RangeM + 1.5 * gun->DispersionSigmaRad;
  b.GunTolDeg = (float)(tolRad * kRad2Deg);
  b.GunInFunnel = b.GunInRange && errDeg <= b.GunTolDeg;
}

void FBMig29FireControl::Run(FBState &state, const Fdm::fb_fdm_state &own, const FBStoreSpec *selected,
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

  /* ---- Dr max1 / Dr max2 / Dr min, which ARE Raero / Rtr / Rmin (§3.5) ---- */
  FBFireControlBlock &b = state.FireControl;
  b.DlzValid = false;
  b.InZone = false;
  b.TargetRangeM = 0.0f; b.ClosureMs = 0.0f;
  b.RaeroM = 0.0f; b.RtrM = 0.0f; b.RminM = 0.0f;
  b.TimeToActiveS = -1.0f; b.TimeToImpactS = -1.0f;
  int lock = state.Radar.H.Readable() ? state.Radar.LockIndex : -1;
  if (selected && selected->Guided && lock >= 0 && lock < state.Radar.ContactCount) {
    const FBRadarContact &c = state.Radar.Contacts[lock];
    double ownLos = own.speed * std::cos(c.AzDeg * kDeg2Rad) * std::cos(c.ElDeg * kDeg2Rad);
    double tgtSpeed = std::sqrt(trk.VelE * trk.VelE + trk.VelN * trk.VelN + trk.VelU * trk.VelU);
    Weapons::FBLaunchZone z = Weapons::FBSolveLaunchZone(selected->Perf, selected->Seeker, own.speed,
                                                        own.elev, c.RangeM, c.ClosureMs, ownLos,
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

  SolveGun(state, own, gun, trk);
  /* No air-to-ground block: this aircraft's computer does not solve one (class banner). It stays
   * invalid, and every consumer already knows what to do with that. */
  b.AgValid = false;
  b.H.Publish(state.NowS);
}

} // namespace FlightBox::Modules
