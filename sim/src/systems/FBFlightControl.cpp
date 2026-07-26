#include "FBFlightControl.h"
#include <cmath>

namespace FlightBox {

static double Clamp(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }

FBFlightControl::FBFlightControl()
  : Flcs(0), RollStickMax(1.0),
    KRollRate(0.06), KG(0.4), KGi(2.0), KVs2g(0.05), KVsi(0.02), KNy(1.5), KNyi(2.0),
    KTi(0.002), KpSpd(0.03), ThrTrim(0.55),
    KpRoll(0.022), KdRoll(0.004), KpPitch(0.06), KdPitch(0.010), KdYaw(0.006), KCoord(0.004),
    PitchMaxDeg(15), KAltRaw(0.05), NzSlew(1.5),
    GIterm(0), VsIterm(0), NyIterm(0), ThrIterm(0), NzPrev(0) {}

void FBFlightControl::Reset(void) { GIterm = VsIterm = NyIterm = ThrIterm = NzPrev = 0; }

FBFlightControl FBFlightControl::F16(void) {
  FBFlightControl c;
  c.Flcs = 1;
  c.RollStickMax = 0.15;   /* gentle pilot roll-in: nz stays 0.7..1.9 vs -1.1..+3.0 at 0.35 (measured) */
  c.KRollRate = 0.05; c.KG = 0.25; c.KGi = 0.8;
  c.KpSpd = 0.02; c.ThrTrim = 0.85;
  return c;
}

FBControls FBFlightControl::Run(const FBGuidance &g, const fb_fdm_state &s) {
  FBControls o{};
  if (g.Mode == FBMode::Manual) {
    o.Roll = g.ManualRoll; o.Pitch = g.ManualPitch; o.Yaw = g.ManualYaw; o.Thr = g.ManualThr;
    return LastControls_ = o;
  }
  if (Flcs) {
    /* Turn-g feedforward from the ACTUAL bank (the g a turn needs NOW depends on the bank NOW), the
     * vs-error INTEGRAL collapsing the false alt/g equilibria, a slew-limited g command, and the g
     * stick BLENDED in only once the bank is nearly established (a pilot rolls in neutral-pitch;
     * driving the g loop mid-roll stacks onto the FLCS's own g-PID -> entry spikes). All measured
     * against the bare model — see flightctl.h history / sim-critic runs. */
    double bc = std::fmin(std::fabs(s.roll), 80.0) * (M_PI / 180.0);
    double targetVs = g.TargetVsMs;
    double vsErr = targetVs - s.vy;
    VsIterm = Clamp(VsIterm + KVsi * vsErr * 0.01, -0.5, 0.5);
    double nzRaw = 1.0 / std::cos(bc) + KVs2g * vsErr + VsIterm;
    if (NzPrev == 0.0) NzPrev = s.nz;
    double step = NzSlew * 0.01;
    double nzCmd = Clamp(nzRaw, NzPrev - step, NzPrev + step);
    NzPrev = nzCmd;
    double gErr = nzCmd - s.nz;
    double bankErr = std::fabs(g.BankCmdDeg - s.roll);
    double blend = Clamp(1.0 - (bankErr - 5.0) / 15.0, 0.0, 1.0);
    GIterm = Clamp(GIterm + blend * KGi * gErr * 0.01, -1.0, 1.0);
    o.Roll = Clamp(KRollRate * (g.BankCmdDeg - s.roll), -RollStickMax, RollStickMax);
    o.Pitch = blend * Clamp(KG * gErr + GIterm, -1.0, 1.0);
    /* pedals: null the lateral load factor — the model's yaw damper (raw r feedback, no washout)
     * holds rudder against a steady turn; residual ny ~ -0.10 g remains model-intrinsic. */
    NyIterm = Clamp(NyIterm + KNyi * s.ny * 0.01, -0.6, 0.6);
    o.Yaw = Clamp(KNy * s.ny + NyIterm, -1.0, 1.0);
  } else {
    double pitchCmd = Clamp(KAltRaw * g.AltErrM, -PitchMaxDeg, PitchMaxDeg); /* not used by the F-16
                            (FLCS-command inner); the c172-era attitude PD for FLCS-less models —
                            pitch-angle target from alt error, verbatim flightctl.h fc_update raw path */
    o.Roll = Clamp(KpRoll * (g.BankCmdDeg - s.roll) - KdRoll * s.p, -1.0, 1.0);
    o.Pitch = -Clamp(KpPitch * (pitchCmd - s.pitch) - KdPitch * s.q, -1.0, 1.0);
    o.Yaw = Clamp(-KdYaw * s.r + KCoord * g.BankCmdDeg, -1.0, 1.0);
  }
  double spdErr = g.TargetSpeedMs - s.speed;
  ThrIterm = Clamp(ThrIterm + KTi * spdErr * 0.01, -ThrTrim, 1.0 - ThrTrim);   /* physical range from trim */
  o.Thr = Clamp(ThrTrim + KpSpd * spdErr + ThrIterm, 0.0, 1.0);
  return LastControls_ = o;
}

void FBFlightControl::DeclareTelemetry(FBTelemetrySchema &schema) const {
  schema.Add("rollCmd");
  schema.Add("pitchCmd");
  schema.Add("yawCmd");
  schema.Add("throttleNorm");
}

void FBFlightControl::SampleTelemetry(FBTelemetryRow &row) const {
  row.Push(LastControls_.Roll);
  row.Push(LastControls_.Pitch);
  row.Push(LastControls_.Yaw);
  row.Push(LastControls_.Thr);
}

} // namespace FlightBox
