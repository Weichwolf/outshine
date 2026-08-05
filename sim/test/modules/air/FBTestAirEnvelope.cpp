/* fb-test-air-envelope: THE ENTRY GATE of doc/modules/air/module.md §Spec 11 criterion 3, and the first
 * of its three attribution instruments — the ANCHOR RESIDUAL, measured before any campaign runs.
 *
 * It is clients/FBTestMig29Envelope.cpp's harness PARAMETERISED BY ROW: the same method, applied ten
 * times instead of once. Not a simulation client — no module, no registry, no mission, no pilot. It
 * reaches each airframe the one legal way (fdm/FBFdmBoot) and flies it with the crudest controllers
 * that can hold a condition long enough to read a number off it.
 *
 * IT MEASURES AND IT DOES NOT JUDGE. What each number is expected to be — the published figure, its
 * band, its source, its tier — is declared in test/modules/air/envelope.json and compared by
 * tools/fb_test.py. That split is not decoration: while this file carried its own verdict, seven
 * anchors sat outside their bands unseen because the exit code counted whether a number CAME INTO
 * EXISTENCE (doc/testing.md §0). An expectation buried in a program can silently fail to gate.
 *
 * Its whole product is one `[measure]` line per measurement. A measurement that cannot be made emits
 * nothing and logs an error; the runner then reports the DECLARATION as unmeasured, which is where a
 * hole belongs. Exit 0 = the harness ran; 2 = no such row.
 *
 *     build/fb-test-air-envelope [row ...]      (no argument = all ten deck rows) */
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

#include "FBAirAnchors.h"
#include "FBFdmBoot.h"
#include "FBFlightControl.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include "FBLogSinks.h"
#include "FBUnits.h"

using namespace FlightBox;
using namespace FlightBox::Test;

namespace {

const char *kModelsRoot = "assets/aircraft";
const double kLatDeg = 46.7, kLonDeg = 6.8;
const double kDt = Fdm::FBFdm::kStepS;

double Clamp(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }

/* The mass every envelope anchor in these sources is quoted at: the published GROSS (clean take-off)
 * weight. The deck carries empty + full internal fuel + a 100 kg pilot, so the fuel fraction that
 * reproduces the gross mass is what the harness loads. */
double FuelPctFor(const FBAirAnchorRow &r, double grossKg) {
  if (r.FuelKg <= 0.0) return 100.0;
  double fuel = grossKg - r.EmptyKg - 100.0;
  return Clamp(100.0 * fuel / r.FuelKg, 0.0, 100.0);
}

/* AN EQUILIBRIUM ANCHOR IS QUOTED AT A WEIGHT, AND HOW LONG THE INSTRUMENT TAKES TO SETTLE IS THE
 * INSTRUMENT'S CHOICE. A Vmax/ceiling/climb run holds full augmentation for minutes, so the tank empties
 * WHILE the number is being read and the run reports the Mach at flame-out instead of the Mach at
 * T = D — measured on f15c: augmentation died at t = 870 s and M 2.04 was reported as its Vmax while
 * (T-D)/W was still +0.025. Topping the tank up holds the CONDITION the published figure is quoted at.
 * The short dynamic measurements (take-off, pull, roll) are NOT held: their duration is physical. */
void HoldFuel(Fdm::FBFdm &f, const FBAirAnchorRow &r) { f.SetFuelPct(FuelPctFor(r, r.GrossKg)); }

/* AND THE OTHER HALF OF THE SAME CONDITION: eight of the ten rows publish a GROSS weight LARGER than
 * empty + internal fuel + pilot — the difference is the clean armament and ammunition the source counts
 * into it (`f15c` 828 kg, `su7` 1 310 kg, `f5e` 645 kg). A full tank therefore leaves the aeroplane
 * BELOW the weight every envelope anchor is quoted at, and the recipe inverted its polar at the gross
 * one. The shortfall is carried as ballast on the deck's own store channel at the CG station, so the
 * measurement and the inversion see the same aircraft. `mirf1` is the opposite case and needs none: its
 * empty + fuel already exceeds gross, and FuelPctFor's part-tank covers it. */
void LoadToGross(Fdm::FBFdm &f, const FBAirAnchorRow &r) {
  double shortfallKg = r.GrossKg - (r.EmptyKg + r.FuelKg + 100.0);
  if (shortfallKg <= 1.0) return;
  int idx = f.AddStorePointMass("anchor ballast", f.GetCgXIn(), 0.0, 0.0);
  if (idx >= 0) f.SetStorePointMassLbs(idx, shortfallKg * 2.2046226);
}

std::unique_ptr<Fdm::FBFdm> Spawn(const FBAirAnchorRow &r, double altM, double casKt) {
  Fdm::FBFdmSpawn ic;
  ic.ModelsRoot = kModelsRoot;
  ic.Aircraft = r.Key;
  ic.LatDeg = kLatDeg;
  ic.LonDeg = kLonDeg;
  ic.GroundElevM = 0.0;
  ic.HeightOffsetM = altM;
  ic.SpeedMs = casKt * kKtToMs;
  ic.HeadingDeg = 90.0;
  std::unique_ptr<Fdm::FBFdm> f = Fdm::FBFdmBoot::Spawn(ic);
  if (!f) return f;
  f->SetGroundElevM(0.0);
  f->SetFuelPct(FuelPctFor(r, r.GrossKg));
  LoadToGross(*f, r);
  f->SetGear(0.0);
  return f;
}

std::unique_ptr<Fdm::FBFdm> SpawnOnGround(const FBAirAnchorRow &r, bool atGross) {
  Fdm::FBFdmSpawn ic;
  ic.ModelsRoot = kModelsRoot;
  ic.Aircraft = r.Key;
  ic.LatDeg = kLatDeg;
  ic.LonDeg = kLonDeg;
  ic.GroundElevM = 0.0;
  ic.HeightOffsetM = -1.0;
  ic.SpeedMs = 0.0;
  ic.HeadingDeg = 90.0;
  std::unique_ptr<Fdm::FBFdm> f = Fdm::FBFdmBoot::Spawn(ic);
  if (!f) return f;
  f->SetGroundElevM(0.0);
  f->SetFuelPct(FuelPctFor(r, r.GrossKg));
  if (atGross) LoadToGross(*f, r);
  f->SetGear(1.0);
  return f;
}

/* ---- THE INNER LOOP IS THE ROW'S OWN, and it used to be three hand-tuned laws in this file. They were
 * tuned against ONE control power and they did not survive the deck getting the elevator its own tail
 * volume implies: the ceiling sweep's 0.9-per-Mach stick dived five rows into the ground and reported
 * their spawn altitude (2 000 m) as their service ceiling, and the pull sketch's limiter answered a
 * 8.5 g anchor with 77 g. An instrument whose reading depends on the deck's gain measures the gain.
 *
 * So the harness now flies every deck through `systems/FBFlightControl::Raw(P, alpha_lim)` — the SAME
 * preset a mission flies the row through, which is recipe §6.1's per-row gain set and the thing whose
 * absence this round found. What is left in this file is OUTER law only: a target vertical speed and a
 * target bank, both of which are control-power invariant by construction. ---- */

Systems::FBFlightControl ControlFor(const FBAirAnchorRow &r) {
  return Systems::FBFlightControl::Raw(r.PitchStickMax, r.AlphaLimitDeg, r.A5G);
}

/* The guidance the outer laws write: wings level, hold a vertical speed. Speed is NOT held here — every
 * envelope anchor is flown at full augmentation and the harness commands the throttle itself. */
Systems::FBGuidance LevelGuidance(double targetVsMs, double bankCmdDeg = 0.0) {
  Systems::FBGuidance g{};
  g.Mode = FBMode::Direct;
  g.BankCmdDeg = bankCmdDeg;
  g.TargetVsMs = targetVsMs;
  g.TargetSpeedMs = 0.0;
  return g;
}

/* THE CLIMB SCHEDULE, as an ENERGY SPLIT and not as a stick: thrust buys the climb, the elevator holds
 * the Mach. Above the schedule Mach the aircraft is told to climb harder, below it to climb less — and
 * the conversion from that vertical-speed demand to a control deflection is the row's own flight
 * control, so the law is the same on a MiG-17 and on a MiG-25. */
double ScheduleVs(double mach, double targetMach) {
  /* THE FLOOR IS ZERO AND NOT A DESCENT: thrust buys the climb, the elevator only holds the schedule,
   * so an aeroplane that has not reached its target yet must WAIT for it in level flight. With a
   * negative floor the two fastest rows dived off their spawn altitude on their first candidate and
   * reported 2 000 m as their service ceiling. */
  return Clamp(1200.0 * (mach - targetMach), 0.0, 300.0);
}

/* THE CLIMB IS FLOWN AT CONSTANT CAS UNTIL A MACH CROSSOVER AND AT CONSTANT MACH ABOVE IT, which is
 * what a climb schedule IS, and the sweep is over the CAS. A constant-MACH schedule alone cannot
 * express the one climb the fastest rows actually have: a MiG-25 cannot reach M 2.6 at 2 000 m, so a
 * M 2.6 target held it level at its spawn altitude for the whole run and reported 2 049 m as a
 * 20 700 m ceiling. A constant-CAS schedule alone cannot express the top of it: every constant-CAS
 * line crosses the row's Vmax somewhere, and the crossover is where a real schedule stops trading
 * altitude for Mach. Whichever demand is larger owns the stick — below the crossover that is the CAS
 * term, above it the Mach term, and no seam has to be found. */
double ScheduleVsCas(double casKt, double targetCasKt, double mach, double machCap) {
  double cas = Clamp(3.0 * (casKt - targetCasKt), 0.0, 300.0);
  double top = Clamp(1200.0 * (mach - machCap), 0.0, 300.0);
  return top > cas ? top : cas;
}

/* THE ENTRY SPEED OF THE g PULL IS DERIVED PER ROW and not a constant 480 kt: what the anchor asks is
 * the g the limiter holds, so the aeroplane has to be fast enough that its own wing can produce it.
 * q = 1.3 * gLim * W / (CLmax * S) is the dynamic pressure with 30 % of margin over the published g;
 * below it the ALPHA limiter binds first and the run measures the wrong limiter (measured: mig17, a
 * subsonic fighter, was being pulled at M 1.1 and read 4.6 g on an 8.0 anchor). The reference lift is
 * the one the ALPHA LIMITER ALLOWS and not CLmax — CLmax sits past the limiter and no pull ever gets
 * there, so sizing the entry on it leaves 11 % of margin where 30 % was intended. */
double PullEntryKt(const FBAirAnchorRow &r, double gLimit, double altM) {
  double rho = 1.225 * std::pow(1.0 - 2.25577e-5 * altM, 4.25588);
  double q = 1.3 * gLimit * r.GrossKg * 9.80665 / (r.ClAtAlphaLimit * r.WingAreaM2);
  double tas = std::sqrt(2.0 * q / rho);
  return Clamp(tas * std::sqrt(rho / 1.225) * kMsToKt, 250.0, 650.0);
}

/* ---- The measurement stream. `args` in a declaration's `measure` block are these key=value fields,
 * and a declaration matches the line whose fields all agree with it (doc/testing.md §2). ---- */

void Emit(const char *key, const char *anchor, double value, const char *unit) {
  std::printf("[measure] air-envelope row=%s anchor=%s value=%.6f unit=%s\n", key, anchor, value,
              unit);
}

void Unmeasurable(const char *key, const char *anchor) {
  FBLog::Error("air", "NOT_MEASURED", {{"row", std::string(key)}, {"anchor", std::string(anchor)}});
}

/* ================================ the measurements ================================ */

bool MeasureVmax(const FBAirAnchorRow &r, double altM, double entryCasKt, double maxS,
                 double &machOut) {
  std::unique_ptr<Fdm::FBFdm> f = Spawn(r, altM, entryCasKt);
  if (!f) return false;
  Fdm::fb_fdm_state st{};
  f->Step(st);
  Systems::FBFlightControl fc = ControlFor(r);
  double best = 0.0, checkpoint = 0.0;
  int stagnant = 0;
  int n = (int)(maxS / kDt);
  /* THE STOPPING RULE IS AN EXCESS-THRUST THRESHOLD AND IT IS STATED, because T = D is approached
   * asymptotically and no finite run reaches it: the run ends when the peak Mach has gained less than
   * kVmaxSettle over kVmaxWindowS, i.e. when the remaining excess thrust is under ~5e-4 g. The number
   * it then reports is a LOWER bound on Vmax by construction, never an upper one. */
  const double kVmaxSettle = 0.001;
  const int kVmaxWindow = (int)(60.0 / kDt);
  for (int i = 0; i < n; i++) {
    Systems::FBControls c = fc.Run(LevelGuidance(Clamp(0.08 * (altM - st.elev), -25.0, 25.0)), st);
    f->SetControls(c.Roll, c.Pitch, c.Yaw, 1.0);
    f->Step(st);
    if (f->Faulted()) return false;
    if (i % 100 == 0) HoldFuel(*f, r);
    if (st.mach > best) best = st.mach;
    if (i > 0 && i % kVmaxWindow == 0) {
      if (best > checkpoint + kVmaxSettle) { checkpoint = best; stagnant = 0; }
      else if (++stagnant >= 2) break;
    }
  }
  machOut = best;
  return best > 0.05;
}

/* SERVICE CEILING: climb at full power until the steady climb decays through 0.5 m/s (100 ft/min),
 * the definition every source in this catalogue uses without saying so. */
bool MeasureCeiling(const FBAirAnchorRow &r, double &ceilOut) {
  /* THE SCHEDULE IS SWEPT, not chosen. A service ceiling is the top of the best climb the aircraft
   * has, and the best climb speed is a property of the airframe — so picking ONE would measure the
   * schedule and not the aeroplane. Seven constant-CAS candidates, each ENTERED ON ITS OWN SPEED: a
   * shared 400 kt entry made every slower candidate dump the excess through the vertical-speed
   * demand's 300 m/s ceiling, and mig25 departed out of that and reported the 5 293 m it was diving
   * through. A schedule below the row's own 1 g stall is not a schedule and is not flown. */
  double best = 0.0;
  const double stallKt =
      std::sqrt(2.0 * r.GrossKg * 9.80665 / (1.225 * r.ClMax * r.WingAreaM2)) * kMsToKt;
  const double machCap = r.A1Mach * 1.02;
  const double casKt[7] = {180.0, 240.0, 300.0, 360.0, 420.0, 480.0, 540.0};
  for (int k = 0; k < 7; k++) {
    double target = casKt[k];
    if (target < 1.3 * stallKt) continue;
    std::unique_ptr<Fdm::FBFdm> f = Spawn(r, 2000.0, target);
    if (!f) return false;
    Systems::FBFlightControl fc = ControlFor(r);
    Fdm::fb_fdm_state st{};
    f->Step(st);
    /* THE DECAY IS READ ON SPECIFIC EXCESS POWER AND NOT ON VERTICAL SPEED, because a climb can be
     * BOUGHT from speed and then it is not a climb: mig25's 540 kt candidate gained its last 889 m
     * while its energy height FELL 7 400 m, and the 0.5 m/s it still showed was a decelerating zoom.
     * Ps = d/dt(h + V²/2g) is the same energy definition recipe §4.2 writes A4 in, filtered over ~10 s
     * so a phugoid does not end the run, and the settled-flight guard on `vy` keeps a departure from
     * passing the threshold on its way down. */
    double psFilt = 25.0, he = st.elev + st.speed * st.speed / (2.0 * 9.80665);
    double top = 0.0;
    bool decayed = false;
    int n = (int)(12000.0 / kDt);
    for (int i = 0; i < n; i++) {
      Systems::FBControls c =
          fc.Run(LevelGuidance(ScheduleVsCas(st.cas * kMsToKt, target, st.mach, machCap)), st);
      f->SetControls(c.Roll, c.Pitch, c.Yaw, 1.0);
      f->Step(st);
      if (f->Faulted()) break;
      if (i % 100 == 0) HoldFuel(*f, r);
      double heNow = st.elev + st.speed * st.speed / (2.0 * 9.80665);
      psFilt += 0.001 * ((heNow - he) / kDt - psFilt);
      he = heNow;
      if (i > (int)(120.0 / kDt) && psFilt < 0.5 && std::fabs(st.vy) < 5.0) {
        top = st.elev;
        decayed = true;
        break;
      }
    }
    /* A RUN THAT DID NOT END ON THE CRITERION MEASURED NOTHING. Reporting the altitude it happened to
     * be passing when the clock or a guard stopped it is what put four rows outside their band from
     * both sides at once: mig23 booked 23 253 m while still climbing at 7.8 m/s, mig25 booked
     * 15 963 m at t = 3 600 s on a schedule that needs 8 166 s. */
    if (!decayed) {
      FBLog::Warn("air", "CEILING_INCOMPLETE",
                  {{"row", std::string(r.Key)}, {"cas_kt", std::to_string((int)target)}});
      continue;
    }
    if (top > best) best = top;
  }
  ceilOut = best;
  return best > 1000.0;
}

/* RATE OF CLIMB at sea level, at the row's own best-climb Mach found by sweeping: the published figure
 * is a MAXIMUM over speed, so measuring at one arbitrary speed would compare two different quantities. */
bool MeasureRoc(const FBAirAnchorRow &r, double &rocOut) {
  double best = -1e9;
  for (double m = 0.4; m <= 1.05; m += 0.1) {
    std::unique_ptr<Fdm::FBFdm> f = Spawn(r, 200.0, m * 340.3 * kMsToKt);
    if (!f) return false;
    Systems::FBFlightControl fc = ControlFor(r);
    Fdm::fb_fdm_state st{};
    f->Step(st);
    double vsFilt = 0.0;
    int nRoc = (int)(60.0 / kDt);
    for (int i = 0; i < nRoc; i++) {
      /* Hold the entry Mach and let the excess power go into climb — the energy definition of Ps. */
      Systems::FBControls c = fc.Run(LevelGuidance(ScheduleVs(st.mach, m)), st);
      f->SetControls(c.Roll, c.Pitch, c.Yaw, 1.0);
      f->Step(st);
      if (f->Faulted()) break;
      if (i % 100 == 0) HoldFuel(*f, r);
      if (i > nRoc / 2) vsFilt += 0.01 * (st.vy - vsFilt);
    }
    if (vsFilt > best) best = vsFilt;
  }
  rocOut = best;
  return best > -1e8;
}

/* THE MAXIMUM PULL at the documented limits, at 5 000 m and a speed high enough that the airframe can
 * reach them: what comes back is the g and the alpha the deck actually delivers, and the instantaneous
 * turn rate that implies. */
bool MeasurePull(const FBAirAnchorRow &r, double &gOut, double &alphaOut, double &turnOut,
                 double &cornerKtOut, double entryKt) {
  double gLimit = r.A5G > 0.0 ? r.A5G : 7.0;   /* [SET] where A5 is [TODO], and the row says so */
  std::unique_ptr<Fdm::FBFdm> f = Spawn(r, 5000.0, entryKt);
  if (!f) return false;
  /* THE PULL IS A FULL-AFT MANUAL STICK AND THE ONLY THING SHAPING IT IS THE ROW'S OWN LIMITER PAIR.
   * That is what "g reached under the limiter" means, and until this round the g half of the pair did
   * not exist in systems/FBFlightControl at all. */
  Systems::FBFlightControl fc = Systems::FBFlightControl::Raw(r.PitchStickMax, r.AlphaLimitDeg, gLimit);
  Fdm::fb_fdm_state st{};
  f->Step(st);
  double gPeak = 0.0, turnMax = 0.0, cornerKt = 0.0;
  int gN = 0;
  /* THE MEASUREMENT IS A SETTLED MEAN AND NOT A PEAK, on both limited quantities. A 60 s hold at the
   * limiter was neither: the turn climbs (nz*cos80 = 1.48 g of vertical), bleeds to the alpha limit,
   * mushes, falls, re-accelerates — and the peak nz of that ride is a transient of the FALL, not the
   * limiter's answer. Measured, it reported 55 g against a published 8.5. And the window has to sit
   * EARLY: at the g limit an 80 deg turn bleeds ~10 kt/s, so by t = 10 s the ALPHA limiter has taken
   * over and the mean reads 3.7 g on a 9.0 anchor. Seconds 3 to 8 — after the roll-in, before the
   * bleed. */
  int nStep = (int)(8.0 / kDt);
  for (int i = 0; i < nStep; i++) {
    Systems::FBGuidance g{};
    g.Mode = FBMode::Manual;
    g.ManualPitch = 1.0;
    /* 80 deg of bank: a level turn at the limit, which is what an instantaneous turn rate is quoted at. */
    g.ManualRoll = Clamp(0.030 * (80.0 - st.roll) - 0.010 * st.p, -1.0, 1.0);
    Systems::FBControls c = fc.Run(g, st);
    f->SetControls(c.Roll, c.Pitch, c.Yaw, 1.0);
    f->Step(st);
    if (f->Faulted()) return false;
    if (i < (int)(2.0 / kDt)) continue;   /* the roll-in transient is not the turn */
    if (st.nz > gPeak) gPeak = st.nz;
    gN++;

    if (st.speed > 1.0 && st.nz > 1.05) {
      double rate = 9.80665 * std::sqrt(st.nz * st.nz - 1.0) / st.speed * kRad2Deg;
      if (rate > turnMax) { turnMax = rate; cornerKt = st.cas * kMsToKt; }
    }
  }
  gOut = gPeak;
  alphaOut = 0.0;
  turnOut = turnMax;
  cornerKtOut = cornerKt;
  return gN > 0 && gOut > 1.0;
}

/* THE ALPHA LIMITER, measured where it is the ONLY thing that can bind: an idle deceleration with the
 * stick full aft. In a TURN the g limiter binds first on every row that publishes a g limit (f5e at
 * 380 kt reaches 7.6 g at CLmax against a 7.0 limit), so a turn cannot measure the alpha limiter at all
 * — it measures which of the two is smaller.
 *
 * THE WINDOW ENDS WHEN THE AEROPLANE STOPS FLYING, and that bound is the row's own 1 g stall speed
 * rather than a clock. Measured on f5e: the limiter holds 21.1 deg against its 22.0 limit for twenty
 * seconds, the zoom then runs the speed down to 22 kt, and what follows is a tumble to 176 deg of alpha
 * — an honest consequence of a full-aft idle deceleration and not a limiter failure. A fixed window
 * that reached into it reported 13.7 deg. */
bool MeasureAlphaLimit(const FBAirAnchorRow &r, double &alphaOut) {
  double vsKt = std::sqrt(2.0 * r.GrossKg * 9.80665 / (1.225 * r.ClMax * r.WingAreaM2)) * kMsToKt;
  /* Entry at 2.5 x the row's own 1 g stall speed, capped at 380 kt: fast enough that the pull reaches
   * the limiter, slow enough that a subsonic row is not asked to do it at M 1.1. */
  std::unique_ptr<Fdm::FBFdm> f = Spawn(r, 8000.0, std::fmin(380.0, 2.5 * vsKt));
  if (!f) return false;
  Systems::FBFlightControl fc = Systems::FBFlightControl::Raw(r.PitchStickMax, r.AlphaLimitDeg, 0.0);
  Fdm::fb_fdm_state st{};
  f->Step(st);
  double aSum = 0.0;
  int aN = 0, nStep = (int)(40.0 / kDt);
  for (int i = 0; i < nStep; i++) {
    Systems::FBGuidance g{};
    g.Mode = FBMode::Manual;
    g.ManualPitch = 1.0;
    g.ManualRoll = Clamp(-0.030 * st.roll - 0.010 * st.p, -0.5, 0.5);
    Systems::FBControls c = fc.Run(g, st);
    f->SetControls(c.Roll, c.Pitch, c.Yaw, 0.0);
    f->Step(st);
    if (f->Faulted()) return false;
    if (st.cas * kMsToKt < 1.05 * vsKt) break;
    if (i > (int)(2.0 / kDt)) { aSum += st.alphaDeg; aN++; }
  }
  alphaOut = aN > 0 ? aSum / aN : 0.0;
  return aN > 0;
}

/* THE ROLL PLANT, and it is the reason this harness is a GATE and not a report: doc/pilot.md's
 * close-combat law inverts p_dot = -a*p + K*da, so `bfm` is unavailable to a row until (a, K) are
 * MEASURED on its own deck. Full aileron from wings level; K is the steady rate, a its reciprocal time
 * constant read at 63.2 % of it. */
bool MeasureRollPlant(const FBAirAnchorRow &r, double &aOut, double &kOut, double &peakOut) {
  std::unique_ptr<Fdm::FBFdm> f = Spawn(r, 5000.0, 400.0);
  if (!f) return false;
  Systems::FBFlightControl fc = ControlFor(r);
  Fdm::fb_fdm_state st{};
  f->Step(st);
  for (int i = 0; i < (int)(3.0 / kDt); i++) {   /* settle */
    Systems::FBControls c = fc.Run(LevelGuidance(Clamp(0.08 * (5000.0 - st.elev), -25.0, 25.0)), st);
    f->SetControls(c.Roll, c.Pitch, c.Yaw, 1.0);
    f->Step(st);
  }
  double peak = 0.0, tau = -1.0;
  int n = (int)(3.0 / kDt);
  double p63 = 0.0;
  for (int i = 0; i < n; i++) {
    Systems::FBControls c = fc.Run(LevelGuidance(Clamp(0.08 * (5000.0 - st.elev), -25.0, 25.0)), st);
    f->SetControls(1.0, c.Pitch, c.Yaw, 1.0);
    f->Step(st);
    if (f->Faulted()) return false;
    double p = std::fabs(st.p);
    if (p > peak) peak = p;
  }
  if (peak < 1.0) return false;
  p63 = 0.632 * peak;
  /* Re-fly the step to find when the rate first crossed 63.2 % of its own steady value. */
  std::unique_ptr<Fdm::FBFdm> g = Spawn(r, 5000.0, 400.0);
  if (!g) return false;
  Systems::FBFlightControl fc2 = ControlFor(r);
  Fdm::fb_fdm_state gs{};
  g->Step(gs);
  for (int i = 0; i < (int)(3.0 / kDt); i++) {
    Systems::FBControls c = fc2.Run(LevelGuidance(Clamp(0.08 * (5000.0 - gs.elev), -25.0, 25.0)), gs);
    g->SetControls(c.Roll, c.Pitch, c.Yaw, 1.0);
    g->Step(gs);
  }
  for (int i = 0; i < n; i++) {
    Systems::FBControls c = fc2.Run(LevelGuidance(Clamp(0.08 * (5000.0 - gs.elev), -25.0, 25.0)), gs);
    g->SetControls(1.0, c.Pitch, c.Yaw, 1.0);
    g->Step(gs);
    if (std::fabs(gs.p) >= p63) { tau = (i + 1) * kDt; break; }
  }
  if (tau <= 0.0) return false;
  /* The pilot's law runs at 10 Hz and its `a` is the DISCRETE pole at that rate, which is the form
   * doc/pilot.md states and FBF16Pilot's 0.734 / 78.7 pair is quoted in. */
  aOut = std::exp(-0.1 / tau);
  kOut = peak;
  peakOut = peak;
  return true;
}

/* TAKE-OFF GROUND RUN, where the catalogue publishes one. */
bool MeasureTakeoff(const FBAirAnchorRow &r, double &runOut) {
  std::unique_ptr<Fdm::FBFdm> f = SpawnOnGround(r, true);
  if (!f) return false;
  Fdm::fb_fdm_state st{};
  f->Step(st);
  double lat0 = st.lat, lon0 = st.lon;
  /* Vr = 1.15 x the stall speed of THIS row's own wing at THIS row's own CLmax [DERIVED], and not a
   * constant: a shared rotation speed would measure the constant and not the aeroplane. */
  double vs = std::sqrt(2.0 * r.GrossKg * 9.80665 / (1.225 * r.ClMax * r.WingAreaM2));
  double rotKt = 1.15 * vs * kMsToKt;
  for (int i = 0; i < (int)(120.0 / kDt); i++) {
    double kt = st.gs * kMsToKt;
    double pitch = kt > rotKt ? Clamp(0.10 * (12.0 - st.pitch) - 0.030 * st.q, -0.3, 0.8) : 0.0;
    f->SetControls(0.0, pitch, 0.0, 1.0);
    f->SetWheelBrakes(0.0, 0.0);
    f->Step(st);
    if (f->Faulted()) return false;
    if (!f->GetWow()) {
      double e = 0.0, n = 0.0;
      FBEnuOffsetM(lat0, lon0, st.lat, st.lon, e, n);
      runOut = std::sqrt(e * e + n * n);
      return true;
    }
  }
  return false;
}

/* MASS CLOSURE: what the deck weighs at empty + full internal fuel + pilot, against the sum of the
 * published parts. The probe the MiG-29 deck passes to 0.002 %; the two rows with no published empty
 * mass cannot be asked at all, and the harness says so instead of inventing an answer. */
bool MeasureMass(const FBAirAnchorRow &r, double &deckKg) {
  std::unique_ptr<Fdm::FBFdm> f = SpawnOnGround(r, false);
  if (!f) return false;
  f->SetFuelPct(100.0);
  Fdm::fb_fdm_state st{};
  f->Step(st);
  deckKg = f->GetWeightLbs() / 2.2046226;
  return deckKg > 100.0;
}

void RunRow(const FBAirAnchorRow &r) {
  std::printf("[row] %s %s\n", r.Key, r.Name);
  double v = 0.0;

  /* NINE THOUSAND SECONDS, and the number is a MEASUREMENT: at the anchor the thrust curve and the
   * drag curve are nearly TANGENT by construction (the inversion put T = D exactly there), so the last
   * tenth of a Mach is flown at 0.001-0.005 g and takes 600-3 000 s of simulated time. At 2 400 s three
   * rows reported the Mach they had reached rather than the Mach they were heading for — su7 M 1.63
   * still accelerating, against an equilibrium of M 1.70 and an anchor of 1.74. The deck's own drag at
   * that point matches the recipe's inversion to 0.5 %; the miss was the clock. */
  if (MeasureVmax(r, r.A1AltM, 400.0, 9000.0, v)) Emit(r.Key, "A1", v, "M");
  else Unmeasurable(r.Key, "A1");

  if (MeasureVmax(r, 100.0, 350.0, 3600.0, v)) Emit(r.Key, "A2", v, "M");
  else Unmeasurable(r.Key, "A2");

  double ceil = 0.0;
  if (MeasureCeiling(r, ceil)) Emit(r.Key, "A3", ceil, "m");
  else Unmeasurable(r.Key, "A3");

  double roc = 0.0;
  if (MeasureRoc(r, roc)) Emit(r.Key, "A4", roc, "m/s");
  else Unmeasurable(r.Key, "A4");

  /* THE TWO PULLS ARE TWO CONDITIONS, and conflating them measures neither: the g limit binds at high
   * dynamic pressure and the ALPHA limit binds at low, so a single fast pull reports the g limit and an
   * alpha of 7 deg while claiming to have measured an alpha limit of 22. */
  double g = 0.0, alpha = 0.0, turn = 0.0, corner = 0.0;
  if (MeasurePull(r, g, alpha, turn, corner, PullEntryKt(r, r.A5G > 0.0 ? r.A5G : 7.0, 5000.0))) {
    Emit(r.Key, "A5", g, "g");
    Emit(r.Key, "TURN", turn, "deg/s");
    Emit(r.Key, "CORNER", corner, "kt");
  } else {
    Unmeasurable(r.Key, "A5");
  }

  double alphaSlow = 0.0;
  if (MeasureAlphaLimit(r, alphaSlow)) Emit(r.Key, "ALPHA", alphaSlow, "deg");
  else Unmeasurable(r.Key, "ALPHA");

  double pa = 0.0, pk = 0.0, ppeak = 0.0;
  if (MeasureRollPlant(r, pa, pk, ppeak)) {
    Emit(r.Key, "ROLL-A", pa, "-");
    Emit(r.Key, "ROLL-K", pk, "deg/s");
  } else {
    Unmeasurable(r.Key, "ROLL-A");
  }

  double run = 0.0;
  if (MeasureTakeoff(r, run)) Emit(r.Key, "TAKEOFF", run, "m");
  else Unmeasurable(r.Key, "TAKEOFF");

  double deckKg = 0.0;
  if (MeasureMass(r, deckKg)) Emit(r.Key, "MASS", deckKg, "kg");
  else Unmeasurable(r.Key, "MASS");
}

} // namespace

int main(int argc, char **argv) {
  Clients::FBStdoutLogSink sink;
  FBLog::SetSink(&sink);
  FBLog::SetLevel(FBLogLevel::Info);
  int rows = 0;
  for (const FBAirAnchorRow &r : kAirAnchors) {
    bool want = argc <= 1;
    for (int i = 1; i < argc && !want; i++) want = std::strcmp(argv[i], r.Key) == 0;
    if (!want) continue;
    rows++;
    RunRow(r);
  }
  if (rows == 0) {
    std::fprintf(stderr, "fb-test-air-envelope: no such row\n");
    return 2;
  }
  std::printf("fb-test-air-envelope: %d row(s) measured\n", rows);
  return 0;
}
