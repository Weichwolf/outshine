#include "FBAutopilot.h"
#include "FBTerrainField.h"
#include <cmath>

namespace FlightBox {

static const double MPD = 111320.0;   /* metres per degree latitude (spherical approx) */
static const double R2D = 57.29577951308232;
static const double G0  = 9.80665;

static double Clamp(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }
static double Wrap180(double a) { while (a > 180) a -= 360; while (a < -180) a += 360; return a; }

FBAutopilot::FBAutopilot()
  : BankMaxDeg(60), KHdg(0.8), KAlt(0.08), KXt(0.02), KXti(0.0004),
    KAgl(0.10), LookNearM(300), LookFarM(22000), LookStepM(250), ClimbCapMs(90), DescentCapMs(15),
    FloorM(100), ClimbLeadMargin(1.5), ClearBufferM(30), ReactTimeS(3.0), MinSpeedMs(130), DescentGuardM(5000),
    FanN(11), FanSamples(10), FanArcDeg(150), FanRangeM(12000), FanStraightBias(4.0), FanEase(0.7), FanMaxHeightW(0.4),
    FanTurnDeadbandDeg(10.0), HeadingDeadbandDeg(4.0), BankCapDeg(45.0),
    Mode(FBMode::Loiter), LatDeg(0), LonDeg(0), AltM(0), RadiusM(8000), SpeedMs(220), Dir(1),
    MRoll(0), MPitch(0), MYaw(0), MThr(0.85), XtIterm(0),
    AglM(150), HeadingDeg(180), FanSteer(false), Field(nullptr), GroundFn(nullptr),
    FanCLat(0), FanCLon(0), FanFenceM(500000.0),
    LastGroundHere(0), LastGroundAhead(0), LastTargetVs(0),
    FanCostMin(0), FanCostMid(0), FanCostMax(0), FanChosenIdx(0) {}

void FBAutopilot::SetLoiter(double lat, double lon, double altM, double radiusM, int dir, double speedMs) {
  Mode = FBMode::Loiter;
  LatDeg = lat; LonDeg = lon; AltM = altM; RadiusM = radiusM; Dir = dir; SpeedMs = speedMs;
  XtIterm = 0;
}

void FBAutopilot::SetManual(double roll, double pitch, double yaw, double thr) {
  Mode = FBMode::Manual;
  MRoll = roll; MPitch = pitch; MYaw = yaw; MThr = thr;
}

void FBAutopilot::SetLowLevel(double aglM, double speedMs, double headingDeg) {
  Mode = FBMode::LowLevel;
  AglM = aglM; SpeedMs = speedMs; HeadingDeg = headingDeg;
  FanSteer = true;   /* the reactive terrain fan is the default lateral law (SetLowLevelHeading turns it off) */
}

/* One ray's cost: distance-weighted terrain height along `hdgDeg` (near samples count more; a blend of
 * the integrated profile and the worst obstacle), plus a hard fence penalty if the ray reaches outside
 * the wander circle. Un-fetched DEM (WASM async) reads as the local ground -> conservative, never a
 * false "low valley there". Lower = a better (lower, in-bounds) direction. */
double FBAutopilot::FanRayCost(const fb_fdm_state &s, double hdgDeg, double groundHere) {
  const double hdg = hdgDeg * M_PI / 180.0;
  const double cN = std::cos(hdg), sE = std::sin(hdg);
  const double mpd = 111320.0, coslat = std::cos(s.lat * M_PI / 180.0);
  const double clc = coslat > 1e-3 ? coslat : 1e-3;
  double integ = 0.0, wsum = 0.0, worst = groundHere;
  double endLat = s.lat, endLon = s.lon;
  for (int j = 1; j <= FanSamples; j++) {
    double d = FanRangeM * (double)j / FanSamples;
    double lat_i = s.lat + (d * cN) / mpd;
    double lon_i = s.lon + (d * sE) / (mpd * clc);
    endLat = lat_i; endLon = lon_i;
    bool ok = false;
    double h = Field ? Field->HeightAt(lat_i, lon_i, &ok) : 0.0;
    if (!ok) h = groundHere;                       /* conservative for un-fetched DEM */
    double w = 1.0 - 0.7 * (double)j / FanSamples; /* near counts ~1.7x the far end */
    integ += w * h; wsum += w;
    if (h > worst) worst = h;
  }
  double cost = (wsum > 0 ? integ / wsum : groundHere) + FanMaxHeightW * worst;
  /* fence: if the ray's far end leaves the wander circle, make this direction expensive. */
  if (FanFenceM > 0) {
    double fx = (endLon - FanCLon) * mpd * clc, fy = (endLat - FanCLat) * mpd;
    double dc = std::hypot(fx, fy);
    if (dc > FanFenceM) cost += 1.0e5 * (dc - FanFenceM) / 1000.0;
  }
  return cost;
}

/* Reactive terrain fan (once/frame): sweep FanN rays over FanArcDeg centred on the committed heading,
 * cost each, add a straight-ahead bias (hysteresis: stay committed unless a clearly lower valley is off
 * axis), pick the min, and EASE HeadingDeg toward it (no snapping -> long straights, decisive turns). */
void FBAutopilot::UpdateLowLevelSteering(const fb_fdm_state &s, double dt) {
  if (!FanSteer || !Field || Mode != FBMode::LowLevel) return;
  double groundHere = GroundFn ? GroundFn(s.lat, s.lon) : Field->HeightAt(s.lat, s.lon);
  if (groundHere < -1e8) groundHere = 0.0;
  int n = FanN < 3 ? 3 : FanN;
  double best = 1e300, bestHdg = HeadingDeg, cmin = 1e300, cmax = -1e300, cmid = 0;
  int mid = (n - 1) / 2, chosen = mid;
  for (int i = 0; i < n; i++) {
    double off = ((double)i - mid) / (double)mid * (FanArcDeg * 0.5);   /* -arc/2 .. +arc/2 off committed */
    double rayHdg = HeadingDeg + off;
    double cost = FanRayCost(s, rayHdg, groundHere) + FanStraightBias * std::fabs(off);   /* commit bias */
    if (cost < cmin) cmin = cost;
    if (cost > cmax) cmax = cost;
    if (i == mid) cmid = cost;
    if (cost < best) { best = cost; bestHdg = rayHdg; chosen = i; }
  }
  double ease = FanEase * dt;   /* frame-rate-independent (native 15 Hz vs browser 60 Hz) */
  if (ease > 1.0) ease = 1.0; else if (ease < 0.0) ease = 0.0;
  double dh = Wrap180(bestHdg - HeadingDeg);
  /* COMMIT: hold the heading (wings-level) unless the best valley is clearly off-axis; then ease into a
   * decisive turn. This + the bank deadband = long straight segments punctuated by committed turns. */
  if (std::fabs(dh) > FanTurnDeadbandDeg) HeadingDeg = Wrap180(HeadingDeg + dh * ease);
  FanCostMin = cmin; FanCostMid = cmid; FanCostMax = cmax; FanChosenIdx = chosen;
}

void FBAutopilot::SetTerrain(FBTerrainField *field, double (*groundFn)(double, double)) {
  Field = field; GroundFn = groundFn;
}

/* Terrain-following AGL-hold (required-VS): local proportional hold to `AglM` above the point ground,
 * MAXed against the climb rate each look-ahead corridor sample demands to clear (ground+AglM) by the
 * time we arrive there (v²-early: a distant ridge starts a gentle climb, a near one a steep one). The
 * MAX = anticipation without diving into intervening dips; the hard floor + caps keep it safe + smooth.
 * Lateral (Stage 1) = hold HeadingDeg; Stage 2's planner steers HeadingDeg down valleys. */
double FBAutopilot::LowLevelVs(const fb_fdm_state &s, double groundHere, double *groundAheadOut,
                               double *maxReqVsOut) {
  double v = s.speed > 60.0 ? s.speed : 60.0;
  /* Baseline = the local hold: descend/climb toward AglM over the terrain UNDER the aircraft. Over flat
   * or falling ground this alone flies the jet at ~AglM (the "möglichly stay low" default). */
  double targetVs = KAgl * (groundHere + AglM - s.elev);
  double maxAhead = groundHere;
  double nearMax = groundHere;   /* highest terrain within DescentGuardM -> the descent floor */
  double maxReq = 0.0;   /* peak UNCAPPED climb demand -> drives the speed-reduction over steep terrain */
  if (Field) {
    const double hdg = HeadingDeg * M_PI / 180.0;
    const double cN = std::cos(hdg), sE = std::sin(hdg);
    const double mpd = 111320.0, coslat = std::cos(s.lat * M_PI / 180.0);
    for (double d = LookNearM; d <= LookFarM; d += LookStepM) {
      double lat_i = s.lat + (d * cN) / mpd;
      double lon_i = s.lon + (d * sE) / (mpd * (coslat > 1e-3 ? coslat : 1e-3));
      bool ok = false;
      double g_i = Field->HeightAt(lat_i, lon_i, &ok);
      if (!ok) g_i = groundHere;   /* DEM tile not fetched yet (WASM async): assume local ground ahead —
                                    * conservative (hold AGL, never a false "clear ahead"), no dive */
      if (g_i > maxAhead) maxAhead = g_i;
      if (d <= DescentGuardM && g_i > nearMax) nearMax = g_i;
      double dH = (g_i + AglM + ClearBufferM) - s.elev;   /* gain to clear at AglM (+buffer vs coarse-DEM peak loss) */
      if (dH <= 0.0) continue;                 /* already above it -> no climb demand (keep hugging) */
      /* Latest distance at which a max-rate climb still clears it, PLUS a reaction-lag distance (the FLCS
       * takes ~ReactTimeS to build a pull from level/descent). Only anticipate once within (1+margin)x
       * that -> the jet stays low until a climb becomes necessary, then pulls — but early enough to clear. */
      double dStart = dH / ClimbCapMs * v + ReactTimeS * v;
      if (d > dStart * (1.0 + ClimbLeadMargin)) continue;
      double tGo = d / v - ReactTimeS;         /* usable climb time = arrival time minus the build-up lag */
      if (tGo < 1.0) tGo = 1.0;
      double reqVs = dH / tGo;                  /* rate that clears it accounting for the lag */
      if (reqVs > targetVs) targetVs = reqVs;
      if (reqVs > maxReq) maxReq = reqVs;
    }
  }
  if (groundAheadOut) *groundAheadOut = maxAhead;
  if (maxReqVsOut) *maxReqVsOut = maxReq;
  /* Descent floor: never sink below AglM over the highest terrain just ahead (clear-at-envelope over
   * ridged terrain -> no diving into a col right in front of the next peak). Auto-relaxes to the local
   * hug once the near terrain is genuinely low. */
  double guardVs = (nearMax + AglM - s.elev) / (ReactTimeS > 1.0 ? ReactTimeS : 1.0);
  if (guardVs > targetVs) targetVs = guardVs;
  /* Safety ramp against the high-res local ground: from 0 at 1.3x floor down to full ClimbCap at the
   * hard floor — a smooth minimum climb (no bang) that guarantees AGL never crosses FloorM. */
  double agl = s.elev - groundHere;
  double band = 0.3 * FloorM;
  if (agl < FloorM + band) {
    double u = (FloorM + band - agl) / band;
    if (u > 1.0) u = 1.0;
    double minVs = ClimbCapMs * u;
    if (minVs > targetVs) targetVs = minVs;
  }
  if (targetVs > ClimbCapMs) targetVs = ClimbCapMs;
  if (targetVs < -DescentCapMs) targetVs = -DescentCapMs;
  return targetVs;
}

FBGuidance FBAutopilot::Run(const fb_fdm_state &s) {
  FBGuidance g{};
  g.Mode = Mode;
  g.TargetSpeedMs = SpeedMs;
  if (Mode == FBMode::Manual) {
    g.ManualRoll = MRoll; g.ManualPitch = MPitch; g.ManualYaw = MYaw; g.ManualThr = MThr;
    return g;
  }
  if (Mode == FBMode::LowLevel) {
    /* Vertical: terrain-following AGL-hold (look-ahead required-VS). The LOCAL ground uses the high-res
     * point source (matches the [agl] metric + the JSBSim floor); the corridor uses the coarse field. */
    double groundHere = GroundFn ? GroundFn(s.lat, s.lon) : (Field ? Field->HeightAt(s.lat, s.lon) : 0.0);
    if (groundHere < -1e8) groundHere = Field ? Field->HeightAt(s.lat, s.lon) : 0.0;   /* /elev not landed yet */
    double groundAhead = groundHere, maxReq = 0.0;
    g.TargetVsMs = LowLevelVs(s, groundHere, &groundAhead, &maxReq);
    g.AltErrM = (groundHere + AglM) - s.elev;
    /* If the terrain demands MORE climb than the cap, slow down so ClimbCap can keep up (a face rising at
     * R m/s needs climb R; at v it needs v·slope, so cutting v cuts the demand). Physics-honest: 450 kt
     * is nominal, the steepest Alps force a slowdown to hold AGL rather than fly into the hill. */
    double tgtSpeed = SpeedMs;
    if (maxReq > ClimbCapMs) tgtSpeed = SpeedMs * (ClimbCapMs / maxReq);
    g.TargetSpeedMs = tgtSpeed < MinSpeedMs ? MinSpeedMs : tgtSpeed;
    /* Lateral: WINGS-LEVEL discipline. Inside the heading deadband the bank command is EXACTLY 0 (no
     * micro-bank wobble about the roll axis); past it, bank proportional to the error beyond the deadband,
     * capped moderate. Combined with the fan's heading hysteresis this gives long straight segments and
     * decisive, promptly-levelled turns — how low-level is actually flown. */
    double hdgErr = Wrap180(HeadingDeg - s.yaw);
    double eff = std::fabs(hdgErr) <= HeadingDeadbandDeg ? 0.0
               : hdgErr - (hdgErr > 0 ? HeadingDeadbandDeg : -HeadingDeadbandDeg);
    g.BankCmdDeg = Clamp(KHdg * eff, -BankCapDeg, BankCapDeg);
    g.RingDistM = 0.0;
    LastGroundHere = groundHere; LastGroundAhead = groundAhead; LastTargetVs = g.TargetVsMs;
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
