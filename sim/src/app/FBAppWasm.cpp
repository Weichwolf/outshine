/* FBRenderer demo page (Stage 6): the REAL in-process F-16 flies the WebGPU engine. libJSBSim +
 * FBF16Pilot (mission phase machine) + FBAutopilot/FBFlightControl (F-16 FLCS-command) run here each
 * frame; the camera is the aircraft's eye (position + attitude -> ECEF basis, FBCamera.h pattern),
 * FBWorld streams z14 fb-tiles around the live flight position. Origin from config.js
 * (FB_ORIGIN_LAT/LON); default boot mission from web/missions/ (see main()'s banner). */
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>
#include <emscripten.h>
#include "FBRenderer.h"
#include "FBWorld.h"
#include "FBF16Module.h"
#include "FBMissionBoot.h"
#include "FBEphemeris.h"
#include "FBTerrainLoader.h"
#include "FBTelemetry.h"
#include "FBOwnshipUnit.h"
#include "jsbsim_adapter.h"
#include <cstdint>

using namespace FlightBox;

static const double kPi = 3.14159265358979323846;
static const double kMPerDeg = 111320.0;
static const double kRadiusM = 8000.0;   /* ?ap=manual spawn offset */
static const double kAglM = 1500.0;      /* ?ap=manual spawn height above ground */
static const double kSpeedMs = 220.0;    /* ?ap=manual spawn speed */
static const double kWpCaptureM = 500.0; /* generic waypoint capture radius (matches FBAppNative.cpp) */
static const char *kDefaultMissionUrl = "/missions/payerne-takeoff-only.fbm";   /* served from sim/web/
    missions/ by fb-sim's own web/ mount (up.sh) — editable without a WASM rebuild */

static FBRenderer R;
static FBWorld W;
/* The App holds its module POLYMORPHICALLY: gActiveModule is the runtime-selected FBModule (today
 * always the F-16, but every per-frame call below goes through the base interface, not the concrete
 * type — the mechanism a future Ka-52/F-18 module plugs into, not a shortcut around it). gF16 is the
 * concrete handle the boot/config code below needs for F-16-specific setup (Autopilot/FlightControl
 * gains, mission spawn) that isn't part of the generic FBModule contract. */
static std::unique_ptr<FBF16Module> gF16 = std::make_unique<FBF16Module>();
static FBModule *gActiveModule = gF16.get();
static fb_fdm_state St;
static FBOwnshipUnit gOwnship(1, St);    /* the ownship as an FBUnit — borrows St (see its own banner);
                                          * safe: St has static storage + no dynamic initializer, so its
                                          * zero-init precedes gOwnship's dynamic init regardless of order */
static double Ground = 430.0;            /* config default; refined from the terrain if available */
static double LastMs = 0.0;
static double Olat = 47.179846, Olon = 7.411427;   /* ENU/home origin (config.js) */
static time_t SimUtc = 0;                /* FB_SIM_UTC override; 0 = real wall clock (live sky) */

/* Ground albedo (TAB in index.html eats the key before SDL) — flips SVS(OSM) <-> EVS(photo) on both
 * the streamer (lazy photo fetch) and the renderer (draw layer + which sun). One source of truth. */
static void GroundSet(int photo) {
  R.SetGroundMode(photo);
  W.SetGroundMode(photo);
  printf("[gpu] ground = %s\n", photo ? "aerial photo (EVS)" : "OSM render (SVS)");
  fflush(stdout);
}
extern "C" EMSCRIPTEN_KEEPALIVE void fb_toggle_ground(void) { GroundSet(!R.GetGroundMode()); }
extern "C" EMSCRIPTEN_KEEPALIVE void fb_set_ground(int photo) { GroundSet(photo); }

/* WGS84 geodetic -> ECEF (double). */
static void geoToEcef(double latDeg, double lonDeg, double alt, double out[3]) {
  const double a = 6378137.0, e2 = 6.69437999014e-3;
  double lat = latDeg * kPi / 180.0, lon = lonDeg * kPi / 180.0;
  double sl = std::sin(lat), cl = std::cos(lat);
  double N = a / std::sqrt(1.0 - e2 * sl * sl);
  out[0] = (N + alt) * cl * std::cos(lon);
  out[1] = (N + alt) * cl * std::sin(lon);
  out[2] = (N * (1.0 - e2) + alt) * sl;
}

static void cross(const double a[3], const double b[3], double o[3]) {
  o[0] = a[1] * b[2] - a[2] * b[1];
  o[1] = a[2] * b[0] - a[0] * b[2];
  o[2] = a[0] * b[1] - a[1] * b[0];
}
static void norm(double v[3]) {
  double l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (l < 1e-12) l = 1.0;
  v[0] /= l; v[1] /= l; v[2] /= l;
}

/* ENU basis in ECEF at (lat,lon) — FBCamera.h w3_enu_axes_ecef. */
static void enuAxes(double latDeg, double lonDeg, double E[3], double N[3], double U[3]) {
  double P = latDeg * kPi / 180.0, L = lonDeg * kPi / 180.0;
  double sP = std::sin(P), cP = std::cos(P), sL = std::sin(L), cL = std::cos(L);
  E[0] = -sL;     E[1] = cL;      E[2] = 0.0;
  N[0] = -sP * cL; N[1] = -sP * sL; N[2] = cP;
  U[0] = cP * cL;  U[1] = cP * sL;  U[2] = sP;
}

/* Aircraft attitude -> ECEF camera basis. Render-ENU basis from yaw/pitch/roll (FBCamera.h
 * w3_basis_from: +s roll sign, silent-mirror-critical), then rotated into ECEF at (lat,lon). */
static void cameraBasis(double yawDeg, double pitchDeg, double rollDeg, double latDeg, double lonDeg,
                        double fwd[3], double right[3], double up[3]) {
  double yaw = yawDeg * kPi / 180.0, pitch = pitchDeg * kPi / 180.0, roll = rollDeg * kPi / 180.0;
  /* render-space (E=+X, up=+Y, N=-Z): forward from yaw/pitch */
  double fR[3] = {std::cos(pitch) * std::sin(yaw), std::sin(pitch), -std::cos(pitch) * std::cos(yaw)};
  double wup[3] = {0, 1, 0}, s[3], u[3];
  cross(fR, wup, s); norm(s);   /* screen-right = fwd x up */
  cross(s, fR, u);              /* screen-up = right x fwd */
  double upR[3], srR[3];
  for (int i = 0; i < 3; i++) {
    upR[i] = u[i] * std::cos(roll) + s[i] * std::sin(roll);
    srR[i] = s[i] * std::cos(roll) - u[i] * std::sin(roll);
  }
  double E[3], N[3], U[3];
  enuAxes(latDeg, lonDeg, E, N, U);
  auto toEcef = [&](const double rv[3], double out[3]) {   /* render->ENU: (e,n,u)=(x,-z,y) */
    double e = rv[0], uu = rv[1], nn = -rv[2];
    for (int i = 0; i < 3; i++) out[i] = E[i] * e + N[i] * nn + U[i] * uu;
  };
  toEcef(fR, fwd);
  toEcef(srR, right);
  toEcef(upR, up);
}

static void frame(void) {
  double now = emscripten_get_now();
  double dt = LastMs > 0.0 ? (now - LastMs) / 1000.0 : 0.0;
  LastMs = now;
  if (dt > 0.1) dt = 0.1;   /* clamp a stall/tab-switch so the sim doesn't lurch */

  /* BOOT LOADING GATE: until the target cut around the SPAWN is resident, stream + show the loading
   * screen and keep JSBSim FROZEN (no step, no [agl]). Then the scene turns on and the sim begins — so
   * the first flown frame is already full-resolution and the spawn DEM ground is loaded. Boot-only. */
  static bool gLoading = true;
  static double gLoadStart = 0.0;
  if (gLoading) {
    if (gLoadStart == 0.0) gLoadStart = now;
    double leye[3], lfwd[3], lright[3], lup[3];
    geoToEcef(St.lat, St.lon, St.elev, leye);
    cameraBasis(St.yaw, St.pitch, St.roll, St.lat, St.lon, lfwd, lright, lup);
    R.SetCameraBasis(leye, lfwd, lright, lup);
    W.Update(St.lat, St.lon, leye, lfwd, now);
    float pct = W.LoadProgress();
    R.SetLoadingScreen(true, pct, W.TargetReadyN(), W.TargetTotal());
    R.RenderFrame();
    const char *te = getenv("FB_LOAD_THRESH"); float thresh = te ? (float)atof(te) : 0.95f;
    static double lastLoadLog = 0;
    if (now - lastLoadLog > 500.0) { lastLoadLog = now; printf("[loading] %.0f pct (%d/%d tiles)\n", pct * 100.0f, W.TargetReadyN(), W.TargetTotal()); fflush(stdout); }
    const char *toe = getenv("FB_LOAD_TIMEOUT_MS"); double tmo = toe ? atof(toe) : 30000.0;
    bool done = (W.TargetTotal() > 0 && pct >= thresh);
    bool timedOut = (now - gLoadStart > tmo);   /* don't hang if a few tiles 204/miss (or headless never commits) */
    if (done || timedOut) {
      gLoading = false;
      R.SetLoadingScreen(false, 1.0f, 0, 0);
      printf("[loading] %s %.0f pct -> scene on, sim start\n", timedOut ? "TIMEOUT" : "converged", pct * 100.0f); fflush(stdout);
    }
    return;
  }

  /* [cpuprof] (branch performance): wall-clock ms per main-loop section, 1 Hz summary. WebGPU GPU work is
   * async, so RenderFrame's time here is CPU-side command RECORDING + submit, not the GPU render itself. */
  double cp_a = emscripten_get_now();

  /* FDM ground floor = the SAME DEM the renderer draws (crash contract): feed JSBSim the terrain ASL
   * under the aircraft BEFORE stepping, so gear/contact/crash collide against real terrain (~430 m at
   * Grenchen), not init-ASL 0. Only push REAL /elev samples; a cold reply leaves the last good value. */
  double ground = fb_stream_ground(St.lat, St.lon);
  if (ground > -1e8) fb_jsbsim_set_ground(ground);
  /* Until the first /elev lands, fall back to the config ground so AGL/radar-alt show a plausible
   * number rather than raw ASL — the SAME fallback SetAgl already used, now shared with
   * FBRadarAltimeter (FBF16Module::SetGroundAsl) so both read one value, not two DEM paths. */
  double gForHud = ground > -1e8 ? ground : Ground;
  gF16->SetGroundAsl((float)gForHud);
  double cp_b = emscripten_get_now();   /* end: ground/bridges */

  /* Advance the active module: guidance -> FLCS-command -> JSBSim, fixed 100 Hz substeps (spiral
   * guard), plus its own system slots at their own rates (incl. FBPilot, 10 Hz) — through the
   * polymorphic FBModule handle. */
  gActiveModule->Run(St, dt, &W);
  FBGuidance g = gF16->LastGuidance();
  int nSub = gF16->LastSubsteps();
  /* Mission waypoint sequencing: the mechanical half FBPilot::Run relies on but does not itself do
   * (its Climb/Route phases just fly whatever ActiveWaypoint() returns) — a no-op on an empty
   * FlightPlan (?ap=manual boot never sets one). */
  FBMissionAdvanceWaypoint(gF16->FlightPlan(), St.lat, St.lon, kWpCaptureM);
  double cp_c = emscripten_get_now();   /* end: jsbsim substeps */

  /* HUD AGL (ASL - DEM ground) — gForHud computed above, before Run(), so FBRadarAltimeter and this
   * read the same sample. 1 Hz telemetry from THIS sim tick (device-loss-proof). */
  R.SetAgl((float)(St.elev - gForHud));

  /* Camera = the aircraft eye. */
  double eye[3], fwd[3], right[3], up[3];
  geoToEcef(St.lat, St.lon, St.elev, eye);
  cameraBasis(St.yaw, St.pitch, St.roll, St.lat, St.lon, fwd, right, up);
  R.SetCameraBasis(eye, fwd, right, up);

  /* HUD pose from the live sim (ENU offset + home vector, MIL-STD-1787 relative bearing). Seeded from
   * the module's own telemetry chain (FBAirDataSystem/FBRadarAltimeter/FBNavSystem/...), then the
   * App-computed pose/sun/moon fields below overwrite their slice. */
  FBState hs = gF16->Telemetry();
  hs.roll = (float)St.roll; hs.pitch = (float)St.pitch; hs.yaw = (float)St.yaw;
  hs.alt = (float)St.elev; hs.gs = (float)St.gs; hs.airspeed = (float)St.speed; hs.vs = (float)St.vy;
  double coslat = std::cos(Olat * kPi / 180.0);
  double dlon = St.lon - Olon;   /* Wrap180: without it the antimeridian gives a ~360° delta -> HUD DIST 38,171,944 */
  while (dlon > 180.0) dlon -= 360.0;
  while (dlon < -180.0) dlon += 360.0;
  hs.y = (float)((St.lat - Olat) * kMPerDeg);
  hs.x = (float)(dlon * kMPerDeg * coslat);
  hs.home_dist = (float)std::sqrt((double)hs.x * hs.x + (double)hs.y * hs.y);
  double absBrg = std::atan2(-(double)hs.x, -(double)hs.y) * 180.0 / kPi, rel = absBrg - St.yaw;
  while (rel > 180) rel -= 360;
  while (rel < -180) rel += 360;
  hs.home_bearing = (float)rel;
  hs.state = gF16->Autopilot().GetMode();
  /* 1 Hz flight telemetry from the sim tick (device-loss-proof): [agl] + [home] (the HUD home BRG/DIST,
   * antimeridian-safe — the gate's measurement convention). */
  { static double accLog = 0.0; accLog += dt;
    if (accLog >= 1.0) { accLog = 0.0;
      FlightBox::FBLogAgl(St, g.Mode, g.RingDistM, gForHud, fb_jsbsim_get_ground());
      printf("[home] dist=%.0f brg=%.0f hdg=%.0f lon=%.4f\n", hs.home_dist, hs.home_bearing, St.yaw, St.lon);
      printf("[pilot] phase=%d\n", (int)gF16->PilotSystem().GetPhase());   /* Idle in ?ap=manual, else the mission phase machine */
      fflush(stdout); } }
  /* Real ephemeris sun + moon for EVS (the renderer uses them only in photo mode; SVS = constant day). */
  time_t utc = SimUtc ? SimUtc : time(nullptr);
  SunPos(St.lat, St.lon, utc, &hs.sun_el, &hs.sun_az);
  MoonPos(St.lat, St.lon, utc, &hs.moon_el, &hs.moon_az, &hs.moon_phase);
  R.SetSkyClock((double)utc);
  R.SetHud(hs, true);

  double cp_d = emscripten_get_now();   /* end: pose/HUD/ephemeris */

  W.SetNightLights(R.GetGroundMode() && hs.sun_el < -3.0f);   /* EVS night -> stream /t/lights */
  W.Update(St.lat, St.lon, eye, fwd, now);   /* multi-LOD quadtree around the live flight */
  double cp_e = emscripten_get_now();   /* end: FBWorld update (quadtree + gain + lights poll) */

  R.RenderFrame();
  double cp_f = emscripten_get_now();   /* end: render (CPU-side record + submit) */

  { static double aGround = 0, aJsbsim = 0, aPose = 0, aWorld = 0, aRender = 0, aPeriod = 0, acc = 0;
    static long nF = 0, sSub = 0;
    aGround += cp_b - cp_a; aJsbsim += cp_c - cp_b; aPose += cp_d - cp_c;
    aWorld += cp_e - cp_d; aRender += cp_f - cp_e; aPeriod += dt * 1000.0;
    sSub += nSub; nF++; acc += dt;
    if (acc >= 1.0) {
      double loop = aGround + aJsbsim + aPose + aWorld + aRender;
      double per = aPeriod / nF;   /* mean rAF period (ms) — the 1/refresh budget */
      printf("[cpuprof] loop=%.2f ms/f (%.0f%% of %.1fms rAF) | ground=%.2f jsbsim=%.2f pose=%.2f world=%.2f render=%.2f (ms/f) | draws=%d tilebuf=%dB substeps=%.1f/f frames=%ld\n",
             loop / nF, per > 0 ? 100.0 * (loop / nF) / per : 0.0, per,
             aGround / nF, aJsbsim / nF, aPose / nF, aWorld / nF, aRender / nF,
             R.DrawCount(), R.DrawCount() * 32, (double)sSub / nF, nF);
      fflush(stdout);
      aGround = aJsbsim = aPose = aWorld = aRender = aPeriod = acc = 0; nF = 0; sSub = 0;
    }
  }
}

int main() {
  static char base[192];
  const char *ju = emscripten_run_script_string("(window.FB_TILES_URL||'http://localhost:8081').toString()");
  snprintf(base, sizeof base, "%s", ju && ju[0] ? ju : "http://localhost:8081");
  /* emscripten_run_script_string returns a SHARED static buffer — atof each before the next call. */
  const char *jla = emscripten_run_script_string(
      "(typeof window.FB_ORIGIN_LAT==='number'?window.FB_ORIGIN_LAT:47.179846).toString()");
  double olat = (jla && jla[0]) ? atof(jla) : 47.179846;
  const char *jlo = emscripten_run_script_string(
      "(typeof window.FB_ORIGIN_LON==='number'?window.FB_ORIGIN_LON:7.411427).toString()");
  double olon = (jlo && jlo[0]) ? atof(jlo) : 7.411427;
  Olat = olat;
  Olon = olon;
  /* Simulated-UTC override (Unix seconds; 0/unset = real time) — pins a reproducible EVS sky. */
  const char *jutc = emscripten_run_script_string(
      "(typeof window.FB_SIM_UTC==='number'?window.FB_SIM_UTC:0).toString()");
  SimUtc = (time_t)((jutc && jutc[0]) ? atof(jutc) : 0.0);

  const char *vk = getenv("FB_VIEW_KM");
  double viewM = (vk && atof(vk) > 0.0) ? atof(vk) * 1000.0 : 240000.0;   /* far view like the old engine */

  /* Boot mode: default = MISSION WITH PILOT — fetch the default .fbm from fb-sim's own web/missions/
   * (a plain HTTP GET against this page's own origin, not fb-tiles; sim/web/ is up.sh's live mount, so
   * editing the mission needs no rebuild), ground-spawn on its runway via the SAME FBMissionGroundSpawn
   * the native --mission runner uses (FBMissionBoot.h — no duplicated spawn logic), and arm FBPilot at
   * Preflight; FBF16Module::Run() already cycles PilotSys every tick, so that alone flies the takeoff/
   * climb/route chain. ?ap=manual keeps a direct-stick sandbox airborne near the config origin instead
   * (no live HOTAS binding yet — FBInputSystem is still the NoOp default, systems/FBSystemSlots.h; this
   * is the same pass-through FBAutopilot::Manual has always offered). */
  const char *jap = emscripten_run_script_string("(new URLSearchParams(location.search).get('ap')||'')");
  bool manualMode = jap && jap[0] == 'm';   /* only 'manual' is a recognised value */

  if (manualMode) {
    double altAsl = Ground + kAglM;
    double slat = olat + kRadiusM / kMPerDeg, slon = olon;   /* 8 km due N, heading E */
    if (fb_jsbsim_init("/jsbsim/aircraft", "f16", slat, slon, altAsl, 0.0, kSpeedMs, 90.0, 0) != 0) {
      printf("[gpu] JSBSim init FAILED\n");
      return 1;
    }
    gF16->Autopilot().SetManual(0.0, 0.0, 0.0, 0.85);
    printf("[gpu] JSBSim F-16 manual %.4f/%.4f, alt %.0f m ASL, %.0f m/s (direct stick pass-through, no HOTAS bound yet)\n",
           olat, olon, altAsl, kSpeedMs);
  } else {
    static char missionText[8192];
    int n = fb_fetch_text(kDefaultMissionUrl, missionText, sizeof missionText);
    FlightBox::FBMission mission;
    std::string perr;
    if (n <= 0 || !FBParseMissionFile(missionText, mission, &perr)) {
      printf("[gpu] mission boot FAILED (%s): %s\n", kDefaultMissionUrl, n <= 0 ? "fetch (is fb-sim serving web/missions/?)" : perr.c_str());
      return 1;
    }
    const FBRunway &rwy = mission.Runway;
    /* Resolve the REAL DEM ground under the runway threshold before the sat-on-gear IC — a bounded
     * blocking wait (fb_stream_ground is async in WASM; ASYNCIFY already yields main() below for the
     * star-catalogue fetch, same pattern), falling back to the config default on a slow/dead fb-tiles
     * rather than hanging boot. */
    double groundAsl = Ground;
    for (int i = 0; i < 40; i++) {
      double g = fb_stream_ground(rwy.ThresholdLatDeg, rwy.ThresholdLonDeg);
      if (g > -1e8) { groundAsl = g; break; }
      emscripten_sleep(50);
    }
    if (!FBMissionGroundSpawn("/jsbsim/aircraft", mission, groundAsl, *gF16, St)) {
      printf("[gpu] JSBSim init FAILED\n");
      return 1;
    }
    printf("[gpu] mission '%s': ground-spawned hdg=%.0f @ %.5f/%.5f, DEM ground=%.0f m, %d waypoints, FBPilot -> Preflight\n",
           mission.Name.c_str(), rwy.TrueHeadingDeg, rwy.ThresholdLatDeg, rwy.ThresholdLonDeg, groundAsl, mission.Plan.Size());
  }
  fb_jsbsim_step(&St);   /* prime St before the first guidance step */

  /* HUD nav placeholder: one steerpoint 8 nm bearing 060 from the config origin, bullseye AT the origin
   * — a concrete, moving relative bearing so the guide's "diamond in FOV" and "crossed-out" cases both
   * occur across a run. */
  {
    const double kStptBrgDeg = 60.0, kStptRangeNm = 8.0;
    double brgRad = kStptBrgDeg * kPi / 180.0, rangeM = kStptRangeNm * 1852.0;
    double stptLat = olat + (rangeM * std::cos(brgRad)) / kMPerDeg;
    double stptLon = olon + (rangeM * std::sin(brgRad)) / (kMPerDeg * std::cos(olat * kPi / 180.0));
    gF16->Nav().SetSteerpoint(stptLat, stptLon, Ground / 0.3048 + 50.0);
    gF16->Nav().SetBullseye(olat, olon);
  }

  R.SetStreaming(512);
  /* Boot ground mode: default EVS/photo (Esri is the first thing shown; OSM becomes the lazy overlay).
   * ?ground=osm|photo overrides. The chosen mode is BOTH the eager base and the initial view. */
  const char *jg = emscripten_run_script_string(
      "(new URLSearchParams(location.search).get('ground')||'photo')");
  int bootPhoto = !(jg && jg[0] == 'o');   /* 'osm' -> OSM base; anything else -> photo base */
  R.SetDefaultMode(bootPhoto); W.SetDefaultMode(bootPhoto);
  R.SetGroundMode(bootPhoto);  W.SetGroundMode(bootPhoto);
  const char *jms = emscripten_run_script_string(
      "(typeof window.FB_MOON_SCALE==='number'?window.FB_MOON_SCALE:1).toString()");
  if (jms && jms[0]) R.SetMoonScale(atof(jms));
  printf("[gpu] boot ground = %s (eager base + view)\n", bootPhoto ? "photo/EVS" : "osm/SVS");
  /* Real-sky assets (EVS): NASA/GSFC CGI-Moon-Kit LROC albedo (public domain, embedded at /moon.jpg)
   * + the HYG star catalogue from fb-tiles. Optional — a miss leaves a grey moon / no stars, never
   * blocks startup. */
  {
    uint8_t *moon = 0; int mw = 0, mh = 0;
    if (fb_load_image_file("/moon.jpg", &moon, &mw, &mh)) {
      R.SetMoonTexture(moon, mw, mh);
      free(moon);
      printf("[gpu] moon texture %dx%d\n", mw, mh);
    } else printf("[gpu] moon texture missing (/moon.jpg) — grey fallback\n");
    static uint8_t stars[262144];
    int sn = fb_fetch_stars(base, stars, (int)sizeof stars);
    if (sn > 0) { R.SetStars(stars, sn, olat, olon); printf("[gpu] star catalogue %d bytes (%d stars)\n", sn, sn / 6); }
    else printf("[gpu] star catalogue unreachable (%s/t/stars) — blank night sky\n", base);
  }
  R.SetHudDisplay(&gF16->Displays());   /* HUD symbology: the module's Displays slot (default HUD) */
  R.Init("#gpu", 1280, 720);
  if (!W.Open(&R, base, olat, olon, 32, viewM, 512)) {
    printf("[gpu] FBWorld open FAILED — is fb-tiles reachable at %s ?\n", base);
    return 1;
  }
  W.RegisterUnit(&gOwnship);   /* the App owns gOwnship; FBWorld only borrows the pointer (its own banner) */
  printf("[gpu] multi-LOD quadtree around the live flight, view %.1f km\n", viewM / 1000.0);

  emscripten_set_main_loop(frame, 0, 1);
  return 0;
}
