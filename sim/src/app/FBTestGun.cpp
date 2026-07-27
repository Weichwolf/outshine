/* FlightBox — fb-test-gun: the gun's own arithmetic, checked against doc/f16/weapons.md's numbers and
 * against itself. No JSBSim, no mission: everything here is core/FBGunBallistics + core/FBGunProjectiles
 * + systems/FBGunSystem, which is the point — the flight code's claims are checkable without flying,
 * and the flying (missions/gun-bfm.fbm) then only has to show that the aircraft can be brought into the
 * geometry these numbers describe.
 *
 * FIVE CHECKS, each printed with the figure it is checked against:
 *   1 DISPERSION   the fitted circular-normal pattern reproduces MIL-DTL-45500/1A's 80%-in-8-mil
 *                  specification it was fitted to, AND predicts the 12-mil figure it was NOT fitted to.
 *   2 BALLISTICS   time of flight, residual speed and drop at gun ranges — against the drag-free floor
 *                  (§4.2's sanity rule: a real round can never arrive sooner than the vacuum case).
 *   3 FUNNEL       the EEGS funnel's geometry: the guide's own 600 ft / 3,000 ft range window turned
 *                  into the angular spans that ARE the funnel's two ends, and the out-of-range test.
 *   4 LEAD         the fire control's solution FLOWN: solve the lead for a crossing target, fire the
 *                  bundle along the solved bore with the projectile pool, integrate, and measure how
 *                  far it actually passes the target. Prediction against reality, in metres.
 *   5 MAGAZINE     the drum empties at the rate it should and the box then refuses the trigger with
 *                  Depleted — the refusal the whole command path exists to be able to give.
 * Exit 0 = every check inside its stated tolerance; 1 = one failed (the line says which). */
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

/* The fraction of a circular-normal pattern inside a circle of `radiusRad` — the closed form the
 * dispersion sigma was fitted with (core/FBGun.h). */
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
  /* The guide calls the 12 mil circle "100%". A Gaussian never reaches 1, so what is checked is that it
   * is at least 95% — i.e. that the fit made from the 80% figure alone lands where the second, unused
   * figure says it should. */
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
    /* Round-trip the closed form against its own inverse — the property the whole lead solve rests on. */
    Check(std::fabs(FBGunPathAfter(k, v0, t) - s) < 0.01, "path(time(path)) is an identity",
          FBGunPathAfter(k, v0, t), s, 0.01);
  }
  /* The one external cross-check available: doc/f16/weapons.md §4.1 gives no firing table, but every
   * published 20 mm table puts the flight time to 1,000 m between 1.1 s and 1.5 s. */
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
  /* The funnel is a shape, and the shape is the wingspan at each range: five times the range is a fifth
   * of the angular width, which is what makes a target that fills it be AT the range the lead was
   * computed for. */
  Check(std::fabs(topMr / botMr - FBF16FireControl::kFunnelMaxRangeM / FBF16FireControl::kFunnelMinRangeM)
            < 1e-9,
        "the funnel's width ratio is its range ratio", topMr / botMr, 5.0, 1e-6);
  /* ...and the guide's out-of-range test: a target beyond the far end appears SMALLER than the funnel's
   * wide end. */
  double farMr = span / (2.0 * FBF16FireControl::kFunnelMaxRangeM) * 1000.0;
  Check(farMr < botMr, "a target past the funnel is smaller than its bottom", farMr, botMr, 0.0);
}

/* CHECK 4: the fire control's lead solution, FLOWN. One synthetic engagement — the shooter running
 * level, the target crossing at 90 degrees 500 m ahead — solved once, then fired and integrated with the
 * same projectile pool the mission runner uses. What is measured is the distance between the bundle and
 * the (independently propagated) target at closest approach: the prediction's own error, in metres. */
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

    /* Fire it. The bundle leaves with the shooter's velocity plus the muzzle velocity along the SOLVED
     * BORE — exactly what systems/FBGunSystem builds — and is then flown by the pool, which knows
     * nothing about the solution. */
    FBGunProjectiles pool;
    FBGunBurst b;
    b.Kind = kM61A1.Kind;
    b.Rounds = 10;
    b.LatDeg = 47.0; b.LonDeg = 7.0; b.AltM = altM;
    b.VelE = ownE + aim.BoreE * kM61A1.MuzzleVelMs;
    b.VelN = ownN + aim.BoreN * kM61A1.MuzzleVelMs;
    b.VelU = ownU + aim.BoreU * kM61A1.MuzzleVelMs;
    pool.Launch(b);

    /* The target, propagated independently on its own straight line from the same origin. */
    double te = relE, tn = relN, tu = relU;
    /* 1 ms steps: the sampled closest approach is quantised by how far the round moves in one step
     * (~1 m here), and the number being measured is a fraction of the dispersion pattern — so the
     * sampling has to be well under it or the harness would be measuring its own grid. */
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
    /* The tolerance is the pattern's own sigma: a solution whose axis passes the target inside one sigma
     * of the round dispersion is a solution — claiming better than the weapon's own spread would be
     * meaningless. */
    Check(best < aim.SpreadM, "the flown bundle passes inside one dispersion sigma", best, 0.0,
          aim.SpreadM);
    Check(std::fabs(bestT - aim.TofS) < 0.05, "the predicted time of flight is the flown one", bestT,
          aim.TofS, 0.05);
  }
}

/* CHECK 5: the drum. Squeeze until it is empty, then squeeze again. */
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

  /* On the wheels first: the interlock a jet applies before anything else. */
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

  /* CONTINUOUS FIRE, the way a pilot produces it: the command bus allows one action on a switch every
   * 0.5 s (core/FBCommandBus.h), so a held trigger is a squeeze re-commanded at exactly that interval,
   * each one EXTENDING the burst already running rather than restarting it (and therefore not
   * restarting the spool-up either). */
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
  /* The mean rate over the whole magazine sits just under the nominal 100 rd/s: the spool-up is the
   * only thing that can hold it back, and it is worth ~15 rounds once (core/FBGun.h). */
  double mean = fireS > 0.0 ? roundsOut / fireS : 0.0;
  Check(mean > 90.0 && mean <= kM61A1.RoundsPerMin / 60.0, "the mean rate is the gun's, less spool-up",
        mean, kM61A1.RoundsPerMin / 60.0, 10.0);
  /* ...and the spool-up is visible in the same number: the whole drum comes out ~0.15 s later than the
   * nominal rate alone would put it (half the ramp, core/FBGun.h's SpoolUpS). */
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
