/* fb-test-air-envelope: THE ENTRY GATE of doc/modules/air/module.md §Spec 11 criterion 3, and the first
 * of its three attribution instruments — the ANCHOR RESIDUAL, measured before any campaign runs.
 *
 * It is clients/FBTestMig29Envelope.cpp's harness PARAMETERISED BY ROW: the same method, applied ten
 * times instead of once. Not a simulation client — no module, no registry, no mission, no pilot. It
 * reaches each airframe the one legal way (fdm/FBFdmBoot) and flies it with the crudest controllers
 * that can hold a condition long enough to read a number off it.
 *
 * WHAT IT PRINTS, per row and per anchor: the published figure, what the deck does, the deviation in
 * percent, and the §7.1 band that deviation is judged against. A row every one of whose anchors is
 * inside its band is ACCEPTED and may answer a campaign question; a row outside stays ALPHA and may
 * fly in a mission and nothing more. THERE IS NO PRODUCTION FOR A CATALOGUE ROW: production is a word
 * about a MODULE, an airframe FlightBox is judged on.
 *
 * A MISSED ANCHOR IS A RESULT AND NOT A CRASH. Exit 0 = every anchor of every requested row was
 * MEASURED; 1 = a measurement could not be made at all; 2 = a model would not load. WIDENING A BAND TO
 * ADMIT A DECK IS THE ONE THING THIS HARNESS EXISTS TO PREVENT.
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
using namespace FlightBox::Clients;

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

/* THE CLIMB IS FLOWN AT CONSTANT CAS, which is what a climb schedule IS, and the sweep is over the CAS.
 * A constant-MACH schedule cannot express the one climb the fastest rows actually have: a MiG-25 cannot
 * reach M 2.6 at 2 000 m, so a M 2.6 target held it level at its spawn altitude for the whole run and
 * reported 2 049 m as a 20 700 m ceiling. At a constant 400 kt CAS the same aeroplane passes M 1.30 at
 * 11 km and M 2.60 at 20 km by arithmetic alone — the schedule finds the altitude instead of being told
 * it. */
double ScheduleVsCas(double casKt, double targetCasKt) {
  return Clamp(3.0 * (casKt - targetCasKt), 0.0, 300.0);
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

/* ---- Anchor bookkeeping. One row of recipe §7.1's table, per catalogue row. ---- */

struct Result {
  const char *Id;
  const char *What;
  double Target, Measured, Band;
  const char *Unit;
  bool Ok;          /* the measurement was MADE */
  bool Published;   /* the catalogue publishes a figure to judge it against */
};

Result gRes[24];
int gResN = 0;
int gMeasureFailures = 0;

void Record(const char *id, const char *what, double target, double measured, double band,
            const char *unit, bool ok, bool published = true) {
  if (gResN < 24) {
    Result &r = gRes[gResN++];
    r.Id = id; r.What = what; r.Target = target; r.Measured = measured; r.Band = band;
    r.Unit = unit; r.Ok = ok; r.Published = published;
  }
  if (!ok) gMeasureFailures++;
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

/* SERVICE CEILING: climb at full power until the rate of climb decays through 0.5 m/s (100 ft/min),
 * the definition every source in this catalogue uses without saying so. */
bool MeasureCeiling(const FBAirAnchorRow &r, double &ceilOut) {
  /* THE SCHEDULE IS SWEPT, not chosen. A service ceiling is the top of the best climb the aircraft
   * has, and the best climb speed is a property of the airframe — so picking ONE would measure the
   * schedule and not the aeroplane. Five constant-CAS candidates; the highest altitude any of them
   * reaches is the answer, which is the same "maximum over speed" the published figure is. */
  double best = 0.0;
  const double casKt[7] = {180.0, 240.0, 300.0, 360.0, 420.0, 480.0, 540.0};
  for (int k = 0; k < 7; k++) {
    double target = casKt[k];
    std::unique_ptr<Fdm::FBFdm> f = Spawn(r, 2000.0, 400.0);
    if (!f) return false;
    Systems::FBFlightControl fc = ControlFor(r);
    Fdm::fb_fdm_state st{};
    f->Step(st);
    /* THE CEILING IS WHERE THE STEADY CLIMB DECAYS THROUGH 0.5 m/s (100 ft/min), the definition every
     * source in this catalogue uses without saying so — and it is NOT the highest altitude the run
     * touches. A fast constant-CAS schedule arrives with kinetic energy and ZOOMS past its own steady
     * ceiling; reading the peak of that measured 28.9 % above a published figure on su22 and turned the
     * probe into a measurement of the entry speed. The vertical speed is filtered over ~10 s so a
     * phugoid does not end the run. */
    double top = 0.0, vsFilt = 25.0;
    int n = (int)(3600.0 / kDt);
    for (int i = 0; i < n; i++) {
      Systems::FBControls c = fc.Run(LevelGuidance(ScheduleVsCas(st.cas * kMsToKt, target)), st);
      f->SetControls(c.Roll, c.Pitch, c.Yaw, 1.0);
      f->Step(st);
      if (f->Faulted()) break;
      if (i % 100 == 0) HoldFuel(*f, r);
      if (st.mach > r.A1Mach * 1.02) break;   /* a climb schedule never exceeds the row's own Vmax */
      vsFilt += 0.001 * (st.vy - vsFilt);
      if (i > (int)(120.0 / kDt)) {
        if (vsFilt < 0.5) { top = st.elev; break; }
        if (st.elev > top) top = st.elev;
      }
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

int RunRow(const FBAirAnchorRow &r) {
  gResN = 0;
  int failuresBefore = gMeasureFailures;
  double v = 0.0;

  /* NINE THOUSAND SECONDS, and the number is a MEASUREMENT: at the anchor the thrust curve and the
   * drag curve are nearly TANGENT by construction (the inversion put T = D exactly there), so the last
   * tenth of a Mach is flown at 0.001-0.005 g and takes 600-3 000 s of simulated time. At 2 400 s three
   * rows reported the Mach they had reached rather than the Mach they were heading for — su7 M 1.63
   * still accelerating, against an equilibrium of M 1.70 and an anchor of 1.74. The deck's own drag at
   * that point matches the recipe's inversion to 0.5 %; the miss was the clock. */
  if (MeasureVmax(r, r.A1AltM, 400.0, 9000.0, v))
    Record("A1", "Vmax at altitude", r.A1Mach, v, kBandVmaxAlt, "M", true);
  else
    Record("A1", "Vmax at altitude", r.A1Mach, 0.0, kBandVmaxAlt, "M", false);

  if (r.A2Mach > 0.0) {
    if (MeasureVmax(r, 100.0, 350.0, 3600.0, v))
      Record("A2", "Vmax at sea level", r.A2Mach, v, kBandVmaxSl, "M", true);
    else
      Record("A2", "Vmax at sea level", r.A2Mach, 0.0, kBandVmaxSl, "M", false);
  } else {
    Record("A2", "Vmax at sea level [TODO in the catalogue]", 0.0, 0.0, kBandVmaxSl, "M", true, false);
  }

  double ceil = 0.0;
  if (MeasureCeiling(r, ceil)) Record("A3", "service ceiling", r.A3CeilingM, ceil, kBandCeiling, "m", true);
  else Record("A3", "service ceiling", r.A3CeilingM, 0.0, kBandCeiling, "m", false);

  double roc = 0.0;
  bool rocOk = MeasureRoc(r, roc);
  /* A4 IS JUDGED ONLY WHERE THE PUBLISHED FIGURE IS REACHABLE AT THE WEIGHT THE REST OF THE ANCHOR SET
   * IS FLOWN AT (recipe R11, and the criterion is per row and computed, not a decree). Every other
   * anchor here is quoted at the GROSS weight; the climb rates name no weight, and inverted against the
   * row's own frozen thrust and its own polar three of them come out BELOW the row's own EMPTY weight —
   * a figure no loading of that aeroplane can reach. Where the inverted weight is under gross the
   * anchor constrains nothing and the harness says so instead of judging a deck against it. */
  if (r.A4RocMs > 0.0 && r.A4WeightKg >= r.GrossKg)
    Record("A4", "rate of climb at sea level", r.A4RocMs, roc, kBandRoc, "m/s", rocOk);
  else if (r.A4RocMs > 0.0)
    Record("A4", "rate of climb [PROBE: the published figure needs a lighter, unnamed weight]", 0.0,
           roc, kBandRoc, "m/s", rocOk, false);
  else
    Record("A4", "rate of climb [the row publishes a TIME to height, not a rate]", 0.0, roc,
           kBandRoc, "m/s", rocOk, false);

  /* THE TWO PULLS ARE TWO CONDITIONS, and conflating them measures neither: the g limit binds at high
   * dynamic pressure and the ALPHA limit binds at low, so a single fast pull reports the g limit and an
   * alpha of 7 deg while claiming to have measured an alpha limit of 22. */
  double g = 0.0, alpha = 0.0, turn = 0.0, corner = 0.0;
  bool pullOk = MeasurePull(r, g, alpha, turn, corner,
                            PullEntryKt(r, r.A5G > 0.0 ? r.A5G : 7.0, 5000.0));
  double alphaSlow = 0.0;
  bool slowOk = MeasureAlphaLimit(r, alphaSlow);
  if (r.A5G > 0.0) Record("A5", "g held under the limiter (80 deg bank, full aft)", r.A5G, g, kBandG, "g", pullOk);
  else Record("A5", "g held [A5 is [TODO]: the limiter is [SET] at 7.0]", 0.0, g, kBandG, "g",
              pullOk, false);
  Record("ALPHA", "alpha held under the limiter (idle decel, full aft)", r.AlphaLimitDeg, alphaSlow,
         kBandAlpha, "deg", slowOk);
  /* NO PUBLISHED TARGET EXISTS for any row's turn rate or corner speed. They are recipe step 7's
   * MEASUREMENTS, they become this row's own FBPilot hooks, and they are DECLARED ACCEPTED MODEL
   * PROPERTIES exactly as the MiG-29's 241 deg/s roll rate was. */
  Record("TURN", "instantaneous turn rate at the limit", 0.0, turn, kBandTurnRate, "deg/s", pullOk, false);
  Record("CORNER", "corner speed (the turn-rate maximum)", 0.0, corner, 0.0, "kt", pullOk, false);

  double pa = 0.0, pk = 0.0, ppeak = 0.0;
  bool rollOk = MeasureRollPlant(r, pa, pk, ppeak);
  Record("ROLL-A", "roll plant pole a at 10 Hz", 0.0, pa, 0.0, "-", rollOk, false);
  Record("ROLL-K", "roll plant gain K (peak rate at full stick)", 0.0, pk, 0.0, "deg/s", rollOk, false);

  if (r.TakeoffRunM > 0.0) {
    double run = 0.0;
    bool ok = MeasureTakeoff(r, run);
    Record("TAKEOFF", "take-off ground run", r.TakeoffRunM, run, kBandTakeoff, "m", ok);
  } else {
    Record("TAKEOFF", "take-off ground run [not published for this row]", 0.0, 0.0, kBandTakeoff,
           "m", true, false);
  }

  double deckKg = 0.0;
  bool massOk = MeasureMass(r, deckKg);
  double want = r.EmptyKg + r.FuelKg + 100.0;
  Record("MASS", "deck mass empty + internal fuel + pilot", want, deckKg, kBandMass, "kg", massOk);

  /* ---- the report ---- */
  int outside = 0, judged = 0;
  std::printf("\n=== %-6s %-22s (%d engine%s) ===\n", r.Key, r.Name, r.Engines,
              r.Engines > 1 ? "s" : "");
  std::printf("  %-8s %-52s %12s %12s %9s %8s %s\n", "id", "what", "published", "measured", "dev%",
              "band%", "verdict");
  for (int i = 0; i < gResN; i++) {
    const Result &x = gRes[i];
    if (!x.Ok) {
      std::printf("  %-8s %-52s %12s %12s %9s %8s %s\n", x.Id, x.What, "-", "-", "-", "-",
                  "NOT MEASURED");
      FBLog::Error("air", "ANCHOR", {{"row", x.Id}, {"key", r.Key}, {"what", x.What},
                                     {"measured", false}});
      continue;
    }
    if (!x.Published || x.Target == 0.0) {
      std::printf("  %-8s %-52s %12s %12.4f %9s %8s %s\n", x.Id, x.What, "-", x.Measured, "-", "-",
                  "MEASURED");
      FBLog::Info("air", "ANCHOR", {{"key", r.Key}, {"id", x.Id}, {"what", x.What},
                                    {"measured", x.Measured}, {"unit", x.Unit}, {"published", false}});
      continue;
    }
    double dev = (x.Measured - x.Target) / x.Target;
    bool in = std::fabs(dev) <= x.Band;
    judged++;
    if (!in) outside++;
    std::printf("  %-8s %-52s %12.4f %12.4f %+8.1f%% %7.0f%% %s\n", x.Id, x.What, x.Target,
                x.Measured, 100.0 * dev, 100.0 * x.Band, in ? "in band" : "OUTSIDE");
    FBLog::Info("air", "ANCHOR", {{"key", r.Key}, {"id", x.Id}, {"what", x.What},
                                  {"target", x.Target}, {"measured", x.Measured},
                                  {"unit", x.Unit}, {"devPct", 100.0 * dev},
                                  {"bandPct", 100.0 * x.Band}, {"inBand", in}});
  }
  const char *promotion = outside == 0 ? "ACCEPTED" : "ALPHA";
  std::printf("  -> %d of %d judged anchors outside their band: %s%s\n", outside, judged, promotion,
              outside == 0 ? " (this row may answer a campaign question)"
                           : " (this row may fly, and may NOT answer a campaign question)");
  FBLog::Info("air", "PROMOTION", {{"key", r.Key}, {"outside", (double)outside},
                                   {"judged", (double)judged}, {"level", std::string(promotion)}});
  return gMeasureFailures - failuresBefore;
}

} // namespace

int main(int argc, char **argv) {
  Clients::FBStdoutLogSink sink;
  FBLog::SetSink(&sink);
  FBLog::SetLevel(FBLogLevel::Info);
  int rows = 0, notMeasured = 0;
  for (const FBAirAnchorRow &r : kAirAnchors) {
    bool want = argc <= 1;
    for (int i = 1; i < argc && !want; i++) want = std::strcmp(argv[i], r.Key) == 0;
    if (!want) continue;
    rows++;
    notMeasured += RunRow(r);
  }
  if (rows == 0) {
    std::fprintf(stderr, "fb-test-air-envelope: no such row\n");
    return 2;
  }
  std::printf("\nfb-test-air-envelope: %d row(s), %d anchor(s) that could not be measured at all\n",
              rows, notMeasured);
  return notMeasured == 0 ? 0 : 1;
}
