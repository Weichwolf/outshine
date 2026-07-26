#include "FBAutopilot.h"
#include <cmath>

namespace FlightBox {

static const double MPD = 111320.0;   /* metres per degree latitude (spherical approx) */
static const double R2D = 57.29577951308232;

static double Clamp(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }
static double Wrap180(double a) { while (a > 180) a -= 360; while (a < -180) a += 360; return a; }

FBAutopilot::FBAutopilot()
  : BankMaxDeg(60), KHdg(0.8), KAlt(0.08),
    Mode(FBMode::Manual), LatDeg(0), LonDeg(0), AltM(0), SpeedMs(220),
    MRoll(0), MPitch(0), MYaw(0), MThr(0.85) {}

void FBAutopilot::SetManual(double roll, double pitch, double yaw, double thr) {
  Mode = FBMode::Manual;
  MRoll = roll; MPitch = pitch; MYaw = yaw; MThr = thr;
}

void FBAutopilot::SetDirect(double lat, double lon, double altM, double speedMs) {
  Mode = FBMode::Direct;
  LatDeg = lat; LonDeg = lon; AltM = altM; SpeedMs = speedMs;
}

FBGuidance FBAutopilot::Run(const fb_fdm_state &s) {
  FBGuidance g{};
  g.Mode = Mode;
  g.TargetSpeedMs = SpeedMs;
  if (Mode == FBMode::Manual) {
    g.ManualRoll = MRoll; g.ManualPitch = MPitch; g.ManualYaw = MYaw; g.ManualThr = MThr;
    return g;
  }
  /* Bearing-to-point + altitude hold, no ring. The VS cap (25 m/s) is tighter than a cruise-altitude
   * correction would need: Direct also drives FBPilot's post-liftoff climb-out where the error is the
   * WHOLE climb (thousands of m) — uncapped the FLCS's own alpha scheduling still keeps AoA safe, but
   * the resulting near-30 deg pitch-up is a needlessly aggressive climb angle for a controlled climb-out. */
  double n = (LatDeg - s.lat) * MPD;
  double e = Wrap180(LonDeg - s.lon) * MPD * std::cos(s.lat * M_PI / 180.0);
  double brg = std::atan2(e, n) * R2D;
  double hdgErr = Wrap180(brg - s.yaw);
  g.BankCmdDeg = Clamp(KHdg * hdgErr, -BankMaxDeg, BankMaxDeg);
  g.AltErrM = AltM - s.elev;
  g.TargetVsMs = Clamp(KAlt * g.AltErrM, -25.0, 25.0);
  g.RingDistM = std::hypot(n, e);   /* diagnostics: distance to the target point */
  return g;
}

} // namespace FlightBox
