#include "FBDamageModel.h"
#include "FBUnits.h"
#include <cmath>

namespace FlightBox {

namespace {
/* The distance from the burst to a zone: the airframe is its longitudinal axis segment (file banner), so
 * the along-axis coordinate is CLAMPED into the zone's own stretch and the lateral/vertical offsets are
 * carried through unchanged. A burst abeam the middle of a zone is therefore as close as its lateral
 * miss; one past the nose has to reach back along the axis as well. */
double ZoneRangeM(const FBBurst &b, const FBDamageZoneSpec &z) {
  double fwd = b.FwdM;
  if (fwd < z.AftM) fwd = z.AftM;
  else if (fwd > z.FwdM) fwd = z.FwdM;
  double dx = b.FwdM - fwd;
  return std::sqrt(dx * dx + b.RightM * b.RightM + b.DownM * b.DownM);
}
} // namespace

const char *FBDamageZoneStr(FBDamageZone z) {
  switch (z) {
    case FBDamageZone::None: return "none";
    case FBDamageZone::Nose: return "nose";
    case FBDamageZone::Forward: return "forward";
    case FBDamageZone::Center: return "center";
    case FBDamageZone::Aft: return "aft";
  }
  return "?";
}

double FBFragmentFluxJm2(double warheadKg, double rangeM, double closureMs) {
  if (warheadKg <= 0.0) return 0.0;
  /* A floor on the range, not a physical statement: the 1/r^2 law diverges at zero and a burst inside
   * the airframe is not more instructive than one against its skin. 0.5 m is about the half-width of a
   * fighter fuselage, i.e. the closest a burst can be to the axis and still be outside the aircraft. */
  double r = rangeM > 0.5 ? rangeM : 0.5;
  double areal = warheadKg * kCaseFraction / (4.0 * kPi * r * r);
  double v2 = kFragSpeedMs * kFragSpeedMs + closureMs * closureMs;
  return 0.5 * areal * v2;
}

FBDamageResult FBDamageModel::Apply(const FBBurst &burst, const FBDamageLayout &layout,
                                    FBSystemHealth &health) {
  FBDamageResult res;
  res.WasEffective = health.CombatEffective();
  res.NowEffective = res.WasEffective;
  if (!layout.Zones || layout.ZoneCount <= 0 || burst.WarheadKg <= 0.0) return res;

  health.NoteHit();
  for (int i = 0; i < layout.ZoneCount; i++) {
    const FBDamageZoneSpec &z = layout.Zones[i];
    double r = ZoneRangeM(burst, z);
    double flux = FBFragmentFluxJm2(burst.WarheadKg, r, burst.ClosureMs);
    if (flux > res.PeakFluxJm2) {
      res.PeakFluxJm2 = flux;
      res.RangeM = r;
      res.Zone = z.Zone;
    }
    for (int k = 0; k < z.SystemCount; k++) {
      const FBZoneSystem &s = z.Systems[k];
      FBHealthState want = FBHealthState::Intact;
      if (s.FailJm2 > 0.0 && flux >= s.FailJm2) want = FBHealthState::Failed;
      else if (s.DegradeJm2 > 0.0 && flux >= s.DegradeJm2) want = FBHealthState::Degraded;
      if (want == FBHealthState::Intact) continue;
      if (!health.Worsen(s.Id, want)) continue;
      uint32_t bit = 1u << (int)s.Id;
      if (want == FBHealthState::Failed) { res.NewlyFailed |= bit; res.NewlyDegraded &= ~bit; }
      else res.NewlyDegraded |= bit;
    }
  }
  res.NowEffective = health.CombatEffective();
  return res;
}

} // namespace FlightBox
