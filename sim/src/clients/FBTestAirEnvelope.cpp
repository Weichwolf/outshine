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
  f->SetGear(0.0);
  return f;
}

std::unique_ptr<Fdm::FBFdm> SpawnOnGround(const FBAirAnchorRow &r) {
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
  f->SetGear(1.0);
  return f;
}

/* ---- The controllers, deliberately primitive, and identical in form to the MiG-29 harness's: every
 * catalogue deck is a RAW airframe with no FLCS inside it, so a stick command IS a surface deflection
 * and an undamped proportional law would oscillate at the short period. Each law therefore carries an
 * explicit rate term — the job systems/FBFlightControl does at module level and nothing does here. ---- */

double AltHoldStick(const Fdm::fb_fdm_state &st, double targetAltM) {
  double vsTarget = Clamp(0.08 * (targetAltM - st.elev), -25.0, 25.0);
  return Clamp(0.030 * (vsTarget - st.vy) - 0.030 * st.q, -0.6, 0.6);
}

double WingsLevelStick(const Fdm::fb_fdm_state &st) {
  return Clamp(-0.030 * st.roll - 0.010 * st.p, -0.5, 0.5);
}

/* THE LIMITER SKETCH, and it lives in the INSTRUMENT on purpose (recipe §6): the alpha and g limits are
 * systems/FBFlightControl's at module level and are deliberately NOT in the deck, so a bare catalogue
 * airframe has no limiting of any kind and a full-stick pull is answered by the whole surface travel.
 * What is measured here is how much turn the AIRFRAME has AT the documented limits. The lead terms are
 * the derivatives of the LIMITED QUANTITIES rather than the pitch rate, for the reason the MiG-29
 * harness records: a steady g turn carries a steady q, and a q lead reads that as an overshoot. */
struct PullLimiter {
  double AlphaDot = 0.0, NzDot = 0.0, PrevAlpha = 0.0, PrevNz = 0.0;
  bool Primed = false;

  double Stick(const Fdm::fb_fdm_state &st, double alphaLimitDeg, double gLimit, double dt) {
    if (Primed) {
      const double k = 0.15;
      AlphaDot += k * ((st.alphaDeg - PrevAlpha) / dt - AlphaDot);
      NzDot += k * ((st.nz - PrevNz) / dt - NzDot);
    }
    PrevAlpha = st.alphaDeg;
    PrevNz = st.nz;
    Primed = true;
    double byAlpha = 0.20 * (alphaLimitDeg - st.alphaDeg - 0.45 * AlphaDot);
    double byG = 0.25 * (gLimit - st.nz - 0.35 * NzDot);
    return Clamp(byAlpha < byG ? byAlpha : byG, -1.0, 0.9);
  }
};

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
  double best = 0.0, checkpoint = 0.0;
  int stagnant = 0;
  int n = (int)(maxS / kDt);
  for (int i = 0; i < n; i++) {
    f->SetControls(WingsLevelStick(st), AltHoldStick(st, altM), 0.0, 1.0);
    f->Step(st);
    if (f->Faulted()) return false;
    if (st.mach > best) best = st.mach;
    if (i > 0 && i % 1000 == 0) {
      if (best > checkpoint + 0.002) { checkpoint = best; stagnant = 0; }
      else if (++stagnant >= 3) break;
    }
  }
  machOut = best;
  return best > 0.05;
}

/* SERVICE CEILING: climb at full power until the rate of climb decays through 0.5 m/s (100 ft/min),
 * the definition every source in this catalogue uses without saying so. */
bool MeasureCeiling(const FBAirAnchorRow &r, double &ceilOut) {
  /* THE SCHEDULE IS SWEPT, not chosen. A service ceiling is the top of the best climb the aircraft
   * has, and for a Mach-2 interceptor that climb is supersonic while for a subsonic fighter it is not
   * — so picking ONE climb Mach would measure the schedule and not the airframe. Four candidates
   * spanning the row's own published envelope; the highest altitude any of them reaches is the answer,
   * which is the same "maximum over speed" the published figure is. */
  double best = 0.0;
  const double frac[4] = {0.45, 0.60, 0.80, 0.95};
  for (int k = 0; k < 4; k++) {
    double target = Clamp(frac[k] * r.A1Mach, 0.60, 2.60);
    std::unique_ptr<Fdm::FBFdm> f = Spawn(r, 2000.0, 400.0);
    if (!f) return false;
    Fdm::fb_fdm_state st{};
    f->Step(st);
    double top = st.elev, checkpoint = st.elev;
    int stagnant = 0;
    int n = (int)(2400.0 / kDt);
    for (int i = 0; i < n; i++) {
      /* Elevator holds the SPEED, thrust buys the climb — the energy split every climb schedule is:
       * too fast, pull; too slow, push. A Mach hold at constant altitude would only accelerate. */
      /* The push-over authority is deliberately tiny: THRUST buys the climb and the elevator only
       * holds the speed, so a schedule whose target Mach is not yet reachable must WAIT for it in level
       * flight rather than dive for it. Measured: with a symmetric authority the two Mach-2.5 rows
       * dived into the ground on their first candidate schedule and reported their spawn altitude as
       * their ceiling. */
      double cmd = Clamp(0.9 * (st.mach - target) - 0.035 * st.q, -0.05, 0.45);
      f->SetControls(WingsLevelStick(st), cmd, 0.0, 1.0);
      f->Step(st);
      if (f->Faulted()) break;
      if (st.elev > top) top = st.elev;
      if (i > 0 && i % 3000 == 0) {   /* every 30 s */
        if (top > checkpoint + 50.0) { checkpoint = top; stagnant = 0; }
        else if (++stagnant >= 2) break;   /* 60 s without gaining 50 m = the top */
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
    Fdm::fb_fdm_state st{};
    f->Step(st);
    double vsFilt = 0.0;
    for (int i = 0; i < (int)(20.0 / kDt); i++) {
      /* Hold the entry Mach and let the excess power go into climb — the energy definition of Ps. */
      double cmd = Clamp(0.6 * (m - st.mach) - 0.030 * st.q, -0.4, 0.7);
      f->SetControls(WingsLevelStick(st), cmd, 0.0, 1.0);
      f->Step(st);
      if (f->Faulted()) break;
      if (i > (int)(10.0 / kDt)) vsFilt += 0.01 * (st.vy - vsFilt);
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
  Fdm::fb_fdm_state st{};
  f->Step(st);
  PullLimiter lim;
  double gMax = 0.0, turnMax = 0.0, cornerKt = 0.0, aSum = 0.0;
  int aN = 0;
  /* A SUSTAINED turn at the limit DECELERATES, and that is what makes one measurement of two: the g
   * limit binds while the speed is up and the ALPHA limit binds once it has bled off, so sixty seconds
   * of holding the limiter passes through both. */
  int nStep = (int)(60.0 / kDt);
  for (int i = 0; i < nStep; i++) {
    double pitch = lim.Stick(st, r.AlphaLimitDeg, gLimit, kDt);
    /* 80 deg of bank: a level turn at the limit, which is what an instantaneous turn rate is quoted at. */
    double roll = Clamp(0.030 * (80.0 - st.roll) - 0.010 * st.p, -1.0, 1.0);
    f->SetControls(roll, pitch, 0.0, 1.0);
    f->Step(st);
    if (f->Faulted()) return false;
    if (i < (int)(4.0 / kDt)) continue;   /* the roll-in transient is not the turn */
    if (st.nz > gMax) gMax = st.nz;
    /* THE HELD alpha, not the peak. A limiter is judged by what it SETTLES at: the peak of a raw
     * airframe's transient is a departure, which recipe R7 says outright is the edge of the model and
     * not a model of the edge. Mean over the last five seconds. */
    if (i > nStep - (int)(5.0 / kDt)) { aSum += st.alphaDeg; aN++; }
    if (st.speed > 1.0 && st.nz > 1.05) {
      double rate = 9.80665 * std::sqrt(st.nz * st.nz - 1.0) / st.speed * kRad2Deg;
      if (rate > turnMax) { turnMax = rate; cornerKt = st.cas * kMsToKt; }
    }
  }
  gOut = gMax;
  alphaOut = aN > 0 ? aSum / aN : 0.0;
  turnOut = turnMax;
  cornerKtOut = cornerKt;
  return gMax > 1.0;
}

/* THE ROLL PLANT, and it is the reason this harness is a GATE and not a report: doc/pilot.md's
 * close-combat law inverts p_dot = -a*p + K*da, so `bfm` is unavailable to a row until (a, K) are
 * MEASURED on its own deck. Full aileron from wings level; K is the steady rate, a its reciprocal time
 * constant read at 63.2 % of it. */
bool MeasureRollPlant(const FBAirAnchorRow &r, double &aOut, double &kOut, double &peakOut) {
  std::unique_ptr<Fdm::FBFdm> f = Spawn(r, 5000.0, 400.0);
  if (!f) return false;
  Fdm::fb_fdm_state st{};
  f->Step(st);
  for (int i = 0; i < (int)(3.0 / kDt); i++) {   /* settle */
    f->SetControls(WingsLevelStick(st), AltHoldStick(st, 5000.0), 0.0, 1.0);
    f->Step(st);
  }
  double peak = 0.0, tau = -1.0;
  int n = (int)(3.0 / kDt);
  double p63 = 0.0;
  for (int i = 0; i < n; i++) {
    f->SetControls(1.0, AltHoldStick(st, 5000.0), 0.0, 1.0);
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
  Fdm::fb_fdm_state gs{};
  g->Step(gs);
  for (int i = 0; i < (int)(3.0 / kDt); i++) {
    g->SetControls(WingsLevelStick(gs), AltHoldStick(gs, 5000.0), 0.0, 1.0);
    g->Step(gs);
  }
  for (int i = 0; i < n; i++) {
    g->SetControls(1.0, AltHoldStick(gs, 5000.0), 0.0, 1.0);
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
  std::unique_ptr<Fdm::FBFdm> f = SpawnOnGround(r);
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
  std::unique_ptr<Fdm::FBFdm> f = SpawnOnGround(r);
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

  if (MeasureVmax(r, r.A1AltM, 400.0, 2400.0, v))
    Record("A1", "Vmax at altitude", r.A1Mach, v, kBandVmaxAlt, "M", true);
  else
    Record("A1", "Vmax at altitude", r.A1Mach, 0.0, kBandVmaxAlt, "M", false);

  if (r.A2Mach > 0.0) {
    if (MeasureVmax(r, 100.0, 350.0, 1200.0, v))
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
  if (r.A4RocMs > 0.0)
    Record("A4", "rate of climb at sea level", r.A4RocMs, roc, kBandRoc, "m/s", rocOk);
  else
    Record("A4", "rate of climb [the row publishes a TIME to height, not a rate]", 0.0, roc,
           kBandRoc, "m/s", rocOk, false);

  /* THE TWO PULLS ARE TWO CONDITIONS, and conflating them measures neither: the g limit binds at high
   * dynamic pressure and the ALPHA limit binds at low, so a single fast pull reports the g limit and an
   * alpha of 7 deg while claiming to have measured an alpha limit of 22. */
  double g = 0.0, alpha = 0.0, turn = 0.0, corner = 0.0;
  bool pullOk = MeasurePull(r, g, alpha, turn, corner, 480.0);
  double gSlow = 0.0, alphaSlow = 0.0, turnSlow = 0.0, cornerSlow = 0.0;
  bool slowOk = MeasurePull(r, gSlow, alphaSlow, turnSlow, cornerSlow, 380.0);
  if (r.A5G > 0.0) Record("A5", "g reached under the limiter (high-q pull)", r.A5G, g, kBandG, "g", pullOk);
  else Record("A5", "g reached [A5 is [TODO]: the limiter is [SET] at 7.0]", 0.0, g, kBandG, "g",
              pullOk, false);
  Record("ALPHA", "alpha reached under the limiter (low-q pull)", r.AlphaLimitDeg, alphaSlow,
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
