#include "FBAutopilot.h"
#include <cmath>

namespace FlightBox {

static const double MPD = 111320.0;   /* metres per degree latitude (spherical approx) */
static const double R2D = 57.29577951308232;

static double Clamp(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }
static double Wrap180(double a) { while (a > 180) a -= 360; while (a < -180) a += 360; return a; }

FBAutopilot::FBAutopilot()
  : BankMaxDeg(60), KHdg(0.8), KAlt(0.08), KXt(0.08), CourseInterceptMaxDeg(45.0), ApproachVsCapMs(8.0),
    Mode(FBMode::Manual), LatDeg(0), LonDeg(0), AltM(0), SpeedMs(220),
    MRoll(0), MPitch(0), MYaw(0), MThr(0.85), CourseDeg(0), RefElevM(0), GlidepathDeg(3.0) {}

void FBAutopilot::SetManual(double roll, double pitch, double yaw, double thr) {
  Mode = FBMode::Manual;
  MRoll = roll; MPitch = pitch; MYaw = yaw; MThr = thr;
}

void FBAutopilot::SetDirect(double lat, double lon, double altM, double speedMs) {
  Mode = FBMode::Direct;
  LatDeg = lat; LonDeg = lon; AltM = altM; SpeedMs = speedMs;
}

void FBAutopilot::SetCourse(double refLat, double refLon, double courseDeg, double refElevM,
                            double glidepathDeg, double speedMs) {
  Mode = FBMode::Course;
  LatDeg = refLat; LonDeg = refLon; CourseDeg = courseDeg; RefElevM = refElevM;
  GlidepathDeg = glidepathDeg; SpeedMs = speedMs;
}

FBGuidance FBAutopilot::Run(const fb_fdm_state &s) {
  FBGuidance g{};
  g.Mode = Mode;
  g.TargetSpeedMs = SpeedMs;
  if (Mode == FBMode::Manual) {
    g.ManualRoll = MRoll; g.ManualPitch = MPitch; g.ManualYaw = MYaw; g.ManualThr = MThr;
    return g;
  }
  if (Mode == FBMode::Course) {
    /* Localizer-style crosstrack-intercept-then-track (SetCourse's banner): project the aircraft into
     * along/across-track coordinates on the line through (LatDeg,LonDeg) heading CourseDeg (same
     * along=0-at-reference-point, +across=right-of-course convention as FBPilot's runway centerline law
     * and FBMissionMonitor's OnRunway, so all three agree on "on the line"). */
    double dy = (s.lat - LatDeg) * MPD;
    double dx = Wrap180(s.lon - LonDeg) * MPD * std::cos(LatDeg * M_PI / 180.0);
    double crs = CourseDeg * M_PI / 180.0;
    double along = dx * std::sin(crs) + dy * std::cos(crs);
    double across = dx * std::cos(crs) - dy * std::sin(crs);
    double distToGoM = -along;   /* + before the reference point (the usual case, final approach) */
    double intercept = Clamp(-KXt * across, -CourseInterceptMaxDeg, CourseInterceptMaxDeg);
    double hdgErr = Wrap180(CourseDeg + intercept - s.yaw);
    g.BankCmdDeg = Clamp(KHdg * hdgErr, -BankMaxDeg, BankMaxDeg);
    /* Glidepath: height above the reference point's elevation grows linearly with distance-to-go —
     * a straight 3-deg-class descent, not a fixed altitude. Past the reference point (distToGoM<=0,
     * e.g. FBPilot already handed off to Flare but one more Run() lands here) the target simply holds
     * RefElevM rather than diving below it. A pure P correction on the resulting altitude error alone
     * has a permanent following-error against this RAMPING target (a classic type-1-servo steady-state
     * lag, error_ss = target_rate/KAlt — measured ~56 m high at the threshold on a 9 nm final at KAlt's
     * default, i.e. touching down deep into the runway instead of near the threshold): feedforward the
     * target's own descent rate (tan(glidepath) * closure speed) so KAlt's P term only has to correct
     * the DEVIATION from the beam, not track its slope from scratch. */
    double tanGp = std::tan(GlidepathDeg * M_PI / 180.0);
    double targetAlt = RefElevM + tanGp * std::fmax(distToGoM, 0.0);
    g.AltErrM = targetAlt - s.elev;
    double vsFeedforward = -tanGp * s.gs;
    g.TargetVsMs = Clamp(vsFeedforward + KAlt * g.AltErrM, -ApproachVsCapMs, ApproachVsCapMs);
    g.RingDistM = distToGoM;
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
