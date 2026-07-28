/* fb-test-mig29-envelope: the ACCEPTANCE TEST for sim/assets/aircraft/mig29, one measurement per
 * anchor of doc/mig29/flight-model-spec.md §8. That table is the deliverable the spec says outlives
 * the rest of the file; this harness is the other half of it, and the model is only as built as the
 * numbers printed here.
 *
 * NOT a simulation client: no module, no registry, no mission, no pilot. It reaches the airframe the
 * one legal way (fdm/FBFdmBoot) and flies it with the crudest controllers that can hold a condition
 * long enough to read a number off it — an instrument, not an autopilot. Every controller here is a
 * proportional law on a measured error, so what it measures is the AIRFRAME's performance and not a
 * regulator's.
 *
 * Each anchor emits one ANCHOR line: id, what the source says, what the model does, deviation in
 * percent, and whether it is a "must" row. Exit 0 = every must-row anchor was MEASURED (not
 * necessarily met — a missed anchor is a documented result, per the spec's §9 rule 1, and it is the
 * report's job to say what it costs); 1 = a measurement could not be made at all; 2 = the model would
 * not load or would not trim. */
#include "FBFdmBoot.h"
#include "FBLog.h"
#include "FBLogSinks.h"
#include "FBUnits.h"
#include <cmath>
#include <cstdio>
#include <memory>

using namespace FlightBox;

namespace {

const char *kModelsRoot = "assets/aircraft";
const char *kAircraft = "mig29";
const double kLatDeg = 46.7, kLonDeg = 6.8;   /* Payerne, the tree's usual reference point */

/* Masses the anchors are quoted at. [DCS-FM p.14] normal takeoff 15,300 kg; the deck itself carries
 * empty + internal fuel + pilot = 14,200 kg, the rest being stores (a module concern). */
const double kEmptyKg = 10900.0, kPilotKg = 100.0, kFuelFullKg = 3200.0;

double FuelPctFor(double grossKg) {
  double fuel = grossKg - kEmptyKg - kPilotKg;
  double pct = 100.0 * fuel / kFuelFullKg;
  return pct < 0.0 ? 0.0 : pct > 100.0 ? 100.0 : pct;
}

double Clamp(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }

/* ---- Anchor bookkeeping. One row of the spec §8 table. ---- */

struct Anchor {
  const char *Id = "";
  const char *What = "";
  double Target = 0.0;      /* the documented figure */
  double Measured = 0.0;
  const char *Unit = "";
  bool Measured_ok = false;
  bool Must = true;
};

Anchor gAnchors[40];
int gAnchorCount = 0;

void Record(const char *id, const char *what, double target, double measured, const char *unit,
            bool ok, bool must = true) {
  if (gAnchorCount >= 40) return;
  Anchor &a = gAnchors[gAnchorCount++];
  a.Id = id; a.What = what; a.Target = target; a.Measured = measured; a.Unit = unit;
  a.Measured_ok = ok; a.Must = must;
  double dev = (ok && target != 0.0) ? 100.0 * (measured - target) / target : 0.0;
  FBLog::Info("mig29", "ANCHOR", {{"id", id}, {"what", what}, {"target", target},
      {"measured", ok ? measured : 0.0}, {"unit", unit}, {"devPct", dev},
      {"ok", ok}, {"must", must}});
}

/* ---- The airframe under test. ---- */

std::unique_ptr<Fdm::FBFdm> Spawn(double altM, double casKt, double grossKg) {
  Fdm::FBFdmSpawn ic;
  ic.ModelsRoot = kModelsRoot;
  ic.Aircraft = kAircraft;
  ic.LatDeg = kLatDeg; ic.LonDeg = kLonDeg;
  ic.GroundElevM = 0.0;
  ic.HeightOffsetM = altM;
  ic.SpeedMs = casKt * kKtToMs;
  ic.HeadingDeg = 90.0;
  std::unique_ptr<Fdm::FBFdm> f = Fdm::FBFdmBoot::Spawn(ic);
  if (!f) return f;
  f->SetGroundElevM(0.0);
  f->SetFuelPct(FuelPctFor(grossKg));
  f->SetGear(0.0);
  return f;
}

std::unique_ptr<Fdm::FBFdm> SpawnOnGround(double grossKg) {
  Fdm::FBFdmSpawn ic;
  ic.ModelsRoot = kModelsRoot;
  ic.Aircraft = kAircraft;
  ic.LatDeg = kLatDeg; ic.LonDeg = kLonDeg;
  ic.GroundElevM = 0.0;
  ic.HeightOffsetM = -1.0;   /* sit on the gear at the model's own clearance */
  ic.SpeedMs = 0.0;
  ic.HeadingDeg = 90.0;
  std::unique_ptr<Fdm::FBFdm> f = Fdm::FBFdmBoot::Spawn(ic);
  if (!f) return f;
  f->SetGroundElevM(0.0);
  f->SetFuelPct(FuelPctFor(grossKg));
  f->SetGear(1.0);
  return f;
}

/* ---- The two controllers, both deliberately primitive. ----
 *
 * This airframe has no FLCS in its deck (spec §7: mechanically signalled, not fly-by-wire), so a stick
 * command IS a stabilator deflection and an undamped proportional law would oscillate at the short
 * period. Both laws below therefore carry an explicit pitch-rate term — that is the SAU damper's job
 * on the real aircraft and systems/FBFlightControl's job in FlightBox, and neither exists here. */

/* Hold an altitude: pitch to a vertical-speed target, damp on q. */
double AltHoldStick(const Fdm::fb_fdm_state &st, double targetAltM) {
  double vsTarget = Clamp(0.08 * (targetAltM - st.elev), -25.0, 25.0);
  double cmd = 0.030 * (vsTarget - st.vy) - 0.030 * st.q;
  return Clamp(cmd, -0.6, 0.6);
}

/* Roll level, for the tests that must stay wings-level for minutes. */
double WingsLevelStick(const Fdm::fb_fdm_state &st) {
  return Clamp(-0.030 * st.roll - 0.010 * st.p, -0.5, 0.5);
}

/* THE SOS SKETCH — and it lives HERE, in the instrument, on purpose.
 *
 * Spec §7.3 keeps the AoA limiter out of the deck (it is a stick FORCE, it must be overridable, and
 * burying it in the deck would hide it from core/FBFlightMonitor), so this airframe has NO limiting of
 * any kind: a full-stick pull is answered by 35 deg of stabilator against a 5 % static margin, and the
 * measured result is a tumble to 180 deg of alpha, not a turn. That is a true statement about a bare
 * mechanically-signalled airframe and a useless one about its turn performance.
 *
 * So the maximum-pull measurements below command the DOCUMENTED limits instead — 26 deg alpha [T4,
 * corroborated by the 25 deg gauge marker DCS-EA p.19] and +9 g [DCS-FM p.14] — through the crudest
 * limiter that can hold them: proportional on whichever error bites first, damped on pitch rate. It is
 * a stand-in for the SOS/damper pair that systems/FBFlightControl will carry at module level, and it
 * is deliberately NOT a control law worth keeping: what it measures is how much turn rate the AIRFRAME
 * has at the documented limits. */
struct PullLimiter {
  double AlphaDot = 0.0, NzDot = 0.0, PrevAlpha = 0.0, PrevNz = 0.0;
  bool Primed = false;

  /* The lead terms are the DERIVATIVES OF THE LIMITED QUANTITIES, not the pitch rate. Two earlier
   * versions of this limiter got that wrong in opposite directions and both are worth recording:
   * without any lead it overshot 26 deg to a mean of 58; with PITCH RATE as the lead it undershot,
   * because a steady 9 g turn carries a steady q of ~12 deg/s and the lead read that as an impending
   * overshoot, holding alpha at 17.7 and g at 7.3 while claiming limits of 26 and 9. A derivative of
   * the limited quantity itself is zero in the steady turn, so it biases nothing and still leads the
   * transient. Lightly filtered: a raw 100 Hz difference is mostly quantisation. */
  double Stick(const Fdm::fb_fdm_state &st, double alphaLimitDeg, double gLimit, double dt) {
    if (Primed) {
      const double k = 0.15;   /* ~0.07 s time constant at 100 Hz */
      AlphaDot += k * ((st.alphaDeg - PrevAlpha) / dt - AlphaDot);
      NzDot += k * ((st.nz - PrevNz) / dt - NzDot);
    }
    PrevAlpha = st.alphaDeg; PrevNz = st.nz; Primed = true;

    double byAlpha = 0.20 * (alphaLimitDeg - st.alphaDeg - 0.45 * AlphaDot);
    double byG = 0.25 * (gLimit - st.nz - 0.35 * NzDot);
    double cmd = byAlpha < byG ? byAlpha : byG;
    return Clamp(cmd, -1.0, 0.9);
  }
};

const double kDt = Fdm::FBFdm::kStepS;

/* ================================ Step 1: ground and mass ================================ */

bool MeasureGroundAndMass() {
  std::unique_ptr<Fdm::FBFdm> f = SpawnOnGround(15300.0);
  if (!f) return false;

  double clearance = f->GetGroundClearanceM(true);
  double bellyClearance = f->GetGroundClearanceM(false);
  double capLbs = f->GetFuelCapacityLbs();

  /* Deck mass at "full internal fuel, pilot aboard, no stores". */
  f->SetFuelPct(100.0);
  Fdm::fb_fdm_state st{};
  f->Step(st);
  double deckKg = f->GetWeightLbs() / 2.20462;

  FBLog::Info("mig29", "GEOMETRY", {{"gearDownClearanceM", clearance},
      {"bellyClearanceM", bellyClearance}, {"fuelCapacityLbs", capLbs},
      {"fuelTanks", (double)f->GetFuelTankCount()}});

  /* The spec §3.1 closure: 10,900 + 3,200 + 100 = 14,200 kg from the deck, + 926 kg standard
   * air-to-air loadout + 125 kg ammunition = 15,251 against a documented 15,300. Only the first
   * part is the deck's; the rest arrives as point masses at module level. */
  Record("MASS", "deck mass empty+fuel+pilot", 14200.0, deckKg, "kg", true);
  Record("MASS-CLOSURE", "deck + 926 stores + 125 ammo vs documented normal TOW",
         15300.0, deckKg + 926.0 + 125.0, "kg", true);
  Record("FUEL", "internal fuel capacity", kFuelFullKg, capLbs / 2.20462, "kg", true);

  /* Does it SIT? 20 s at idle, brakes on: the gear must neither sink through the runway nor bounce. */
  double alt0 = 0.0, altMin = 1e9, altMax = -1e9, nzMax = 0.0;
  for (int i = 0; i < (int)(20.0 / kDt); i++) {
    f->SetControls(0.0, 0.0, 0.0, 0.0);
    f->SetWheelBrakes(1.0, 1.0);
    f->Step(st);
    if (i == 200) alt0 = st.elev;   /* after the first 2 s of settling */
    if (i > 200) {
      altMin = st.elev < altMin ? st.elev : altMin;
      altMax = st.elev > altMax ? st.elev : altMax;
      double d = std::fabs(st.nz - 1.0);
      nzMax = d > nzMax ? d : nzMax;
    }
  }
  double bounceM = altMax - altMin;
  FBLog::Info("mig29", "GROUND_SIT", {{"altAfterSettleM", alt0}, {"bounceM", bounceM},
      {"nzDeviationMax", nzMax}, {"wow", f->GetWow()}, {"gsMs", st.gs}});
  Record("S1-SIT", "static gear travel over 18 s (bounce)", 0.0, bounceM, "m", true);

  /* Taxi and brake-check, the two documented part-power anchors [DCS-EA p.75/76].
   * Throttle maps to spool as N2 = idleN2 + throttle_pos * (maxN2 - idleN2) with
   * throttle_pos = 2 * cmd (the deck's Throttle channel), so cmd = (N2 - 65)/70. */
  {
    std::unique_ptr<Fdm::FBFdm> g = SpawnOnGround(15300.0);
    if (!g) return false;
    Fdm::fb_fdm_state gs{};
    double cmd80 = (80.0 - 65.0) / 70.0;
    for (int i = 0; i < (int)(15.0 / kDt); i++) {
      g->SetControls(0.0, 0.0, 0.0, cmd80);
      g->SetWheelBrakes(1.0, 1.0);
      g->Step(gs);
    }
    double creepMs = gs.gs;
    FBLog::Info("mig29", "BRAKE_CHECK", {{"n2Pct", 80.0}, {"throttleCmd", cmd80},
        {"groundspeedMs", creepMs}});
    Record("S1-BRAKE", "groundspeed after 15 s at 80% RPM, brakes full [DCS-EA p.75]",
           0.0, creepMs, "m/s", true);
  }
  {
    std::unique_ptr<Fdm::FBFdm> g = SpawnOnGround(15300.0);
    if (!g) return false;
    Fdm::fb_fdm_state gs{};
    double cmd73 = (73.0 - 65.0) / 70.0;
    for (int i = 0; i < (int)(30.0 / kDt); i++) {
      g->SetControls(0.0, 0.0, 0.0, cmd73);
      g->SetWheelBrakes(0.0, 0.0);
      g->Step(gs);
    }
    double taxiKt = gs.gs * kMsToKt;
    FBLog::Info("mig29", "TAXI", {{"n2Pct", 73.0}, {"throttleCmd", cmd73},
        {"groundspeedKt", taxiKt}});
    /* "Comfortable taxi speed" is not a number in the source; 20-30 kt is what a taxiing fighter
     * does, and the anchor is that the aircraft ROLLS at this setting and can be held with light
     * braking. Recorded against 25 kt as the mid-band, flagged as non-must. */
    Record("S1-TAXI", "groundspeed after 30 s at 73% RPM, brakes off [DCS-EA p.76]",
           25.0, taxiKt, "kt", true, false);
  }
  return true;
}

/* ================================ Steps 2-3: level acceleration ================================ */

/* Full augmentation, hold altitude, run until the speed stops rising. Returns the peak Mach. */
bool MeasureVmax(double altM, double entryCasKt, double grossKg, double maxS,
                 double &machOut, double &kmhOut) {
  std::unique_ptr<Fdm::FBFdm> f = Spawn(altM, entryCasKt, grossKg);
  if (!f) return false;
  Fdm::fb_fdm_state st{};
  f->Step(st);

  double best = 0.0, bestSpeed = 0.0;
  double checkpoint = 0.0;   /* the Mach at the last convergence check — NOT the running best, or the
                              * test compares the peak against itself and always reads "converged" */
  int stagnant = 0;
  int n = (int)(maxS / kDt);
  for (int i = 0; i < n; i++) {
    f->SetControls(WingsLevelStick(st), AltHoldStick(st, altM), 0.0, 1.0);
    f->Step(st);
    if (f->Faulted()) return false;
    if (st.mach > best) { best = st.mach; bestSpeed = st.speed; }
    if (i > 0 && i % 1000 == 0) {   /* every 10 s */
      if (best > checkpoint + 0.002) { checkpoint = best; stagnant = 0; }
      else if (++stagnant >= 3) break;   /* 30 s without gaining 0.002 M = converged */
    }
  }
  machOut = best;
  kmhOut = bestSpeed * 3.6;
  FBLog::Info("mig29", "VMAX_RUN", {{"altM", altM}, {"grossKg", grossKg}, {"mach", best},
      {"kmh", kmhOut}, {"altEndM", st.elev}, {"casKtEnd", st.cas * kMsToKt}});
  return best > 0.1;
}

/* Specific excess power at a STATED Mach. Measured while the aircraft accelerates THROUGH that Mach at
 * full power, in a window centred on it: Ps is a function of the flight condition, so a settling time
 * long enough to spool the engines is also long enough to leave the condition being asked about (the
 * first version of this test spooled up for 6 s and reported Ps at M 1.02 under the name M 0.90).
 * Energy form, so a controller that trades a metre of altitude does not corrupt the reading. */
bool MeasurePs(double altM, double targetMach, double grossKg, double &psOut, double &machOut) {
  std::unique_ptr<Fdm::FBFdm> f = Spawn(altM, (targetMach - 0.18) * 340.29 * kMsToKt, grossKg);
  if (!f) return false;
  Fdm::fb_fdm_state st{};
  f->Step(st);
  const double kHalfBand = 0.02;
  int guard = (int)(240.0 / kDt);
  while (st.mach < targetMach - kHalfBand && guard-- > 0) {
    f->SetControls(WingsLevelStick(st), AltHoldStick(st, altM), 0.0, 1.0);
    f->Step(st);
    if (f->Faulted()) return false;
  }
  if (guard <= 0) return false;
  double v0 = st.speed, h0 = st.elev, t = 0.0;
  double machSum = 0.0; int nMach = 0;
  guard = (int)(60.0 / kDt);
  while (st.mach < targetMach + kHalfBand && guard-- > 0) {
    f->SetControls(WingsLevelStick(st), AltHoldStick(st, altM), 0.0, 1.0);
    f->Step(st);
    if (f->Faulted()) return false;
    t += kDt;
    machSum += st.mach; nMach++;
  }
  if (t < 0.05) return false;
  double dV = st.speed - v0, dH = st.elev - h0;
  psOut = ((v0 + st.speed) * 0.5 * dV / 9.80665 + dH) / t;
  machOut = machSum / nMach;
  FBLog::Info("mig29", "PS_RUN", {{"altM", altM}, {"machMid", machOut}, {"grossKg", grossKg},
      {"windowS", t}, {"dVms", dV}, {"dHm", dH}, {"psMs", psOut}});
  return true;
}

/* Service ceiling: the standard CONSTANT-CAS-THEN-CONSTANT-MACH climb, entered from a level
 * acceleration at low altitude. Two earlier versions of this test failed for instructive reasons and
 * both mistakes are cheap to make: a speed-hold climb entered BELOW its target speed flies downhill to
 * get it, and a climb Mach of 1.6 cannot be reached at 2,000 m at all, because M 1.6 is above the
 * aircraft's own sea-level Vmax. Holding CAS instead lets Mach rise WITH the climb, which is what a
 * real climb schedule does and why it is written that way.
 * Ceiling = where the rate of climb dies. */
bool MeasureCeiling(double climbCasKt, double climbMach, double &ceilM, double &machAtCeil) {
  std::unique_ptr<Fdm::FBFdm> f = Spawn(500.0, 400.0, 13000.0);
  if (!f) return false;
  Fdm::fb_fdm_state st{};
  f->Step(st);

  int guard = (int)(180.0 / kDt);
  while (st.cas * kMsToKt < climbCasKt && guard-- > 0) {
    f->SetControls(WingsLevelStick(st), AltHoldStick(st, 500.0), 0.0, 1.0);
    f->Step(st);
    if (f->Faulted()) return false;
  }
  if (guard <= 0) {
    FBLog::Warn("mig29", "CEILING_NO_ACCEL", {{"casKtReached", st.cas * kMsToKt},
        {"climbCasKt", climbCasKt}});
    return false;
  }

  /* The ceiling is where the STEADY climb dies, not the highest point reached: past that altitude the
   * Mach hold trades what is left of the airspeed for a zoom, and taking the apex of that zoom would
   * report 19.6 km for an aircraft whose last sustained climb ended at 17-18 (the spec asks for a
   * "zoom-free steady climb to ROC < 0.5 m/s" for exactly this reason). So the recorded altitude is
   * the one at the END of the last 20 s window that still climbed. */
  double lastClimbingAlt = st.elev, hPrev = st.elev, rocWindow = 0.0;
  int slow = 0;
  int n = (int)(1800.0 / kDt);
  for (int i = 0; i < n; i++) {
    /* Below the changeover, hold CAS; above it, hold Mach. Too fast -> climb harder, so the airframe's
     * own excess power is what sets the rate of climb. */
    double cmd = st.mach < climbMach
        ? 0.006 * (st.cas * kMsToKt - climbCasKt) - 0.035 * st.q
        : 9.0 * (st.mach - climbMach) - 0.035 * st.q;
    f->SetControls(WingsLevelStick(st), Clamp(cmd, -0.35, 0.55), 0.0, 1.0);
    f->Step(st);
    if (f->Faulted()) break;
    if (i % 2000 == 1999) {   /* every 20 s */
      rocWindow = (st.elev - hPrev) / 20.0;
      hPrev = st.elev;
      FBLog::Info("mig29", "CEILING_STEP", {{"altM", st.elev}, {"mach", st.mach},
          {"casKt", st.cas * kMsToKt}, {"rocMs", rocWindow}});
      if (rocWindow < 0.5) { if (++slow >= 2) break; }
      else { slow = 0; lastClimbingAlt = st.elev; machAtCeil = st.mach; }
    }
  }
  ceilM = lastClimbingAlt;
  FBLog::Info("mig29", "CEILING_RUN", {{"climbCasKt", climbCasKt}, {"climbMach", climbMach},
      {"ceilingM", ceilM}, {"machAtCeiling", machAtCeil}, {"finalRocMs", rocWindow}});
  return ceilM > 3000.0;
}

/* B7: the documented part-power climb — 270 kts at 83-85 % RPM gives 985-1,480 ft/min. */
bool MeasureClimbSchedule(double &rocFpm, double &n2Pct) {
  n2Pct = 84.0;
  double cmd = (n2Pct - 65.0) / 70.0;
  std::unique_ptr<Fdm::FBFdm> f = Spawn(1500.0, 270.0, 14200.0);
  if (!f) return false;
  Fdm::fb_fdm_state st{};
  f->Step(st);
  /* Hold 270 kt CALIBRATED (the documented schedule is an IAS one) by pitch, throttle fixed. */
  double vsSum = 0.0; int vsN = 0;
  for (int i = 0; i < (int)(90.0 / kDt); i++) {
    double casErrKt = st.cas * kMsToKt - 270.0;
    double pitch = Clamp(0.010 * casErrKt - 0.035 * st.q, -0.5, 0.6);
    f->SetControls(WingsLevelStick(st), pitch, 0.0, cmd);
    f->Step(st);
    if (f->Faulted()) return false;
    if (i > (int)(45.0 / kDt)) { vsSum += st.vy; vsN++; }
  }
  rocFpm = (vsSum / vsN) * kMToFt * 60.0;
  FBLog::Info("mig29", "CLIMB_SCHEDULE", {{"casKt", st.cas * kMsToKt}, {"n2Pct", n2Pct},
      {"throttleCmd", cmd}, {"rocFpm", rocFpm}, {"altEndM", st.elev}});
  return true;
}

/* ================================ Step 4: takeoff ================================ */

struct Takeoff {
  double RotateCasKt = 0.0, LiftoffCasKt = 0.0, GroundRunM = 0.0, PitchAfterDeg = 0.0;
  bool Ok = false;
};

Takeoff MeasureTakeoff(double grossKg, double rotateCasKt) {
  Takeoff t;
  std::unique_ptr<Fdm::FBFdm> f = SpawnOnGround(grossKg);
  if (!f) return t;
  Fdm::fb_fdm_state st{};
  f->SetFlap(1.0);            /* TAKEOFF position: flaps and slats extend together [DCS-EA p.57] */
  for (int i = 0; i < 500; i++) { f->SetControls(0.0, 0.0, 0.0, 0.0); f->SetWheelBrakes(1.0, 1.0); f->Step(st); }

  /* [DCS-EA p.78] hold 90 % RPM on the brakes, then release into full augmentation. */
  double holdCmd = (90.0 - 65.0) / 70.0;
  for (int i = 0; i < (int)(5.0 / kDt); i++) {
    f->SetControls(0.0, 0.0, 0.0, holdCmd);
    f->SetWheelBrakes(1.0, 1.0);
    f->Step(st);
  }

  double distM = 0.0;
  bool rotating = false, airborne = false;
  int airborneSteps = 0;
  for (int i = 0; i < (int)(90.0 / kDt); i++) {
    double casKt = st.cas * kMsToKt;
    double pitch = 0.0;
    if (!airborne && casKt >= rotateCasKt) {
      if (!rotating) { rotating = true; t.RotateCasKt = casKt; }
      /* Rotate to and hold the documented 8-10 deg takeoff attitude [DCS-EA p.78]. */
      pitch = Clamp(0.22 * (9.0 - st.pitch) - 0.040 * st.q, -0.4, 0.7);
    } else if (airborne) {
      /* HOLD the takeoff attitude rather than climbing away from it: B3 is the attitude at and just
       * after liftoff, and sampling it during a transition to a steeper climb measures the transition. */
      pitch = Clamp(0.22 * (9.0 - st.pitch) - 0.040 * st.q, -0.4, 0.7);
    }
    f->SetControls(WingsLevelStick(st), pitch, 0.0, 1.0);
    f->SetWheelBrakes(0.0, 0.0);
    f->SetNosewheelSteer(0.0);
    f->Step(st);
    if (f->Faulted()) return t;
    if (!airborne) {
      distM += st.gs * kDt;
      if (!f->GetWow() && st.elev > 0.3) {
        airborne = true;
        t.LiftoffCasKt = st.cas * kMsToKt;
        t.GroundRunM = distM;
      }
    } else if (++airborneSteps >= (int)(4.0 / kDt)) {
      t.PitchAfterDeg = st.pitch;
      t.Ok = true;
      break;
    }
  }
  FBLog::Info("mig29", "TAKEOFF", {{"grossKg", grossKg}, {"rotateCasKt", t.RotateCasKt},
      {"liftoffCasKt", t.LiftoffCasKt}, {"groundRunM", t.GroundRunM},
      {"pitchAfterDeg", t.PitchAfterDeg}, {"ok", t.Ok}});
  return t;
}

/* B10 without a landing: the trimmed angle of attack in the landing configuration at the documented
 * touchdown speed. This tests the CLmax inversion of spec §6.2 directly and without a flare
 * controller in the way. */
bool MeasureApproachAoA(double casKt, double grossKg, double &alphaDeg) {
  std::unique_ptr<Fdm::FBFdm> f = Spawn(600.0, casKt, grossKg);
  if (!f) return false;
  Fdm::fb_fdm_state st{};
  f->SetGear(1.0);
  f->SetFlap(1.0);
  f->Step(st);
  double sum = 0.0; int n = 0;
  for (int i = 0; i < (int)(40.0 / kDt); i++) {
    /* Hold BOTH altitude and speed: pitch for level, throttle for the target speed. */
    double casErr = st.cas * kMsToKt - casKt;
    double thr = Clamp(0.30 - 0.02 * casErr, 0.05, 0.95);
    f->SetControls(WingsLevelStick(st), AltHoldStick(st, 600.0), 0.0, thr);
    f->Step(st);
    if (f->Faulted()) return false;
    if (i > (int)(25.0 / kDt)) { sum += st.alphaDeg; n++; }
  }
  alphaDeg = sum / n;
  FBLog::Info("mig29", "APPROACH_AOA", {{"casKt", st.cas * kMsToKt}, {"grossKg", grossKg},
      {"alphaDeg", alphaDeg}, {"altM", st.elev}});
  return true;
}

/* ================================ Step 5: landing roll ================================ */

struct Landing {
  double TouchdownCasKt = 0.0, TouchdownAlphaDeg = 0.0, RollM = 0.0, SinkMs = 0.0;
  bool Ok = false;
};

Landing MeasureLanding(double grossKg) {
  Landing l;
  /* Start on final at the documented >= 175 kt and fly the documented profile down: approach speed,
   * then the flare at 20-30 ft AGL with the throttle closed. */
  std::unique_ptr<Fdm::FBFdm> f = Spawn(300.0, 175.0, grossKg);
  if (!f) return l;
  Fdm::fb_fdm_state st{};
  f->SetGear(1.0);
  f->SetFlap(1.0);
  f->Step(st);

  bool down = false;
  double distM = 0.0;
  int rollSteps = 0;
  for (int i = 0; i < (int)(240.0 / kDt); i++) {
    if (!down) {
      double agl = st.elev;
      /* Above the flare: descend at ~5 m/s holding the approach speed on the throttle.
       * IN THE FLARE (below 9 m = the documented 20-30 ft): close the throttle and fly the documented
       * TOUCHDOWN ATTITUDE — 11 deg AoA [DCS-EA p.79] — instead of a vertical-speed target. That is
       * both what the procedure says and what makes the float long enough to bleed the ~12 kt between
       * approach and touchdown speed; a sink-rate target flies the aircraft onto the runway at
       * approach speed, which is what the first version of this test measured. */
      double pitch;
      double thr;
      if (agl > 18.0) {
        double vsTarget = agl > 45.0 ? -5.0 : -2.0;
        pitch = Clamp(0.055 * (vsTarget - st.vy) - 0.030 * st.q, -0.4, 0.9);
        double casErr = st.cas * kMsToKt - 148.0;
        thr = Clamp(0.28 - 0.020 * casErr, 0.0, 0.9);
      } else {
        pitch = Clamp(0.18 * (11.0 - st.alphaDeg) - 0.030 * st.q, -0.3, 0.9);
        thr = 0.0;
      }
      f->SetControls(WingsLevelStick(st), pitch, 0.0, thr);
      f->Step(st);
      if (f->Faulted()) return l;
      if (f->GetWow()) {
        down = true;
        l.TouchdownCasKt = st.cas * kMsToKt;
        l.TouchdownAlphaDeg = st.alphaDeg;
        l.SinkMs = -st.vy;
      }
    } else {
      /* Rollout: idle, hold the nose up until it falls through, brakes at the documented 115 kt.
       * The chute deploys itself (deck Drag Chute channel) on weight-on-wheels below 175 kt. */
      double casKt = st.cas * kMsToKt;
      double brake = casKt < 115.0 ? 1.0 : 0.0;
      double pitch = casKt > 120.0 ? 0.25 : 0.0;
      f->SetControls(0.0, pitch, 0.0, 0.0);
      f->SetWheelBrakes(brake, brake);
      f->Step(st);
      if (f->Faulted()) return l;
      distM += st.gs * kDt;
      rollSteps++;
      if (st.gs < 1.0) { l.RollM = distM; l.Ok = true; break; }
    }
  }
  (void)rollSteps;
  FBLog::Info("mig29", "LANDING", {{"grossKg", grossKg}, {"touchdownCasKt", l.TouchdownCasKt},
      {"touchdownAlphaDeg", l.TouchdownAlphaDeg}, {"sinkMs", l.SinkMs}, {"rollM", l.RollM},
      {"ok", l.Ok}});
  return l;
}

/* ================================ Steps 8-9: g, alpha, turn, roll ================================
 * The same instantaneous-turn sweep clients/FBTestCornerSpeed.cpp runs on the F-16, for the same
 * reason: the corner speed and the achievable g are MEASURED PROPERTIES OF THE MODEL. For this
 * airframe there is additionally nothing to compare the corner speed against (spec §8.3) — but the
 * peak g and the alpha it is reached at ARE anchored (+9 g, 26 deg limiter), and since this deck
 * carries NO limiter (spec §7.3 puts SOS in FBFlightControl), what the sweep shows is what the bare
 * airframe does. */

struct SweepPoint {
  double EntryCasKt = 0.0, TurnRateDegS = 0.0, NzMean = 0.0, NzPeak = 0.0;
  double AlphaMeanDeg = 0.0, AlphaPeakDeg = 0.0;
};

bool MeasureTurnPoint(double altM, double entryCasKt, double grossKg, double alphaLimDeg, double gLim,
                      SweepPoint &out) {
  std::unique_ptr<Fdm::FBFdm> f = Spawn(altM, entryCasKt, grossKg);
  if (!f) return false;
  Fdm::fb_fdm_state st{};
  f->Step(st);

  const double kBankDeg = 85.0;
  for (int i = 0; i < (int)(6.0 / kDt) && st.roll < kBankDeg; i++) {
    f->SetControls(Clamp((kBankDeg - st.roll) / 45.0, -1.0, 1.0), 0.15, 0.0, 1.0);
    f->Step(st);
    if (f->Faulted()) return false;
  }
  if (st.roll < kBankDeg - 8.0) return false;

  /* Let the limiter settle onto whichever limit bites, THEN measure — the transient into the pull is
   * not the turn rate. */
  PullLimiter lim;
  for (int i = 0; i < (int)(3.0 / kDt); i++) {
    f->SetControls(Clamp((kBankDeg - st.roll) / 45.0, -1.0, 1.0),
                   lim.Stick(st, alphaLimDeg, gLim, kDt), 0.0, 1.0);
    f->Step(st);
    if (f->Faulted()) return false;
  }

  double wrap = 0.0, prevPsi = st.yaw, nzSum = 0.0, nzPeak = 0.0, aSum = 0.0, aPeak = 0.0;
  int n = (int)(4.0 / kDt);
  for (int i = 0; i < n; i++) {
    f->SetControls(Clamp((kBankDeg - st.roll) / 45.0, -1.0, 1.0),
                   lim.Stick(st, alphaLimDeg, gLim, kDt), 0.0, 1.0);
    f->Step(st);
    if (f->Faulted()) return false;
    double d = st.yaw - prevPsi;
    while (d > 180.0) d -= 360.0;
    while (d < -180.0) d += 360.0;
    wrap += d;
    prevPsi = st.yaw;
    nzSum += st.nz;
    aSum += st.alphaDeg;
    if (st.nz > nzPeak) nzPeak = st.nz;
    if (st.alphaDeg > aPeak) aPeak = st.alphaDeg;
  }
  out.EntryCasKt = entryCasKt;
  out.TurnRateDegS = std::fabs(wrap) / 4.0;
  out.NzMean = nzSum / n;
  out.NzPeak = nzPeak;
  out.AlphaMeanDeg = aSum / n;
  out.AlphaPeakDeg = aPeak;
  return true;
}

bool MeasureRollRate(double entryCasKt, double grossKg, double &peakDegS) {
  std::unique_ptr<Fdm::FBFdm> f = Spawn(3048.0, entryCasKt, grossKg);   /* 10,000 ft */
  if (!f) return false;
  Fdm::fb_fdm_state st{};
  f->Step(st);
  for (int i = 0; i < (int)(2.0 / kDt); i++) { f->SetControls(0.0, 0.0, 0.0, 0.6); f->Step(st); }
  peakDegS = 0.0;
  for (int i = 0; i < (int)(4.0 / kDt); i++) {
    f->SetControls(1.0, 0.0, 0.0, 0.6);
    f->Step(st);
    if (f->Faulted()) return false;
    double p = std::fabs(st.p);
    if (p > peakDegS) peakDegS = p;
  }
  return true;
}

} // namespace

int main() {
  Clients::FBStdoutLogSink sink;
  FBLog::SetSink(&sink);
  FBLog::SetLevel(FBLogLevel::Info);

  {   /* Does it load and trim at all? Everything below is meaningless if not. */
    std::unique_ptr<Fdm::FBFdm> probe = Spawn(5000.0, 350.0, 14200.0);
    if (!probe) {
      FBLog::Error("mig29", "RESULT", {{"result", "LOAD_FAILED"}});
      return 2;
    }
  }

  int failed = 0;

  /* ---- Step 1 ---- */
  if (!MeasureGroundAndMass()) { FBLog::Error("mig29", "STEP_FAILED", {{"step", 1.0}}); failed++; }

  /* ---- Steps 2-3: the speed/altitude envelope ---- */
  double mach = 0.0, kmh = 0.0;
  if (MeasureVmax(200.0, 500.0, 13000.0, 420.0, mach, kmh))
    Record("A2", "Vmax at sea level [DCS-FM p.14]", 1500.0, kmh, "km/h", true);
  else { Record("A2", "Vmax at sea level [DCS-FM p.14]", 1500.0, 0.0, "km/h", false); failed++; }

  if (MeasureVmax(11000.0, 400.0, 13000.0, 900.0, mach, kmh))
    Record("A1", "Vmax at 11 km [DCS-FM p.14]", 2450.0, kmh, "km/h", true);
  else { Record("A1", "Vmax at 11 km [DCS-FM p.14]", 2450.0, 0.0, "km/h", false); failed++; }

  double ps = 0.0, psMach = 0.0;
  if (MeasurePs(200.0, 0.9, 13000.0, ps, psMach))
    Record("A4", "Ps at SL, M 0.9, light [DCS-FM p.14, reinterpreted per spec §8.1]",
           330.0, ps, "m/s", true);
  else { Record("A4", "Ps at SL, M 0.9, light", 330.0, 0.0, "m/s", false); failed++; }

  double ceil = 0.0, ceilMach = 0.0;
  if (MeasureCeiling(500.0, 1.60, ceil, ceilMach))
    Record("A3", "service ceiling [DCS-FM p.14]", 18000.0, ceil, "m", true);
  else { Record("A3", "service ceiling [DCS-FM p.14]", 18000.0, 0.0, "m", false); failed++; }

  double rocFpm = 0.0, n2 = 0.0;
  if (MeasureClimbSchedule(rocFpm, n2))
    Record("B7", "ROC at 270 kt, 84% RPM [DCS-EA p.77: 985-1480 fpm]", 1230.0, rocFpm, "ft/min", true);
  else { Record("B7", "ROC at 270 kt, 84% RPM", 1230.0, 0.0, "ft/min", false); failed++; }

  /* ---- Step 4: takeoff ---- */
  Takeoff to = MeasureTakeoff(15300.0, 130.0);
  Record("B1", "rotation speed [DCS-EA p.78: 125-135 kt]", 130.0, to.RotateCasKt, "kt", to.Ok);
  Record("B2", "liftoff speed [DCS-EA p.78: 140-150 kt]", 145.0, to.LiftoffCasKt, "kt", to.Ok);
  Record("B3", "takeoff pitch attitude [DCS-EA p.78: 8-10 deg]", 9.0, to.PitchAfterDeg, "deg", to.Ok);
  Record("B8", "takeoff ground run at normal weight [T3]", 250.0, to.GroundRunM, "m", to.Ok);
  if (!to.Ok) failed++;

  double approachAlpha = 0.0;
  if (MeasureApproachAoA(140.0, 12000.0, approachAlpha))
    Record("B10", "AoA in landing config at 140 kt, 12 t [DCS-EA p.79: 11 deg]",
           11.0, approachAlpha, "deg", true);
  else { Record("B10", "AoA in landing config at 140 kt", 11.0, 0.0, "deg", false); failed++; }

  /* ---- Step 5: landing roll with the chute ---- */
  Landing la = MeasureLanding(12000.0);
  Record("B9", "landing roll with brake chute [T3]", 600.0, la.RollM, "m", la.Ok);
  Record("B10-TD", "touchdown speed [DCS-EA p.79: ~140 kt]", 140.0, la.TouchdownCasKt, "kt", la.Ok);
  if (!la.Ok) failed++;

  /* ---- Steps 8-9: what the bare airframe does in a maximum pull ---- */
  /* Two sweeps, because the turn-rate anchor has no altitude on it. 5,000 m is the reference point
   * clients/FBTestCornerSpeed.cpp uses on the F-16, so the corner speed is comparable to that measured
   * number; 1,000 m is where an instantaneous-turn figure like the T4 28 deg/s is normally quoted, and
   * the same limits there buy a higher rate purely because the true airspeed is lower for the same
   * calibrated one. Reporting one without the other would be picking the flattering altitude. */
  const double kSweepAltM = 5000.0;
  SweepPoint pts[24];
  int count = 0, departed = 0;
  double bestRate = 0.0, peakNz = 0.0, peakAlpha = 0.0;
  for (double cas = 240.0; cas <= 620.0 + 1e-9 && count < 24; cas += 40.0) {
    SweepPoint p;
    if (!MeasureTurnPoint(kSweepAltM, cas, 13000.0, 26.0, 9.0, p)) {
      FBLog::Warn("mig29", "TURN_POINT_FAILED", {{"entryCasKt", cas}});
      continue;
    }
    /* A point that ran away past 35 deg is a DEPARTURE, not a turn: an 85 deg bank demands more lift
     * than the airframe has at low speed, the limiter loses it, and what follows is a tumble whose
     * "turn rate" is a rotation about the wrong axis. Logged as a measured property of the airframe —
     * where its departure boundary is — and excluded from the turn statistics. */
    bool dep = p.AlphaPeakDeg > 35.0;
    FBLog::Info("mig29", "TURN_POINT", {{"entryCasKt", p.EntryCasKt},
        {"turnRateDegS", p.TurnRateDegS}, {"nzMean", p.NzMean}, {"nzPeak", p.NzPeak},
        {"alphaMeanDeg", p.AlphaMeanDeg}, {"alphaPeakDeg", p.AlphaPeakDeg}, {"departed", dep}});
    if (dep) { departed++; continue; }
    pts[count++] = p;
    if (p.TurnRateDegS > bestRate) bestRate = p.TurnRateDegS;
    if (p.NzMean > peakNz) peakNz = p.NzMean;
    if (p.AlphaMeanDeg > peakAlpha) peakAlpha = p.AlphaMeanDeg;
  }
  FBLog::Info("mig29", "TURN_SWEEP", {{"points", (double)count}, {"departedPoints", (double)departed}});
  if (count >= 3) {
    int corner = -1;
    for (int i = 0; i < count && corner < 0; i++)
      if (pts[i].TurnRateDegS >= 0.97 * bestRate) corner = i;
    FBLog::Info("mig29", "CORNER", {{"cornerCasKt", corner >= 0 ? pts[corner].EntryCasKt : 0.0},
        {"cornerTurnRateDegS", corner >= 0 ? pts[corner].TurnRateDegS : 0.0},
        {"bestTurnRateDegS", bestRate}, {"points", (double)count}});
    Record("A9-5km", "instantaneous turn rate at the documented limits, 5,000 m [T4, no altitude given]",
           28.0, bestRate, "deg/s", true, false);
    /* A5/A6 ask whether the AIRFRAME can reach the documented limits when a limiter commands them —
     * not what an unlimited pull does, which on a deck with no SOS is a tumble and measures nothing. */
    Record("A5", "highest sustained normal load reached under the 9 g limiter [DCS-FM p.14: +9]",
           9.0, peakNz, "g", true);
    Record("A6", "highest sustained alpha reached under the 26 deg limiter [T4 + DCS-EA p.19]",
           26.0, peakAlpha, "deg", true);
  } else { failed++; }

  double bestLowRate = 0.0;
  int lowCount = 0;
  for (double cas = 240.0; cas <= 560.0 + 1e-9; cas += 40.0) {
    SweepPoint p;
    if (!MeasureTurnPoint(1000.0, cas, 13000.0, 26.0, 9.0, p)) continue;
    bool dep = p.AlphaPeakDeg > 35.0;
    FBLog::Info("mig29", "TURN_POINT_LOW", {{"entryCasKt", p.EntryCasKt},
        {"turnRateDegS", p.TurnRateDegS}, {"nzMean", p.NzMean}, {"alphaMeanDeg", p.AlphaMeanDeg},
        {"departed", dep}});
    if (dep) continue;
    lowCount++;
    if (p.TurnRateDegS > bestLowRate) bestLowRate = p.TurnRateDegS;
  }
  Record("A9", "instantaneous turn rate at the documented limits, 1,000 m [T4 only, weakest anchor]",
         28.0, bestLowRate, "deg/s", lowCount >= 3, false);

  double roll450 = 0.0, roll300 = 0.0;
  bool rollOk = MeasureRollRate(450.0, 13000.0, roll450) && MeasureRollRate(300.0, 13000.0, roll300);
  FBLog::Info("mig29", "ROLL", {{"peakDegS450Kt", roll450}, {"peakDegS300Kt", roll300}});
  /* [GAP] at every tier (spec §8.3): there is nothing to compare this against, so it is recorded as
   * a MEASURED MODEL PROPERTY with no target. Target 0 marks "no anchor exists". */
  Record("GAP-ROLL", "peak roll rate at 450 kt (NO documented anchor exists)", 0.0, roll450,
         "deg/s", rollOk, false);

  int missing = 0;
  for (int i = 0; i < gAnchorCount; i++)
    if (gAnchors[i].Must && !gAnchors[i].Measured_ok) missing++;

  FBLog::Info("mig29", "RESULT", {{"result", missing == 0 ? "MEASURED" : "INCOMPLETE"},
      {"anchors", (double)gAnchorCount}, {"unmeasuredMust", (double)missing},
      {"stepFailures", (double)failed}});
  return missing == 0 ? 0 : 1;
}
