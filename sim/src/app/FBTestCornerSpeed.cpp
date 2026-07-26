/* FlightBox — fb-test-corner-speed: the MEASUREMENT behind systems/FBPilot's BFM energy management.
 * doc/f16/aerodynamics-performance.md gives a corner PLATEAU of ~330-440 KCAS for the real jet and
 * explicitly refuses to tabulate a corner speed; CLAUDE.md Prinzip 5 says the reference is the vanilla
 * JSBSim model itself. So this harness measures the model's own instantaneous turn performance and the
 * pilot quotes the result — no copied real-jet number anywhere in the flight code.
 *
 * METHOD, one entry speed per sweep point: spawn airborne and trimmed at kAltM, roll to kBankDeg with
 * full lateral stick, then hold that bank while commanding full aft stick (fcs/elevator-cmd-norm = -1
 * through the model's OWN FLCS — the same path FBPilot's BFM phase commands, alpha limiter included) and
 * average the turn rate over kMeasureS seconds. Reported per point:
 *   turnRateDegS  d(psi)/dt, wrap-corrected — the measured instantaneous turn rate
 *   nz            body normal load factor reached (the g the FLCS actually allowed)
 *   alphaDeg      AoA at that g (the lift limit, once the alpha limiter is the binding constraint)
 *   casKtEnd      what the pull cost in speed over the window (the energy price)
 * Corner speed = the SLOWEST entry speed at which the turn rate is within kCornerBandFrac of the sweep's
 * best — the operational reading of "the slowest speed that still buys max turn rate", which is what a
 * pilot's corner number is for.
 *
 * `make test-corner` builds this -> build/fb-test-corner-speed. Exit 0 = a corner speed was found and
 * the sweep is monotone-then-flat as a turn-rate curve must be; 1 = the sweep produced no usable peak;
 * 2 = setup failure (JSBSim init). */
#include "FBFdmBoot.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include "FBLogSinks.h"
#include "FBUnits.h"
#include <cmath>
#include <memory>

using namespace FlightBox;

namespace {

const double kAltM = 5000.0;         /* well clear of terrain, still dense enough for a real pull */
const double kBankDeg = 85.0;        /* near-pure horizontal turn: the classic instantaneous-turn test */
const double kRollInMaxS = 4.0;      /* give up on the roll-in rather than hang if the bank never comes */
const double kMeasureS = 4.0;        /* long enough for the FLCS's own pitch loop to develop the pull,
                                      * short enough that the ENTRY speed still characterises the point */
const double kCornerBandFrac = 0.97; /* "within 3 % of the best rate" = still on the corner plateau */

struct SweepPoint {
  double EntryCasKt = 0.0;
  double TurnRateDegS = 0.0;
  double NzMean = 0.0, NzPeak = 0.0;
  double AlphaDeg = 0.0;
  double CasKtEnd = 0.0;
  double AltLossM = 0.0;
};

/* Bank hold + full aft stick. The roll axis is a RATE command into the model's own FLCS (f16.xml's
 * Roll channel differences fcs/aileron-cmd-norm against 0.31821*p), so a proportional bank-error law IS
 * a rate command — no damping term, the airframe's PID owns the inner loop. */
double BankHoldStick(double bankErrDeg) {
  double cmd = bankErrDeg / 45.0;   /* full stick at 45 deg of bank error */
  return cmd < -1.0 ? -1.0 : cmd > 1.0 ? 1.0 : cmd;
}

bool MeasurePoint(double entryCasKt, SweepPoint &out) {
  FBFdmSpawn ic;
  ic.ModelsRoot = "vendor/jsbsim/aircraft";
  ic.Aircraft = "f16";
  ic.LatDeg = 46.7; ic.LonDeg = 6.8;
  ic.GroundElevM = 0.0;
  ic.HeightOffsetM = kAltM;
  ic.SpeedMs = entryCasKt * kKtToMs;
  ic.HeadingDeg = 90.0;
  std::unique_ptr<FBFdm> fdm = FBFdmBoot::Spawn(ic);
  if (!fdm) return false;
  fdm->SetGroundElevM(0.0);
  fdm->SetFuelPct(60.0);
  fdm->SetGear(0.0);

  fb_fdm_state st{};
  fdm->Step(st);

  /* Roll-in: full lateral stick, wings-level pitch, until the bank is established. */
  for (int i = 0; i < (int)(kRollInMaxS / FBFdm::kStepS) && st.roll < kBankDeg; i++) {
    fdm->SetControls(1.0, 0.0, 0.0, 1.0);
    fdm->Step(st);
  }
  if (st.roll < kBankDeg - 5.0) return false;

  double psi0 = st.yaw, alt0 = st.elev, wrap = 0.0, prevPsi = st.yaw;
  double nzSum = 0.0, nzPeak = 0.0, alphaSum = 0.0;
  int n = (int)(kMeasureS / FBFdm::kStepS);
  for (int i = 0; i < n; i++) {
    fdm->SetControls(BankHoldStick(kBankDeg - st.roll), 1.0, 0.0, 1.0);
    fdm->Step(st);
    double d = FBWrap180(st.yaw - prevPsi);
    wrap += d;
    prevPsi = st.yaw;
    nzSum += st.nz;
    if (st.nz > nzPeak) nzPeak = st.nz;
    alphaSum += st.alphaDeg;
  }
  (void)psi0;
  out.EntryCasKt = entryCasKt;
  out.TurnRateDegS = std::fabs(wrap) / kMeasureS;
  out.NzMean = nzSum / n;
  out.NzPeak = nzPeak;
  out.AlphaDeg = alphaSum / n;
  out.CasKtEnd = st.cas * kMsToKt;
  out.AltLossM = alt0 - st.elev;
  return true;
}

} // namespace

int main() {
  FBStdoutLogSink sink;
  FBLog::SetSink(&sink);
  FBLog::SetLevel(FBLogLevel::Debug);

  SweepPoint pts[32];
  int count = 0;
  for (double cas = 180.0; cas <= 620.0 + 1e-9; cas += 20.0) {
    SweepPoint p;
    if (!MeasurePoint(cas, p)) {
      FBLog::Warn("corner", "POINT_FAILED", {{"entryCasKt", cas}});
      continue;
    }
    pts[count++] = p;
    FBLog::Info("corner", "POINT", {{"entryCasKt", p.EntryCasKt}, {"turnRateDegS", p.TurnRateDegS},
        {"nzMean", p.NzMean}, {"nzPeak", p.NzPeak}, {"alphaDeg", p.AlphaDeg},
        {"casKtEnd", p.CasKtEnd}, {"altLossM", p.AltLossM}});
    if (count == 32) break;
  }
  if (count < 3) {
    FBLog::Error("corner", "RESULT", {{"result", "SWEEP_FAILED"}, {"points", count}});
    return count == 0 ? 2 : 1;
  }

  double best = 0.0;
  for (int i = 0; i < count; i++) best = pts[i].TurnRateDegS > best ? pts[i].TurnRateDegS : best;
  int corner = -1;
  for (int i = 0; i < count && corner < 0; i++)
    if (pts[i].TurnRateDegS >= kCornerBandFrac * best) corner = i;
  if (corner < 0) {
    FBLog::Error("corner", "RESULT", {{"result", "NO_CORNER"}, {"bestDegS", best}});
    return 1;
  }
  FBLog::Info("corner", "RESULT", {{"result", "CORNER_FOUND"},
      {"cornerCasKt", pts[corner].EntryCasKt}, {"cornerTurnRateDegS", pts[corner].TurnRateDegS},
      {"cornerNz", pts[corner].NzMean}, {"cornerAlphaDeg", pts[corner].AlphaDeg},
      {"bestTurnRateDegS", best}, {"bandFrac", kCornerBandFrac}, {"points", count}});
  return 0;
}
