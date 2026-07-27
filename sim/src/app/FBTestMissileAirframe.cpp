/* FlightBox — fb-test-missile-airframe: the AIM-120 MODEL's own proof, with no guidance anywhere near
 * it. It flies sim/assets/aircraft/aim120 (FlightBox's own JSBSim model — the one model in the root
 * with no upstream counterpart, because the submodule has no AMRAAM) through three open-loop runs and prints the numbers the
 * model's own banner claims, so the claim and the measurement can be read against each other:
 *
 *   1. POWERED, fins neutral — the motor. Boost acceleration, the boost/sustain step, burnout time and
 *      speed, the mass that left through the nozzle, then the drag deceleration on the coast.
 *   2. UNPOWERED, fins neutral — the drag deck alone, from the same release state.
 *   3. POWERED, a fixed pitch fin — the trim relation and the achievable g: what alpha a given fin
 *      deflection settles at and what normal load factor that alpha buys at a stated Mach/altitude.
 *
 * Everything here goes through the same FBFdm::SetControls the guidance uses; nothing sets a state.
 *
 * `make test-missile` builds this -> build/fb-test-missile-airframe. Exit 0 = every check inside the
 * stated band; 1 = a check failed (the numbers are printed either way); 2 = the model would not load. */
#include "FBFdmBoot.h"
#include "FBLog.h"
#include "FBLogSinks.h"
#include "FBUnits.h"
#include <cmath>
#include <cstdio>
#include <memory>

using namespace FlightBox;

namespace {

constexpr double kReleaseAltM = 6000.0;   /* the altitude every number in the model banner is stated at */
constexpr double kHighAltM = 15000.0;     /* where a spec-sheet max-speed figure is actually made */
constexpr double kReleaseSpeedMs = 270.0; /* ~M 0.9 at 6 km — a fighter's launch speed */
/* The same "a weapon has no ground to hit" rule units/FBSimUnit applies to every released store
 * (kWeaponNoGroundElevM): a 20,000 lbf/ft/s contact damper met at 800 m/s is a stiff ODE that diverges
 * inside one step, so an open-loop run that outlives its altitude must coast, not crater. */
constexpr double kNoGroundElevM = -100000.0;

std::unique_ptr<FBFdm> SpawnMissile(double altM) {
  FBFdmSpawn ic;
  ic.ModelsRoot = "assets/aircraft";
  ic.Aircraft = "aim120";
  ic.LatDeg = 46.9; ic.LonDeg = 6.9;
  ic.GroundElevM = 0.0;
  ic.HeightOffsetM = altM;
  ic.HeadingDeg = 90.0;
  ic.Ballistic = true;                    /* a released store's IC: attitude + velocity vector, no trim */
  ic.VelEastMs = kReleaseSpeedMs;
  std::unique_ptr<FBFdm> m = FBFdmBoot::Spawn(ic);
  if (m) m->SetGroundElevM(kNoGroundElevM);
  return m;
}

struct Run {
  double PeakMach = 0.0, PeakSpeedMs = 0.0;
  double BurnoutS = 0.0, BurnoutSpeedMs = 0.0, BurnoutWeightLbs = 0.0;
  double LaunchWeightLbs = 0.0;
  double PeakAccelG = 0.0;                /* peak longitudinal acceleration during boost */
  double SustainAccelG = 0.0;             /* longitudinal acceleration at t = 6 s (mid sustain) */
  double CoastDecelG = 0.0;               /* deceleration 1 s after burnout */
  double SpeedAt20S = 0.0;
  double TrimAlphaDeg = 0.0, TrimNz = 0.0, TrimMach = 0.0;
};

/* One open-loop run. `thr` is the throttle (1 = light the motor, 0 = never), `pitchCmd` the pitch fin
 * command held for the whole run (FBFdm's convention: + = nose up). */
Run Fly(double thr, double pitchCmd, double seconds, double altM = kReleaseAltM) {
  Run r;
  std::unique_ptr<FBFdm> m = SpawnMissile(altM);
  if (!m) return r;
  r.LaunchWeightLbs = m->GetWeightLbs();
  fb_fdm_state st{};
  double prevSpeed = kReleaseSpeedMs, t = 0.0;
  bool burnedOut = false;
  double fuel0 = m->GetFuelTotalLbs();
  for (int i = 0; i < (int)(seconds / FBFdm::kStepS); i++) {
    m->SetControls(0.0, pitchCmd, 0.0, thr);
    m->Step(st);
    t += FBFdm::kStepS;
    double accelG = (st.speed - prevSpeed) / FBFdm::kStepS / 9.80665;
    prevSpeed = st.speed;
    if (st.speed > r.PeakSpeedMs) { r.PeakSpeedMs = st.speed; r.PeakMach = st.mach; }
    if (!burnedOut && accelG > r.PeakAccelG) r.PeakAccelG = accelG;
    if (std::fabs(t - 6.0) < FBFdm::kStepS * 0.5) r.SustainAccelG = accelG;
    if (!burnedOut && m->GetFuelTotalLbs() <= 0.001 && fuel0 > 0.001) {
      burnedOut = true;
      r.BurnoutS = t;
      r.BurnoutSpeedMs = st.speed;
      r.BurnoutWeightLbs = m->GetWeightLbs();
    }
    if (burnedOut && std::fabs(t - (r.BurnoutS + 1.0)) < FBFdm::kStepS * 0.5) r.CoastDecelG = -accelG;
    if (std::fabs(t - 20.0) < FBFdm::kStepS * 0.5) r.SpeedAt20S = st.speed;
    /* The trim sample: late enough for the fin's alpha response to have settled, still supersonic. */
    if (std::fabs(t - 12.0) < FBFdm::kStepS * 0.5) {
      r.TrimAlphaDeg = st.alphaDeg;
      r.TrimNz = st.nz;
      r.TrimMach = st.mach;
    }
  }
  return r;
}

bool Check(const char *what, double got, double lo, double hi) {
  bool ok = got >= lo && got <= hi;
  FBLog::Info("test", ok ? "CHECK_OK" : "CHECK_FAILED",
              {{"what", what}, {"got", got}, {"min", lo}, {"max", hi}});
  return ok;
}

} // namespace

int main() {
  FBStdoutLogSink sink;
  FBLog::SetSink(&sink);
  FBLog::SetLevel(FBLogLevel::Info);

  Run powered = Fly(1.0, 0.0, 40.0);
  if (powered.LaunchWeightLbs <= 0.0) {
    FBLog::Error("test", "RESULT", {{"result", "SETUP_FAILED"}, {"reason", "aim120 model would not load"}});
    return 2;
  }
  Run high = Fly(1.0, 0.0, 40.0, kHighAltM);
  Run coast = Fly(0.0, 0.0, 40.0);
  Run trimmed = Fly(1.0, 0.5, 16.0);

  FBLog::Info("test", "POWERED", {{"launchLbs", powered.LaunchWeightLbs},
      {"burnoutS", powered.BurnoutS}, {"burnoutLbs", powered.BurnoutWeightLbs},
      {"propellantLbs", powered.LaunchWeightLbs - powered.BurnoutWeightLbs},
      {"peakAccelG", powered.PeakAccelG}, {"sustainAccelG", powered.SustainAccelG},
      {"burnoutMs", powered.BurnoutSpeedMs}, {"peakMach", powered.PeakMach},
      {"coastDecelG", powered.CoastDecelG}, {"speed20sMs", powered.SpeedAt20S}});
  FBLog::Info("test", "POWERED_HIGH", {{"altM", kHighAltM}, {"peakMach", high.PeakMach},
      {"peakMs", high.PeakSpeedMs}, {"burnoutS", high.BurnoutS}, {"speed20sMs", high.SpeedAt20S}});
  FBLog::Info("test", "UNPOWERED", {{"peakMach", coast.PeakMach}, {"peakMs", coast.PeakSpeedMs},
      {"speed20sMs", coast.SpeedAt20S}});
  FBLog::Info("test", "TRIM", {{"pitchCmd", 0.5}, {"finDeg", 0.5 * 25.0},
      {"alphaDeg", trimmed.TrimAlphaDeg}, {"nzG", trimmed.TrimNz}, {"mach", trimmed.TrimMach}});

  bool ok = true;
  /* The model's own claims (aim120.xml's banner + engine/WPU-6.xml's derivation), as bands wide enough
   * that they test the MODEL rather than the last digit of an integration. */
  ok &= Check("launch weight lb (335 [T3])", powered.LaunchWeightLbs, 330.0, 340.0);
  ok &= Check("propellant burned lb (115 [DERIVED])",
              powered.LaunchWeightLbs - powered.BurnoutWeightLbs, 113.0, 117.0);
  ok &= Check("burn time s (10.7 [DERIVED])", powered.BurnoutS, 10.0, 11.5);
  /* ED's "~Mach 4" (weapons.md §3) is a spec-sheet maximum, i.e. a HIGH-altitude figure: the same motor
   * against four times the air density cannot produce it, and a model that claimed otherwise would be
   * hiding its own drag. Both are checked — the high-altitude run against ED's number, the 6 km run
   * against what this much drag actually leaves of it. */
  ok &= Check("peak Mach at 15 km (~4 [T-ED])", high.PeakMach, 3.4, 4.4);
  ok &= Check("peak Mach at 6 km", powered.PeakMach, 2.5, 3.3);
  ok &= Check("boost accel g", powered.PeakAccelG, 12.0, 22.0);
  ok &= Check("sustain accel g", powered.SustainAccelG, 1.5, 6.0);
  ok &= Check("coast decel g after burnout", powered.CoastDecelG, 3.0, 9.0);
  /* Unpowered from the same release state, the drag deck alone: it never goes supersonic on the level,
   * only the ballistic dive that follows buys it anything, and 20 s later it is back below the release
   * speed. A model whose drag was too low would show a fast supersonic dive here instead. */
  ok &= Check("unpowered peak Mach (ballistic dive)", coast.PeakMach, 0.85, 1.10);
  ok &= Check("unpowered speed at 20 s (below release)", coast.SpeedAt20S, 200.0, 300.0);
  /* The trim relation: half fin (12.5 deg) should settle near half the full-fin trim alpha of 23.5 deg,
   * and the resulting normal load factor is what the terminal turn is actually flown on. */
  ok &= Check("trim alpha deg at half fin", trimmed.TrimAlphaDeg, 6.0, 18.0);
  ok &= Check("trim nz g at half fin", trimmed.TrimNz, 3.0, 30.0);

  FBLog::Info("test", "RESULT", {{"result", ok ? "PROVEN" : "FAILED"}});
  return ok ? 0 : 1;
}
