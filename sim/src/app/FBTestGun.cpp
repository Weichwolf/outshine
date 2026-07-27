/* fb-test-gun: the gun's arithmetic against doc/f16/weapons.md's numbers and against itself. No JSBSim
 * and no mission — which is the point: the flight code's claims are checkable WITHOUT flying, and the
 * flying then only has to show the aircraft can reach the geometry these numbers describe.
 * Five checks: dispersion, ballistics, funnel geometry, the lead solution FLOWN, magazine + depletion.
 * Exit 0 = every check inside its stated tolerance, 1 = one failed and the line says which. */
#include "FBGun.h"
#include "FBGunBallistics.h"
#include "FBGunProjectiles.h"
#include "FBGunSystem.h"
#include "FBCommandBus.h"
#include "FBF16FireControl.h"
#include "FBLog.h"
#include "FBLogSinks.h"
#include "FBUnits.h"
#include <cmath>

using namespace FlightBox;

namespace {

int Failures = 0;

void Check(bool ok, const char *what, double got, double want, double tol) {
  if (!ok) Failures++;
  FBLog::Info("gun", ok ? "CHECK_OK" : "CHECK_FAIL", {{"what", what}, {"got", got}, {"want", want},
                                                      {"tol", tol}});
}

/* The closed form the dispersion sigma was fitted with. */
double PatternFraction(double radiusRad, double sigmaRad) {
  return 1.0 - std::exp(-(radiusRad * radiusRad) / (2.0 * sigmaRad * sigmaRad));
}

void CheckDispersion() {
  const double s = kM61A1.DispersionSigmaRad;
  double in8 = PatternFraction(4.0e-3, s);    /* 8 mil DIAMETER = 4 mil radius */
  double in12 = PatternFraction(6.0e-3, s);   /* 12 mil diameter */
  FBLog::Info("gun", "DISPERSION", {{"sigmaMr", s * 1000.0}, {"in8milPct", in8 * 100.0},
                                    {"in12milPct", in12 * 100.0}});
  Check(std::fabs(in8 - 0.80) < 0.005, "80% inside the 8 mil circle (MIL-DTL-45500/1A)", in8, 0.80,
        0.005);
  /* A Gaussian never reaches 1, so the check is >= 95%: the fit made from the 80% figure ALONE must
   * land where the second, unused figure says it should. */
  Check(in12 > 0.95, "the 12 mil circle holds essentially all of it", in12, 1.0, 0.05);
}

void CheckBallistics() {
  const double rho = 1.225;                       /* sea level, where a firing table is quoted */
  const double k = FBGunRetardation(kM61A1, rho);
  const double v0 = kM61A1.MuzzleVelMs;
  FBLog::Info("gun", "RETARDATION", {{"kPerM", k}, {"muzzleMs", v0},
                                     {"decelMs2", k * v0 * v0}});
  for (double s : {300.0, 500.0, 1000.0, 2000.0}) {
    double t = FBGunTimeToPath(k, v0, s);
    double v = FBGunSpeedAfter(k, v0, t);
    double drop = 0.5 * kGravityMs2 * t * t;
    FBLog::Info("gun", "BALLISTIC", {{"rangeM", s}, {"tofS", t}, {"speedMs", v}, {"dropM", drop},
                                     {"vacuumTofS", s / v0}});
    Check(t > s / v0, "a real round is slower than the drag-free floor", t, s / v0, 0.0);
    /* The closed form against its own inverse — the property the lead solve rests on. */
    Check(std::fabs(FBGunPathAfter(k, v0, t) - s) < 0.01, "path(time(path)) is an identity",
          FBGunPathAfter(k, v0, t), s, 0.01);
  }
  /* The one external cross-check: no firing table exists in the source, but every published 20 mm
   * table puts the flight time to 1,000 m between 1.1 s and 1.5 s. */
  double t1000 = FBGunTimeToPath(k, v0, 1000.0);
  Check(t1000 > 1.1 && t1000 < 1.5, "time of flight to 1,000 m is in the published band", t1000, 1.3,
        0.2);
}

void CheckFunnel() {
  const double span = FBF16FireControl::kTargetSpanM;
  double topMr = span / FBF16FireControl::kFunnelMinRangeM * 1000.0;
  double botMr = span / FBF16FireControl::kFunnelMaxRangeM * 1000.0;
  FBLog::Info("gun", "FUNNEL", {{"minRangeFt", FBF16FireControl::kFunnelMinRangeM * kMToFt},
                                {"maxRangeFt", FBF16FireControl::kFunnelMaxRangeM * kMToFt},
                                {"spanM", span}, {"topMr", topMr}, {"bottomMr", botMr}});
  Check(std::fabs(FBF16FireControl::kFunnelMinRangeM * kMToFt - 600.0) < 0.5,
        "the funnel's near end is the guide's 600 ft", FBF16FireControl::kFunnelMinRangeM * kMToFt,
        600.0, 0.5);
  Check(std::fabs(FBF16FireControl::kFunnelMaxRangeM * kMToFt - 3000.0) < 0.5,
        "...and its far end the guide's 3,000 ft", FBF16FireControl::kFunnelMaxRangeM * kMToFt, 3000.0,
        0.5);
  /* Five times the range is a fifth of the angular width — which is what makes a target that FILLS
   * the funnel be at the range its lead was computed for. */
  Check(std::fabs(topMr / botMr - FBF16FireControl::kFunnelMaxRangeM / FBF16FireControl::kFunnelMinRangeM)
            < 1e-9,
        "the funnel's width ratio is its range ratio", topMr / botMr, 5.0, 1e-6);
  /* The out-of-range test: past the far end a target appears SMALLER than the funnel's wide end. */
  double farMr = span / (2.0 * FBF16FireControl::kFunnelMaxRangeM) * 1000.0;
  Check(farMr < botMr, "a target past the funnel is smaller than its bottom", farMr, botMr, 0.0);
}

/* The lead solution FLOWN: solved once, then fired and integrated by the same pool the runner uses,
 * against an independently propagated target. The measurement is the prediction's error in metres. */
void CheckLead() {
  const double altM = 4000.0;
  const double ownE = 250.0, ownN = 0.0, ownU = 0.0;      /* shooter tracking east at 250 m/s */
  const double relE = 0.0, relN = 500.0, relU = 0.0;      /* target 500 m to the north */
  const double tgtE = 0.0, tgtN = 0.0, tgtU = 0.0;        /* ...and, first case, stationary */
  struct Case { const char *Name; double VE, VN, VU; };
  const Case cases[] = {
      {"stationary", tgtE, tgtN, tgtU},
      {"crossing_right", 200.0, 0.0, 0.0},
      {"crossing_left", -200.0, 0.0, 0.0},
      {"climbing_away", 0.0, 200.0, 50.0},
  };
  for (const Case &c : cases) {
    FBGunAim aim = FBGunSolveLead(kM61A1, altM, ownE, ownN, ownU, relE, relN, relU, c.VE, c.VN, c.VU);
    if (!aim.Valid) { Check(false, "the lead solve produced an answer", 0.0, 1.0, 0.0); continue; }

    /* Exactly what FBGunSystem builds, then flown by a pool that knows nothing about the solution. */
    FBGunProjectiles pool;
    FBGunBurst b;
    b.Kind = kM61A1.Kind;
    b.Rounds = 10;
    b.LatDeg = 47.0; b.LonDeg = 7.0; b.AltM = altM;
    b.VelE = ownE + aim.BoreE * kM61A1.MuzzleVelMs;
    b.VelN = ownN + aim.BoreN * kM61A1.MuzzleVelMs;
    b.VelU = ownU + aim.BoreU * kM61A1.MuzzleVelMs;
    pool.Launch(b);

    /* Propagated independently on its own straight line. */
    double te = relE, tn = relN, tu = relU;
    /* 1 ms: the sampled closest approach is quantised by how far a round moves in one step, and the
     * number measured is a fraction of the dispersion — otherwise this measures its own grid. */
    const double dt = 0.001;
    double best = 1e18, bestT = 0.0, missE = 0.0, missN = 0.0, missU = 0.0;
    for (double t = 0.0; t < 3.0; t += dt) {
      pool.Step(dt);
      te += c.VE * dt; tn += c.VN * dt; tu += c.VU * dt;
      const FBGunProjectiles::Bundle &bu = pool.At(0);
      if (!bu.Live) break;
      double coslat = std::cos(b.LatDeg * kDeg2Rad);
      double be = (bu.LonDeg - b.LonDeg) * kMPerDeg * coslat;
      double bn = (bu.LatDeg - b.LatDeg) * kMPerDeg;
      double bu_ = bu.AltM - b.AltM;
      double d = std::sqrt((be - te) * (be - te) + (bn - tn) * (bn - tn) + (bu_ - tu) * (bu_ - tu));
      if (d < best) { best = d; bestT = t + dt; missE = be - te; missN = bn - tn; missU = bu_ - tu; }
    }
    FBLog::Info("gun", "LEAD", {{"case", c.Name}, {"predTofS", aim.TofS}, {"flownTofS", bestT},
                                {"predRangeM", aim.RangeM}, {"missM", best},
                                {"missE", missE}, {"missN", missN}, {"missU", missU},
                                {"spreadM", aim.SpreadM}, {"impactMs", aim.ImpactSpeedMs}});
    /* The tolerance IS the pattern's own sigma: claiming better than the weapon's spread would be
     * meaningless. */
    Check(best < aim.SpreadM, "the flown bundle passes inside one dispersion sigma", best, 0.0,
          aim.SpreadM);
    Check(std::fabs(bestT - aim.TofS) < 0.05, "the predicted time of flight is the flown one", bestT,
          aim.TofS, 0.05);
  }
}

/* The drum: squeeze until empty, then squeeze again. */
void CheckMagazine() {
  FBGunSystem gun;
  gun.Install(kM61A1, 4.6, -0.9, -0.3, 0.0, 0.0);
  gun.SetMasterArm(FBArmState::Arm);

  FBState state{};
  state.Airframe.WeightOnWheels = false;
  state.Airframe.H.Publish(0.0);
  fb_fdm_state st{};
  st.lat = 47.0; st.lon = 7.0; st.elev = 4000.0;
  st.vx = 250.0;

  /* On the wheels first: the interlock that comes before anything else. */
  {
    FBGunSystem ground;
    ground.Install(kM61A1, 4.6, -0.9, -0.3, 0.0, 0.0);
    ground.SetMasterArm(FBArmState::Arm);
    FBCommandOutcome o = FBCommandOutcome::Pending;
    FBCommandReason r = FBCommandReason::None;
    bool fired = ground.Trigger(0.5, 0.0, o, r);
    FBLog::Info("gun", "INTERLOCK", {{"case", "weight_on_wheels"}, {"fired", fired},
                                     {"reason", FBCommandReasonStr(r)}});
    Check(!fired && r == FBCommandReason::HardwarePrecedence, "the gun refuses to fire on the ground",
          fired ? 1.0 : 0.0, 0.0, 0.0);
  }
  /* ...and master arm SAFE. */
  {
    FBGunSystem safe;
    safe.Install(kM61A1, 4.6, -0.9, -0.3, 0.0, 0.0);
    FBState s2{};
    s2.Airframe.WeightOnWheels = false;
    s2.Airframe.H.Publish(0.0);
    safe.Run(s2, st, 0.1, 0.1);
    FBCommandOutcome o = FBCommandOutcome::Pending;
    FBCommandReason r = FBCommandReason::None;
    bool fired = safe.Trigger(0.5, 0.1, o, r);
    FBLog::Info("gun", "INTERLOCK", {{"case", "master_arm_safe"}, {"fired", fired},
                                     {"reason", FBCommandReasonStr(r)}});
    Check(!fired && r == FBCommandReason::HardwarePrecedence, "...and with the master arm safe",
          fired ? 1.0 : 0.0, 0.0, 0.0);
  }

  /* Continuous fire the way a pilot produces it: the bus allows one switch action every 0.5 s, so a
   * held trigger is a squeeze re-commanded at that interval, each EXTENDING the running burst rather
   * than restarting it — and therefore not restarting the spool-up either. */
  const double dt = 0.1;
  double nowS = 0.0;
  int bursts = 0, roundsOut = 0;
  double firstBurstS = -1.0, lastBurstS = 0.0;
  double nextTriggerS = 0.0;
  FBCommandOutcome outcome = FBCommandOutcome::Pending;
  FBCommandReason reason = FBCommandReason::None;
  gun.Run(state, st, nowS, dt);   /* one cycle so the WOW state is read before the first trigger */
  for (int i = 0; i < 200; i++) {
    nowS += dt;
    if (gun.RoundsRemaining() > 0 && nowS >= nextTriggerS) {
      gun.Trigger(FBCommandBus::kHotasLatencyS, nowS, outcome, reason);
      nextTriggerS = nowS + FBCommandBus::kHotasLatencyS;
    }
    gun.Run(state, st, nowS, dt);
    FBGunBurst b;
    while (gun.TakeBurst(b)) {
      bursts++;
      roundsOut += b.Rounds;
      if (firstBurstS < 0.0) firstBurstS = nowS;
      lastBurstS = nowS;
    }
    if (gun.RoundsRemaining() <= 0) break;
  }
  double fireS = lastBurstS - firstBurstS + dt;
  FBLog::Info("gun", "MAGAZINE", {{"capacity", kM61A1.Capacity}, {"roundsOut", roundsOut},
                                  {"bursts", bursts}, {"firingS", fireS},
                                  {"meanRatePerS", fireS > 0.0 ? roundsOut / fireS : 0.0},
                                  {"remaining", gun.RoundsRemaining()}});
  Check(roundsOut == kM61A1.Capacity, "every round in the drum came out", roundsOut, kM61A1.Capacity,
        0.0);
  /* Just under the nominal rate: the spool-up is the only thing that can hold it back. */
  double mean = fireS > 0.0 ? roundsOut / fireS : 0.0;
  Check(mean > 90.0 && mean <= kM61A1.RoundsPerMin / 60.0, "the mean rate is the gun's, less spool-up",
        mean, kM61A1.RoundsPerMin / 60.0, 10.0);
  /* The spool-up is visible in the same number: half the ramp, later than the nominal rate alone. */
  double nominalS = kM61A1.Capacity / (kM61A1.RoundsPerMin / 60.0);
  FBLog::Info("gun", "SPOOL", {{"nominalS", nominalS}, {"firingS", fireS},
                               {"costRounds", (fireS - nominalS) * kM61A1.RoundsPerMin / 60.0}});

  bool fired = gun.Trigger(0.5, nowS + 1.0, outcome, reason);
  FBLog::Info("gun", "EMPTY", {{"fired", fired}, {"outcome", FBCommandOutcomeStr(outcome)},
                               {"reason", FBCommandReasonStr(reason)}});
  Check(!fired && reason == FBCommandReason::Depleted, "an empty drum refuses the trigger, and says so",
        fired ? 1.0 : 0.0, 0.0, 0.0);
}

} // namespace

int main() {
  FBStdoutLogSink sink;
  FBLogSinkScope scope(&sink);
  FBLog::SetTime(0.0);

  CheckDispersion();
  CheckBallistics();
  CheckFunnel();
  CheckLead();
  CheckMagazine();

  FBLog::Info("gun", "RESULT", {{"failures", Failures}});
  return Failures == 0 ? 0 : 1;
}
