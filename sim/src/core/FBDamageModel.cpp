#include "FBDamageModel.h"
#include "FBUnits.h"
#include <cmath>

namespace FlightBox {

namespace {
/* The airframe is its longitudinal axis segment: clamp the along-axis coordinate into the zone's own
 * stretch, carry the lateral/vertical offsets through unchanged. */
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
  /* A floor, not a physical statement: 1/r^2 diverges at zero, and 0.5 m is about the closest a burst
   * can be to the axis and still be outside the aircraft. */
  double r = rangeM > 0.5 ? rangeM : 0.5;
  double areal = warheadKg * kCaseFraction / (4.0 * kPi * r * r);
  double v2 = kFragSpeedMs * kFragSpeedMs + closureMs * closureMs;
  return 0.5 * areal * v2;
}

double FBPresentedAreaM2(const FBDamageLayout &layout, double fwd, double right, double down) {
  double len = std::sqrt(fwd * fwd + right * right + down * down);
  if (len < 1e-9) return layout.FrontalAreaM2;
  double along = std::fabs(fwd) / len;
  double across = std::sqrt(1.0 - along * along);
  return layout.FrontalAreaM2 * along + layout.LateralAreaM2 * across;
}

double FBPresentedExtentM(const FBDamageLayout &layout, double fwd, double right, double down) {
  double len = std::sqrt(fwd * fwd + right * right + down * down);
  if (len < 1e-9) return layout.FrontalExtentM;
  double along = std::fabs(fwd) / len;
  double across = std::sqrt(1.0 - along * along);
  return layout.FrontalExtentM * along + layout.LateralExtentM * across;
}

FBDamageResult FBDamageModel::ApplyKinetic(const FBKineticBurst &burst, const FBDamageLayout &layout,
                                           FBSystemHealth &health) {
  FBDamageResult res;
  res.WasEffective = health.CombatEffective();
  res.NowEffective = res.WasEffective;
  if (!layout.Zones || layout.ZoneCount <= 0 || burst.FluxJm2 <= 0.0) return res;

  health.NoteHit();
  /* One sigma of the pattern, floored so a point-blank burst lands on the zone it passed through
   * rather than on a mathematical point between two of them. */
  double half = burst.SpreadM > 0.5 ? burst.SpreadM : 0.5;
  double aft = burst.FwdM - half, fwd = burst.FwdM + half;
  for (int i = 0; i < layout.ZoneCount; i++) {
    const FBDamageZoneSpec &z = layout.Zones[i];
    if (fwd < z.AftM || aft > z.FwdM) continue;   /* the stream missed this stretch of the airframe */
    /* What this ZONE has taken altogether, not what this bundle brought: fifty rounds arriving as
     * five bundles do the damage of fifty rounds. */
    double total = health.AddKinetic((int)z.Zone, burst.FluxJm2);
    if (total > res.PeakFluxJm2) {
      res.PeakFluxJm2 = total;
      res.RangeM = 0.0;                           /* a hit, not a stand-off burst: there is no range */
      res.Zone = z.Zone;
    }
    for (int k = 0; k < z.SystemCount; k++) {
      const FBZoneSystem &s = z.Systems[k];
      FBHealthState want = FBHealthState::Intact;
      if (s.FailJm2 > 0.0 && total >= s.FailJm2) want = FBHealthState::Failed;
      else if (s.DegradeJm2 > 0.0 && total >= s.DegradeJm2) want = FBHealthState::Degraded;
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
