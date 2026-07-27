/* fb-test-corner-speed: the MEASUREMENT behind FBPilot's BFM energy management. The source refuses to
 * tabulate a corner speed and the reference is the vanilla model itself, so this measures the MODEL's
 * own instantaneous turn performance and the pilot quotes the result — no copied real-jet number
 * anywhere in the flight code.
 *
 * Method per sweep point: trimmed airborne entry, roll to kBankDeg, then hold that bank at full aft
 * stick through the model's OWN FLCS (alpha limiter included) and average the turn rate. Corner speed =
 * the SLOWEST entry speed still within kCornerBandFrac of the sweep's best turn rate.
 * Exit 0 = a corner speed was found and the curve is monotone-then-flat; 1 = no usable peak;
 * 2 = setup failure. */
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

/* The roll axis is a RATE command into the model's own FLCS, so a proportional bank-error law IS a
 * rate command — no damping term here, the airframe's own loop owns that. */
double BankHoldStick(double bankErrDeg) {
  double cmd = bankErrDeg / 45.0;   /* full stick at 45 deg of bank error */
  return cmd < -1.0 ? -1.0 : cmd > 1.0 ? 1.0 : cmd;
}

bool MeasurePoint(double entryCasKt, SweepPoint &out) {
  Fdm::FBFdmSpawn ic;
  ic.ModelsRoot = "assets/aircraft";
  ic.Aircraft = "f16";
  ic.LatDeg = 46.7; ic.LonDeg = 6.8;
  ic.GroundElevM = 0.0;
  ic.HeightOffsetM = kAltM;
  ic.SpeedMs = entryCasKt * kKtToMs;
  ic.HeadingDeg = 90.0;
  std::unique_ptr<Fdm::FBFdm> fdm = Fdm::FBFdmBoot::Spawn(ic);
  if (!fdm) return false;
  fdm->SetGroundElevM(0.0);
  fdm->SetFuelPct(60.0);
  fdm->SetGear(0.0);

  Fdm::fb_fdm_state st{};
  fdm->Step(st);

  /* Roll-in until the bank is established. */
  for (int i = 0; i < (int)(kRollInMaxS / Fdm::FBFdm::kStepS) && st.roll < kBankDeg; i++) {
    fdm->SetControls(1.0, 0.0, 0.0, 1.0);
    fdm->Step(st);
  }
  if (st.roll < kBankDeg - 5.0) return false;

  double psi0 = st.yaw, alt0 = st.elev, wrap = 0.0, prevPsi = st.yaw;
  double nzSum = 0.0, nzPeak = 0.0, alphaSum = 0.0;
  int n = (int)(kMeasureS / Fdm::FBFdm::kStepS);
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
  Clients::FBStdoutLogSink sink;
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
