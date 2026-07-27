#include "FBAutopilot.h"
#include "FBGeodesy.h"
#include <cmath>

namespace FlightBox {

static double Clamp(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }

/* ---- THE LINE-TRACKING LAW, one definition, two users (the localizer and a route/run-in leg) ----
 * Crosstrack turns into an INTERCEPT ANGLE off the course, capped, and the resulting direction error
 * turns into bank. Cascaded rather than summed on purpose: the cap bounds the intercept a large offset
 * may ask for, so a jet ten kilometres wide of its track flies a steady angle at it instead of a
 * momentary right-angle turn — and for small errors the cascade IS the two-weight state feedback
 * (bank = -(kxt*kDir*y + kDir*dchi)), which is what the roots below are computed from. */
static double TrackBankCmd(double courseDeg, double acrossM, double dirDeg, double kXtDegPerM,
                           double interceptMaxDeg, double kDir, double bankMaxDeg) {
  double intercept = Clamp(-kXtDegPerM * acrossM, -interceptMaxDeg, interceptMaxDeg);
  double dirErr = FBWrap180(courseDeg + intercept - dirDeg);
  return Clamp(kDir * dirErr, -bankMaxDeg, bankMaxDeg);
}

/* ---- WHAT A LEG'S TWO WEIGHTS ARE, and why neither is chosen ----
 * The plant is a coordinated turn, and it is exactly second order in the crosstrack y:
 *     y'  = V * dchi                    (offset moves at the speed times the TRACK error)
 *     dchi' = (g/V) * phi               (the velocity vector turns with bank)
 * Feeding back both states, phi = -(k_y*y + k_chi*dchi), closes it as
 *     y'' + (g*k_chi/V) y' + (g*k_y) y = 0   ->   wn = sqrt(g*k_y),  zeta = k_chi*sqrt(g/k_y)/(2V)
 * Two unknowns, so exactly two statements are needed and no more:
 *
 * (1) DAMPING. zeta = 1/sqrt(2), the same textbook "settles without ringing" root the gun tracking loop
 *     is closed on (systems/FBPilot's kBfmTrackKi). Nothing about a route leg argues for another one.
 * (2) AUTHORITY. The crosstrack term alone reaches the bank LIMIT when the aircraft is one turn RADIUS
 *     off the line — the radius that same bank limit flies, R = V^2/(g*tan(phi_max)). That is the only
 *     self-referential statement available here: an offset the aircraft could not close inside its own
 *     tightest turn is exactly the offset for which the tightest turn is the right answer, and anything
 *     beyond it is saturated because there is nothing more to command.
 * Solving (2) for k_y and substituting into zeta gives the striking part: k_chi comes out INDEPENDENT of
 * both speed and gravity,
 *     k_chi = 2*zeta*sqrt(phi_max*tan(phi_max))          [deg of bank per deg of track error]
 *     k_y   = phi_max / R(V)                             [deg of bank per metre]
 * so the whole law is fixed by BankMaxDeg, and only the crosstrack half is scheduled — with 1/V^2, which
 * is what keeps the damping at 1/sqrt(2) at every speed the same jet flies. In the cascade above the
 * crosstrack gain is k_y/k_chi and the intercept cap is phi_max/k_chi (the angle at which the crosstrack
 * term alone is already asking for everything the bank limit has), so both fall out of the same two
 * statements rather than being picked.
 *
 * CROSS-CHECK, and the reason this derivation is trusted at all: at the F-16's flown approach speed
 * (165 KCAS, ~85 m/s) the schedule produces 0.074 deg/m against the 0.08 deg/m the localizer (KXt below)
 * has been flying by hand since Phase 3, and a 31.5 deg intercept cap against its 45. Within 8 % of a
 * gain that was arrived at independently, on an aircraft three times slower than the one the leg law
 * was measured on — which is what says the schedule is describing the aeroplane rather than fitting one
 * case. COURSE is nevertheless left on its own two numbers: it is a flown, measured approach, and this
 * round is not about the landing.
 *
 * WHAT IT DOES NOT DO. Against a CONSTANT lateral disturbance (an airframe that needs 0.2 deg of bank to
 * fly straight, as the vanilla model does) a two-state feedback keeps a static droop of phi_d/k_y — no
 * integrator, no zero steady-state error. That is deliberate: an integrator on a leg that is switched,
 * re-anchored and abandoned at every waypoint is a wind-up problem, and the droop it would remove is
 * ~10 m at 450 kt against the ~30 m the bearing law leaves. It is a bounded, speed-scheduled offset
 * instead of an unbounded, range-driven one. */
const double kLegZeta = 0.70710678118654752;   /* 1/sqrt(2) */
const double kLegG0 = 9.80665;
/* Below this the velocity vector has no direction worth regulating (a jet on the ground, a stall),
 * so the law falls back to reading the nose. It is well under any speed at which a leg is flown. */
const double kLegMinSpeedMs = 30.0;

FBAutopilot::FBAutopilot()
  : BankMaxDeg(60), KHdg(0.8), KAlt(0.08), KXt(0.08), CourseInterceptMaxDeg(45.0), ApproachVsCapMs(8.0),
    Mode(FBMode::Manual), LatDeg(0), LonDeg(0), AltM(0), SpeedMs(220),
    MRoll(0), MPitch(0), MYaw(0), MThr(0.85), CourseDeg(0), RefElevM(0), GlidepathDeg(3.0),
    HaveLeg(false), LegLatDeg(0), LegLonDeg(0), LegCourseDeg(0) {}

void FBAutopilot::SetManual(double roll, double pitch, double yaw, double thr) {
  Mode = FBMode::Manual;
  MRoll = roll; MPitch = pitch; MYaw = yaw; MThr = thr;
}

void FBAutopilot::SetDirect(double lat, double lon, double altM, double speedMs) {
  Mode = FBMode::Direct;
  LatDeg = lat; LonDeg = lon; AltM = altM; SpeedMs = speedMs;
  HaveLeg = false;
}

void FBAutopilot::SetDirectLeg(double fromLat, double fromLon, double lat, double lon, double altM,
                               double speedMs) {
  SetDirect(lat, lon, altM, speedMs);
  if (FBPlanarDistM(fromLat, fromLon, lat, lon) < kMinLegM) return;   /* not a direction (see the header) */
  HaveLeg = true;
  LegLatDeg = fromLat; LegLonDeg = fromLon;
  LegCourseDeg = FBBearingDeg(fromLat, fromLon, lat, lon);
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
    double along, across;
    FBTrackProjectM(LatDeg, LonDeg, CourseDeg, s.lat, s.lon, along, across);
    double distToGoM = -along;   /* + before the reference point (the usual case, final approach) */
    g.BankCmdDeg = TrackBankCmd(CourseDeg, across, s.yaw, KXt, CourseInterceptMaxDeg, KHdg, BankMaxDeg);
    /* Glidepath: height above the reference point's elevation grows linearly with distance-to-go —
     * a straight 3-deg-class descent, not a fixed altitude. Past the reference point (distToGoM<=0,
     * e.g. FBPilot already handed off to Flare but one more Run() lands here) the target simply holds
     * RefElevM rather than diving below it. A pure P correction on the resulting altitude error alone
     * has a permanent following-error against this RAMPING target (a classic type-1-servo steady-state
     * lag, error_ss = target_rate/KAlt — measured ~56 m high at the threshold on a 9 nm final at KAlt's
     * default, i.e. touching down deep into the runway instead of near the threshold): feedforward the
     * target's own descent rate (tan(glidepath) * closure speed) so KAlt's P term only has to correct
     * the DEVIATION from the beam, not track its slope from scratch. */
    double tanGp = std::tan(GlidepathDeg * kDeg2Rad);
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
  double n, e;
  FBEnuOffsetM(s.lat, s.lon, LatDeg, LonDeg, e, n);
  if (HaveLeg) {
    /* ON A LEG: hold the LINE (see the derivation above), and hold it against the GROUND TRACK rather
     * than the nose. That is not a detail — crosstrack is the integral of where the aircraft is
     * actually going, so regulating the heading instead leaves whatever sideslip or crab sits between
     * the two as a permanent drift rate that no amount of crosstrack gain can see. */
    double along, across;
    FBTrackProjectM(LegLatDeg, LegLonDeg, LegCourseDeg, s.lat, s.lon, along, across);
    double bankMaxRad = BankMaxDeg * kDeg2Rad;
    double kDir = 2.0 * kLegZeta * std::sqrt(bankMaxRad * std::tan(bankMaxRad));
    double v = std::fmax(s.gs, kLegMinSpeedMs);
    double turnRadiusM = v * v / (kLegG0 * std::tan(bankMaxRad));
    double trackDeg = s.gs > kLegMinSpeedMs ? std::atan2(s.vx, -s.vz) * kRad2Deg : s.yaw;
    g.BankCmdDeg = TrackBankCmd(LegCourseDeg, across, trackDeg, BankMaxDeg / (kDir * turnRadiusM),
                                BankMaxDeg / kDir, kDir, BankMaxDeg);
    (void)along;   /* the along half is the SEQUENCING's business, not the flying's (FBNavSystem) */
  } else {
    double brg = std::atan2(e, n) * kRad2Deg;
    double hdgErr = FBWrap180(brg - s.yaw);
    g.BankCmdDeg = Clamp(KHdg * hdgErr, -BankMaxDeg, BankMaxDeg);
  }
  g.AltErrM = AltM - s.elev;
  g.TargetVsMs = Clamp(KAlt * g.AltErrM, -25.0, 25.0);
  g.RingDistM = std::hypot(n, e);   /* diagnostics: distance to the target point */
  return g;
}

} // namespace FlightBox
