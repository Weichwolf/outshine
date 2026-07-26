/* FlightBox — FBAutopilot: outer guidance, the DEFAULT implementation of a module's guidance system.
 * Modes (MANUAL pass-through, LOITER bank-to-circle around a geo centre at target altitude/radius,
 * LOWLEVEL terrain-following, DIRECT point-to-point bearing+altitude hold to one lat/lon — FBPilot's
 * climb-out/route guidance, reusing Loiter's own lat/lon/alt/speed target fields and KHdg/BankMaxDeg/
 * KAlt gains, just without the circling) -> a guidance command the inner FBW (FBFlightControl) tracks.
 * Port of flightctl.h's outer loop; numerics preserved verbatim (equivalence-tested).
 *
 * Run() is the one override point: a module whose guidance genuinely behaves differently (not just
 * different gains — e.g. a rotorcraft's station-keeping loiter) subclasses FBAutopilot and overrides
 * Run(); FBF16Module composes this DEFAULT unmodified. Config differences (F-16 gains, LOWLEVEL
 * tuning) stay data on this class, not a subclass. */
#ifndef FBAUTOPILOT_H
#define FBAUTOPILOT_H

#include "jsbsim_adapter.h"
#include "FBMode.h"

namespace FlightBox {

class FBTerrainField;   /* coarse-DEM look-ahead oracle (LOWLEVEL); wired by the app */

struct FBGuidance {
  FBMode Mode;
  double BankCmdDeg;     /* commanded bank (deg), loiter geometry + capture */
  double AltErrM;        /* target alt - current alt (m), unclamped — raw material for BOTH the FLCS's
                          * vs-command (TargetVsMs) and the raw path's pitch-attitude command */
  double TargetVsMs;     /* desired vertical speed (m/s) from the altitude loop */
  double TargetSpeedMs;  /* airspeed to hold */
  double ManualRoll, ManualPitch, ManualYaw, ManualThr;   /* pass-through in Manual */
  double RingDistM;      /* diagnostics: current distance from the loiter centre */
};

class FBAutopilot {
public:
  FBAutopilot();
  virtual ~FBAutopilot() = default;

  /* Loiter target: circle centre (deg), altitude (m ASL), radius (m), dir +1 CW / -1 CCW, speed. */
  void SetLoiter(double lat, double lon, double altM, double radiusM, int dir, double speedMs);
  void SetManual(double roll, double pitch, double yaw, double thr);

  /* DIRECT: fly the bearing to (lat, lon), holding altM/speedMs — a point, not a circle (see the class
   * banner). FBPilot's climb-out/route guidance. */
  void SetDirect(double lat, double lon, double altM, double speedMs);

  /* LOWLEVEL: terrain-following AGL-hold with look-ahead. Stage 1 flies a fixed heading; the vertical
   * law holds `aglM` above the terrain, pulling up early for ridges in the look-ahead corridor and never
   * commanding below the hard floor. Wire the terrain sources first via SetTerrain. */
  void SetLowLevel(double aglM, double speedMs, double headingDeg);
  void SetLowLevelHeading(double headingDeg) { HeadingDeg = headingDeg; FanSteer = false; }   /* external steer (planner/fixed) -> fan off */
  void SetLowLevelFan(bool on) { FanSteer = on; }   /* reactive terrain fan chooses the heading (default) */
  /* Wander fence for the fan: a ray whose reach leaves this circle is penalised, keeping the jet inside. */
  void SetFence(double centerLat, double centerLon, double radiusM) { FanCLat = centerLat; FanCLon = centerLon; FanFenceM = radiusM; }
  /* Terrain sources for LOWLEVEL: `field` = coarse-DEM look-ahead oracle (corridor samples); `groundFn`
   * = the high-res point ground (fb_stream_ground) for the local AGL + hard safety floor. */
  void SetTerrain(FBTerrainField *field, double (*groundFn)(double lat, double lon));

  /* REACTIVE TERRAIN FAN (default LOWLEVEL lateral law): once per frame, sample a fan of rays ahead on
   * the DEM, cost each by distance-weighted height + fence + straight-bias, and ease the committed
   * heading toward the lowest-cost ray (hysteresis). Call before the substep Run() loop. No-op unless
   * FanSteer + a terrain field. The per-substep Run() then flies the chosen heading wings-level. */
  void UpdateLowLevelSteering(const fb_fdm_state &s, double dt);

  /* The guidance override point (see the class banner) — everything else here is config/telemetry,
   * shared verbatim by any override. */
  virtual FBGuidance Run(const fb_fdm_state &s);

  FBMode GetMode(void) const { return Mode; }
  double GetTargetAlt(void) const { return AltM; }
  double GetRadius(void) const { return RadiusM; }
  /* LOWLEVEL diagnostics from the last Run (the [lowlevel] telemetry line). */
  double LlTargetAgl(void) const { return AglM; }
  double LlGroundHere(void) const { return LastGroundHere; }
  double LlGroundAhead(void) const { return LastGroundAhead; }
  double LlTargetVs(void) const { return LastTargetVs; }
  double LlHeading(void) const { return HeadingDeg; }
  /* [fan] telemetry from the last steering update. */
  double FanMinCost(void) const { return FanCostMin; }
  double FanMidCost(void) const { return FanCostMid; }
  double FanMaxCost(void) const { return FanCostMax; }
  int    FanChosen(void) const { return FanChosenIdx; }

  /* gains — public like a config block; defaults = the flown F-16 preset */
  double BankMaxDeg, KHdg, KAlt, KXt, KXti;
  /* LOWLEVEL vertical law: local-hold gain, corridor look-ahead (m), sample step, VS caps, hard floor,
   * and the climb-lead margin (fraction: a ridge is only anticipated once it enters (1+margin)x the
   * latest-safe max-rate-climb distance — so the jet hugs low terrain and climbs as LATE as safe). */
  double KAgl, LookNearM, LookFarM, LookStepM, ClimbCapMs, DescentCapMs, FloorM, ClimbLeadMargin;
  double ClearBufferM;   /* extra clearance over corridor ridges (absorbs coarse-DEM peak smoothing) */
  double ReactTimeS;     /* FLCS climb build-up lag: anticipate ridges this many seconds earlier */
  double MinSpeedMs;     /* slow to at most this over terrain too steep to out-climb at cruise (safe > stall) */
  double DescentGuardM;  /* don't sink below AglM over the highest terrain within this near distance (no
                          * diving into a col right before the next peak) — the safe clear-at-envelope rule */
  /* REACTIVE FAN law: ray count, total arc (deg), reach (m), samples/ray, straight-ahead cost bias
   * (per deg off the committed heading), heading-ease rate/frame (hysteresis), and the WINGS-LEVEL
   * roll discipline: heading deadband (below it -> bank 0, no wobble) + a moderate bank cap. */
  int    FanN, FanSamples;
  double FanArcDeg, FanRangeM, FanStraightBias, FanEase, FanMaxHeightW;   /* FanEase = heading-ease rate /s */
  double FanTurnDeadbandDeg;   /* commit: hold heading until the best ray is more than this off-axis (long straights) */
  double HeadingDeadbandDeg, BankCapDeg;

private:
  FBMode Mode;
  double LatDeg, LonDeg, AltM, RadiusM, SpeedMs;
  int Dir;
  double MRoll, MPitch, MYaw, MThr;
  double XtIterm;        /* cross-track integrator: closes the last few % of ring radius */
  double LowLevelVs(const fb_fdm_state &s, double groundHere, double *groundAheadOut, double *maxReqVsOut);
  double FanRayCost(const fb_fdm_state &s, double hdgDeg, double groundHere);
  /* LOWLEVEL */
  double AglM, HeadingDeg;
  bool FanSteer;
  FBTerrainField *Field;
  double (*GroundFn)(double lat, double lon);
  double FanCLat, FanCLon, FanFenceM;
  double LastGroundHere, LastGroundAhead, LastTargetVs;   /* telemetry snapshot */
  double FanCostMin, FanCostMid, FanCostMax;
  int FanChosenIdx;
};

} // namespace FlightBox
#endif
