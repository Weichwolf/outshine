/* fb-test-missile-airframe: the AIM-120 MODEL's own proof, with no guidance anywhere near it. Three
 * open-loop runs — powered with neutral fins (the motor), unpowered (the drag deck alone), powered with
 * a fixed pitch fin (the trim relation and achievable g) — printed beside the numbers the model's own
 * banner claims, so claim and measurement can be read against each other.
 * Everything goes through the same FBFdm::SetControls the guidance uses; nothing sets a state.
 * Exit 0 = every check inside its band, 1 = a check failed, 2 = the model would not load. */
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
/* The same "a weapon has no ground to hit" rule FBSimUnit applies: a 20,000 lbf/ft/s contact damper
 * met at 800 m/s is a stiff ODE that diverges inside ONE step. */
constexpr double kNoGroundElevM = -100000.0;

std::unique_ptr<Fdm::FBFdm> SpawnMissile(double altM) {
  Fdm::FBFdmSpawn ic;
  ic.ModelsRoot = "assets/aircraft";
  ic.Aircraft = "aim120";
  ic.LatDeg = 46.9; ic.LonDeg = 6.9;
  ic.GroundElevM = 0.0;
  ic.HeightOffsetM = altM;
  ic.HeadingDeg = 90.0;
  ic.Ballistic = true;                    /* a released store's IC: attitude + velocity vector, no trim */
  ic.VelEastMs = kReleaseSpeedMs;
  std::unique_ptr<Fdm::FBFdm> m = Fdm::FBFdmBoot::Spawn(ic);
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

/* `pitchCmd` is held for the whole run; + = nose up. */
Run Fly(double thr, double pitchCmd, double seconds, double altM = kReleaseAltM) {
  Run r;
  std::unique_ptr<Fdm::FBFdm> m = SpawnMissile(altM);
  if (!m) return r;
  r.LaunchWeightLbs = m->GetWeightLbs();
  Fdm::fb_fdm_state st{};
  double prevSpeed = kReleaseSpeedMs, t = 0.0;
  bool burnedOut = false;
  double fuel0 = m->GetFuelTotalLbs();
  for (int i = 0; i < (int)(seconds / Fdm::FBFdm::kStepS); i++) {
    m->SetControls(0.0, pitchCmd, 0.0, thr);
    m->Step(st);
    t += Fdm::FBFdm::kStepS;
    double accelG = (st.speed - prevSpeed) / Fdm::FBFdm::kStepS / 9.80665;
    prevSpeed = st.speed;
    if (st.speed > r.PeakSpeedMs) { r.PeakSpeedMs = st.speed; r.PeakMach = st.mach; }
    if (!burnedOut && accelG > r.PeakAccelG) r.PeakAccelG = accelG;
    if (std::fabs(t - 6.0) < Fdm::FBFdm::kStepS * 0.5) r.SustainAccelG = accelG;
    if (!burnedOut && m->GetFuelTotalLbs() <= 0.001 && fuel0 > 0.001) {
      burnedOut = true;
      r.BurnoutS = t;
      r.BurnoutSpeedMs = st.speed;
      r.BurnoutWeightLbs = m->GetWeightLbs();
    }
    if (burnedOut && std::fabs(t - (r.BurnoutS + 1.0)) < Fdm::FBFdm::kStepS * 0.5) r.CoastDecelG = -accelG;
    if (std::fabs(t - 20.0) < Fdm::FBFdm::kStepS * 0.5) r.SpeedAt20S = st.speed;
    /* Late enough for the alpha response to have settled, still supersonic. */
    if (std::fabs(t - 12.0) < Fdm::FBFdm::kStepS * 0.5) {
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
  Clients::FBStdoutLogSink sink;
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
  /* The model's own claims, as bands wide enough to test the MODEL and not an integration's last digit. */
  ok &= Check("launch weight lb (335 [T3])", powered.LaunchWeightLbs, 330.0, 340.0);
  ok &= Check("propellant burned lb (115 [DERIVED])",
              powered.LaunchWeightLbs - powered.BurnoutWeightLbs, 113.0, 117.0);
  ok &= Check("burn time s (10.7 [DERIVED])", powered.BurnoutS, 10.0, 11.5);
  /* The published "~Mach 4" is a spec-sheet maximum, i.e. a HIGH-altitude figure: the same motor
   * against four times the density cannot produce it, and a model claiming otherwise would be hiding
   * its own drag. So BOTH are checked. */
  ok &= Check("peak Mach at 15 km (~4 [T-ED])", high.PeakMach, 3.4, 4.4);
  ok &= Check("peak Mach at 6 km", powered.PeakMach, 2.5, 3.3);
  ok &= Check("boost accel g", powered.PeakAccelG, 12.0, 22.0);
  ok &= Check("sustain accel g", powered.SustainAccelG, 1.5, 6.0);
  ok &= Check("coast decel g after burnout", powered.CoastDecelG, 3.0, 9.0);
  /* A model whose drag was too low would show a fast supersonic dive here instead. */
  ok &= Check("unpowered peak Mach (ballistic dive)", coast.PeakMach, 0.85, 1.10);
  ok &= Check("unpowered speed at 20 s (below release)", coast.SpeedAt20S, 200.0, 300.0);
  /* Half fin should settle near half the full-fin trim alpha, and the resulting load factor is what
   * the terminal turn is actually flown on. */
  ok &= Check("trim alpha deg at half fin", trimmed.TrimAlphaDeg, 6.0, 18.0);
  ok &= Check("trim nz g at half fin", trimmed.TrimNz, 3.0, 30.0);

  FBLog::Info("test", "RESULT", {{"result", ok ? "PROVEN" : "FAILED"}});
  return ok ? 0 : 1;
}
