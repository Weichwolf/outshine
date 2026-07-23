#include "FBAutopilot.h"
#include <cmath>

namespace FlightBox {

static const double MPD = 111320.0;   /* metres per degree latitude (spherical approx) */
static const double R2D = 57.29577951308232;
static const double G0  = 9.80665;

static double Clamp(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }
static double Wrap180(double a) { while (a > 180) a -= 360; while (a < -180) a += 360; return a; }

FBAutopilot::FBAutopilot()
  : BankMaxDeg(60), KHdg(0.8), KAlt(0.08), KXt(0.02), KXti(0.0004),
    Mode(FBMode::Loiter), LatDeg(0), LonDeg(0), AltM(0), RadiusM(8000), SpeedMs(220), Dir(1),
    MRoll(0), MPitch(0), MYaw(0), MThr(0.85), XtIterm(0) {}

void FBAutopilot::SetLoiter(double lat, double lon, double altM, double radiusM, int dir, double speedMs) {
  Mode = FBMode::Loiter;
  LatDeg = lat; LonDeg = lon; AltM = altM; RadiusM = radiusM; Dir = dir; SpeedMs = speedMs;
  XtIterm = 0;
}

void FBAutopilot::SetManual(double roll, double pitch, double yaw, double thr) {
  Mode = FBMode::Manual;
  MRoll = roll; MPitch = pitch; MYaw = yaw; MThr = thr;
}

FBGuidance FBAutopilot::Run(const fb_fdm_state &s) {
  FBGuidance g{};
  g.Mode = Mode;
  g.TargetSpeedMs = SpeedMs;
  if (Mode == FBMode::Manual) {
    g.ManualRoll = MRoll; g.ManualPitch = MPitch; g.ManualYaw = MYaw; g.ManualThr = MThr;
    return g;
  }
  /* Tangent + feedforward-bank + cross-track capture (P + slow I). On the ring in steady state the
   * cross-track terms vanish and the aircraft flies the tangent, so bankCmd == the pure feedforward
   * bank for the turn — no double-count. (Numerics identical to the proven flightctl.h law.) */
  double n = (s.lat - LatDeg) * MPD;
  /* Wrap180 the lon delta: without it a loiter straddling the ±180° dateline sees a ~360° delta ->
   * east offset ~40,000 km -> ringDist blows up + a false >70° bank reversal at the crossing. */
  double e = Wrap180(s.lon - LonDeg) * MPD * std::cos(LatDeg * M_PI / 180.0);
  double d = std::hypot(n, e);
  double tangent = std::atan2(e, n) * R2D + Dir * 90.0;
  double xtErr = d - RadiusM;
  XtIterm = Clamp(XtIterm + KXti * xtErr * 0.01, -15.0, 15.0);
  double xtCorr = Dir * Clamp(KXt * xtErr + XtIterm, -50.0, 50.0);
  double hdgErr = Wrap180(tangent + xtCorr - s.yaw);
  double v = s.speed > 12.0 ? s.speed : 12.0;
  double ffBank = std::atan2(v * v, RadiusM * G0) * R2D * Dir;
  g.BankCmdDeg = Clamp(ffBank + KHdg * hdgErr, -BankMaxDeg, BankMaxDeg);
  g.AltErrM = AltM - s.elev;
  g.TargetVsMs = Clamp(KAlt * g.AltErrM, -50.0, 50.0);
  g.RingDistM = d;
  return g;
}

} // namespace FlightBox
