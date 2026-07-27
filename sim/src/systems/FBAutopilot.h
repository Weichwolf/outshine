/* FlightBox — FBAutopilot: outer guidance, the DEFAULT implementation of a module's guidance system.
 * Modes (MANUAL pass-through, DIRECT altitude hold plus either a bearing to one lat/lon or the TRACK of
 * a declared leg — FBPilot's climb-out/route/run-in guidance, COURSE localizer/glideslope-style line
 * tracking — FBPilot's final-approach guidance, doc/f16/navigation-ils.md) -> a guidance command the
 * inner FBW (FBFlightControl) tracks. Port of flightctl.h's outer loop; numerics preserved verbatim
 * (equivalence-tested); COURSE is new (Phase 3, the landing) but sits at the same level as DIRECT, not a
 * subclass override — see Run()'s banner below.
 *
 * A POINT IS NOT A PATH, and Direct answers both because the difference is not a manoeuvre, it is what
 * the CALLER KNOWS. Steering at a bearing is the correct law when a point is all there is (an intercept,
 * a fight, a search: there is no line to be on). It is the wrong law when a track exists, and wrong in
 * a way that is a property of pure pursuit rather than of tuning: its positional stiffness falls as 1/R,
 * so at range it cannot hold a line at all — measured on the 19 km CCRP run-in, a 0.2 deg equivalent
 * bank asymmetry walked the jet 31 m off its own attack track while the loop answered with 0.2 deg of
 * bank. Hence SetDirectLeg below: the same mode, the same altitude/speed half, with the leg the caller
 * was flying handed in as data. No leg = no line = the bearing law, unchanged — which is also why a
 * defender told to fly at a point inside its own turn circle still orbits it forever (missions/
 * bfm-basic.fbm). That is one guidance kind with an optional origin and NOT a second mode, because a
 * second mode would duplicate the altitude, speed, capture and telemetry half of this one and force
 * every caller, every override and FBMode itself to learn a third word for the same manoeuvre.
 *
 * Run() is the one override point: a module whose guidance genuinely behaves differently (not just
 * different gains) subclasses FBAutopilot and overrides Run(); FBF16Module composes this DEFAULT
 * unmodified. Config differences (F-16 gains) stay data on this class, not a subclass. */
#ifndef FBAUTOPILOT_H
#define FBAUTOPILOT_H

#include "FBFdm.h"
#include "FBMode.h"

namespace FlightBox {

struct FBGuidance {
  FBMode Mode;
  double BankCmdDeg;     /* commanded bank (deg) */
  double AltErrM;        /* target alt - current alt (m), unclamped — raw material for the FLCS's
                          * vs-command (TargetVsMs) */
  double TargetVsMs;     /* desired vertical speed (m/s) from the altitude loop */
  double TargetSpeedMs;  /* airspeed to hold */
  double ManualRoll, ManualPitch, ManualYaw, ManualThr;   /* pass-through in Manual */
  double RingDistM;      /* diagnostics: distance to the Direct target point */
};

class FBAutopilot {
public:
  FBAutopilot();
  virtual ~FBAutopilot() = default;

  void SetManual(double roll, double pitch, double yaw, double thr);

  /* DIRECT: fly the bearing to (lat, lon), holding altM/speedMs — a point, not a circle. FBPilot's
   * climb-out/route guidance. */
  void SetDirect(double lat, double lon, double altM, double speedMs);

  /* DIRECT ON A LEG: the same destination, altitude and speed, flown as the TRACK from (fromLat,fromLon)
   * to (lat,lon) instead of as a bearing (class banner). The origin is the caller's knowledge — the
   * preceding fix of a route, the point a bombing run-in was commenced from — and this class only asks
   * that it be a real one: a leg shorter than kMinLegM has no defined direction and falls back to the
   * bearing law. Gains are not config here, they are DERIVED from BankMaxDeg alone (see Run()). */
  void SetDirectLeg(double fromLat, double fromLon, double lat, double lon, double altM, double speedMs);

  /* COURSE: track the infinite line through (refLat,refLon) on true heading courseDeg (an ILS
   * localizer's centerline, extended) while descending a straight glidepath of glidepathDeg toward
   * refElevM AT the reference point — FBPilot's final-approach guidance (doc/f16/navigation-ils.md's
   * "center the bars": crosstrack-intercept-then-track for the localizer, distance-to-go-scheduled
   * altitude for the glideslope). Generic over what the reference point/course MEANS (the caller, e.g.
   * FBPilot, supplies a runway threshold + its heading) — this class only tracks the line. */
  void SetCourse(double refLat, double refLon, double courseDeg, double refElevM, double glidepathDeg,
                 double speedMs);

  /* The guidance override point (see the class banner) — everything else here is config/telemetry,
   * shared verbatim by any override. */
  virtual FBGuidance Run(const fb_fdm_state &s);

  FBMode GetMode(void) const { return Mode; }
  double GetTargetAlt(void) const { return AltM; }

  /* gains — public like a config block; defaults = the flown F-16 preset */
  double BankMaxDeg, KHdg, KAlt;

  /* COURSE-only gains (localizer-intercept + glidepath, see SetCourse's banner): KXt turns crosstrack
   * offset (m) into an intercept-heading offset (deg), capped at CourseInterceptMaxDeg so the aircraft
   * flies a steady intercept angle rather than an instant turn-to-course; ApproachVsCapMs caps the
   * commanded vertical speed tighter than DIRECT's climb-out cap — a flown glidepath, not a dive. */
  double KXt, CourseInterceptMaxDeg, ApproachVsCapMs;

  /* Shorter than this a leg is a pair of coordinates, not a direction: the bearing between its ends
   * stops meaning anything long before the aircraft could fly it. Two mission fixes are kilometres
   * apart, so this only ever catches a degenerate declaration. */
  static constexpr double kMinLegM = 100.0;

private:
  FBMode Mode;
  double LatDeg, LonDeg, AltM, SpeedMs;
  double MRoll, MPitch, MYaw, MThr;
  double CourseDeg, RefElevM, GlidepathDeg;
  bool   HaveLeg;                       /* Direct carries a track origin (SetDirectLeg) */
  double LegLatDeg, LegLonDeg, LegCourseDeg;
};

} // namespace FlightBox
#endif
