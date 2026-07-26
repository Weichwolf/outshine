/* FlightBox — FBAutopilot: outer guidance, the DEFAULT implementation of a module's guidance system.
 * Modes (MANUAL pass-through, DIRECT point-to-point bearing+altitude hold to one lat/lon — FBPilot's
 * climb-out/route guidance, COURSE localizer/glideslope-style line tracking — FBPilot's final-approach
 * guidance, doc/f16/navigation-ils.md) -> a guidance command the inner FBW (FBFlightControl) tracks.
 * Port of flightctl.h's outer loop; numerics preserved verbatim (equivalence-tested); COURSE is new
 * (Phase 3, the landing) but sits at the same level as DIRECT, not a subclass override — see Run()'s
 * banner below.
 *
 * Run() is the one override point: a module whose guidance genuinely behaves differently (not just
 * different gains) subclasses FBAutopilot and overrides Run(); FBF16Module composes this DEFAULT
 * unmodified. Config differences (F-16 gains) stay data on this class, not a subclass. */
#ifndef FBAUTOPILOT_H
#define FBAUTOPILOT_H

#include "jsbsim_adapter.h"
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

private:
  FBMode Mode;
  double LatDeg, LonDeg, AltM, SpeedMs;
  double MRoll, MPitch, MYaw, MThr;
  double CourseDeg, RefElevM, GlidepathDeg;
};

} // namespace FlightBox
#endif
