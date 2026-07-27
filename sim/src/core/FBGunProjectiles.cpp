#include "FBGunProjectiles.h"
#include "FBAtmosphere.h"
#include "FBGunBallistics.h"
#include "FBUnits.h"
#include <cmath>

namespace FlightBox {

bool FBGunProjectiles::Launch(const FBGunBurst &burst) {
  const FBGunSpec *spec = FBGunSpecOf(burst.Kind);
  if (!spec || burst.Rounds <= 0) return false;
  for (Bundle &b : B_) {
    if (b.Live) continue;
    b = Bundle{};
    b.Live = true;
    b.LauncherId = burst.LauncherId;
    b.Spec = spec;
    b.Rounds = burst.Rounds;
    b.LatDeg = b.PrevLatDeg = burst.LatDeg;
    b.LonDeg = b.PrevLonDeg = burst.LonDeg;
    b.AltM = b.PrevAltM = burst.AltM;
    b.VelE = burst.VelE; b.VelN = burst.VelN; b.VelU = burst.VelU;
    b.FiredS = burst.SimTimeS;
    Launched_++;
    return true;
  }
  Dropped_++;
  return false;
}

/* Drag on the SPEED (closed form), gravity on the vertical, trapezoidal position update on the mean of
 * the two velocities — second order in dt, which matters at 0.1 s ticks and ~1,000 m/s. */
void FBGunProjectiles::Step(double dt) {
  if (dt <= 0.0) return;
  for (Bundle &b : B_) {
    if (!b.Live) continue;
    const FBGunSpec &spec = *b.Spec;
    double v = std::sqrt(b.VelE * b.VelE + b.VelN * b.VelN + b.VelU * b.VelU);
    if (v < 1.0) { b.Live = false; continue; }
    double k = FBGunRetardation(spec, FBIsaDensity(b.AltM));
    double vNew = FBGunSpeedAfter(k, v, dt);
    double scale = vNew / v;

    double e0 = b.VelE, n0 = b.VelN, u0 = b.VelU;
    b.VelE *= scale; b.VelN *= scale; b.VelU *= scale;
    b.VelU -= kGravityMs2 * dt;

    double me = 0.5 * (e0 + b.VelE), mn = 0.5 * (n0 + b.VelN), mu = 0.5 * (u0 + b.VelU);
    b.PrevLatDeg = b.LatDeg; b.PrevLonDeg = b.LonDeg; b.PrevAltM = b.AltM;
    double coslat = std::cos(b.LatDeg * kDeg2Rad);
    b.LatDeg += mn * dt / kMPerDeg;
    if (coslat > 1e-6) b.LonDeg += me * dt / (kMPerDeg * coslat);
    b.AltM += mu * dt;

    b.PathM += std::sqrt(me * me + mn * mn + mu * mu) * dt;
    b.AgeS += dt;
    if (b.AgeS >= kMaxAgeS || b.PathM >= kMaxPathM) b.Live = false;
  }
}

void FBGunProjectiles::Retire(int index) {
  if (index < 0 || index >= kMaxBundles) return;
  B_[index].Live = false;
}


int FBGunProjectiles::LiveCount() const {
  int n = 0;
  for (const Bundle &b : B_)
    if (b.Live) n++;
  return n;
}

} // namespace FlightBox
