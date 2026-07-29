#include "FBLaunchZone.h"
#include "FBAtmosphere.h"

namespace FlightBox::Weapons {

FBLaunchZone FBSolveLaunchZone(const FBWeaponPerf &perf, FBSeekerKind seeker, double ownSpeedMs,
                               double altM, double rangeM, double closureMs, double ownLosMs,
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
  double activeS = -1.0;
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
    if (perf.ActivationRangeM > 0.0 && activeS < 0.0 && r <= perf.ActivationRangeM) activeS = t + dt;
    if (z.TimeToImpactS < 0.0 && r <= 0.0) z.TimeToImpactS = t + dt;
    /* Dead: no closure left to run an intercept with. Only tested after the boost, since the round
     * starts below MinSpeed whenever the launcher is subsonic. */
    if (t > perf.BoostS && v < perf.MinSpeedMs) { t += dt; break; }
  }

  z.Valid = true;
  z.RaeroM = s + tgtLosMs * t;
  z.RtrM = s - tgtSpeedMs * t;
  if (z.RtrM < 0.0) z.RtrM = 0.0;
  /* EIN INFRAROTKOPF MUSS VOR DEM START ERFASST HABEN ("lock before launch, then fire-and-forget",
   * doc/weapons.md Spec), also kann eine solche Runde nicht weiter gestartet werden, als ihr eigener
   * Kopf reicht — die Kinematik ist dann nicht mehr die Grenze. Fuer eine aktive Runde gilt das NICHT
   * (der Uplink traegt sie bis zum Aktivierungsring) und fuer eine halbaktive auch nicht (der Schuetze
   * beleuchtet); deshalb haengt es an der Sucherart und nicht an der Runde. */
  if (seeker == FBSeekerKind::Infrared && perf.SeekerRangeM > 0.0) {
    if (z.RaeroM > perf.SeekerRangeM) z.RaeroM = perf.SeekerRangeM;
    if (z.RtrM > perf.SeekerRangeM) z.RtrM = perf.SeekerRangeM;
  }
  z.RminM = (closureMs > 0.0 ? closureMs : 0.0) * perf.ArmingS + kLaunchZoneMinTurnM;
  /* The SHOOTER's obligation, not the seeker's power-up: a seeker kind decides it, and one of the three
   * answers is "never" (core/FBStore.h). */
  z.TimeToActiveS = FBSeekerHandoverS(seeker, activeS);
  return z;
}

} // namespace FlightBox::Weapons
