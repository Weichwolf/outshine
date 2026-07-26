/* FBRenderer demo page (Stage 6): the REAL in-process F-16 flies the WebGPU engine. libJSBSim +
 * the module's own pilot (mission phase machine) + FBAutopilot/FBFlightControl run here each
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
#include "FBCamera.h"
#include "FBModuleRegistry.h"
#include "FBMissionBoot.h"
#include "FBMissionMonitor.h"
#include "FBElevationProvider.h"
#include "FBEphemeris.h"
#include "FBGeodesy.h"
#include "FBTerrainLoader.h"
#include "FBSimUnit.h"
#include "FBUnitRegistry.h"
#include "FBLog.h"
#include "FBLogSinks.h"
#include "FBFdm.h"
#include <cstdint>

using namespace FlightBox;


/* FBGuidance's mode as a short HUD/log label — one Manual/Direct/Course switch, shared by every caller
 * that logs g.Mode (the [flight] agl line below) rather than each re-deriving its own ternary. */
static const char *ModeLabel(FBMode m) {
  switch (m) {
    case FBMode::Manual: return "MANUAL";
    case FBMode::Direct: return "DIRECT";
    case FBMode::Course: return "COURSE";
  }
  return "?";
}
static const double kRadiusM = 8000.0;   /* ?ap=manual spawn offset */
static const double kAglM = 1500.0;      /* ?ap=manual spawn height above ground */
static const double kSpeedMs = 220.0;    /* ?ap=manual spawn speed */
static const char *kSandboxModule = "f16";   /* ?ap=manual has no .fbm to name a module — the sandbox
    picks this one by REGISTRY NAME, so even the debug path never names a concrete module type */
static const char *kDefaultMissionUrl = "/missions/payerne-full.fbm";   /* the full autonomous sortie:
    ground start, waypoint loop, landing to a full stop. web/missions/ is a build-time copy of
    sim/missions/ (make wasm) served by fb-sim's web/ mount — editable without a WASM rebuild */

static FBRenderer R;
static FBWorld W;
/* The mission's whole cast (units/FBSimUnit), one entry per `unit` block the .fbm declares: each owns
 * its airframe and the module it is flown by (produced by NAME through FBModuleRegistry — that block's
 * own `module` line, or kSandboxModule for the ?ap=manual sandbox — and held polymorphically, so this
 * file still names no concrete module type), its live fb_fdm_state, the ground truth under it, and BOTH
 * incorruptible judges (core/FBFlightMonitor for the physical K.O., core/FBMissionMonitor for the
 * mission verdict when the block had objectives; ?ap=manual has no plan to judge). The judges are still
 * App-owned and still invisible to the module (FBSimUnit's own banner). A mission trip just logs RESULT
 * to the console — the browser has no process exit.
 *
 * gOwnship is the FIRST actor and only that: the browser has ONE eye and ONE HUD, so the camera and the
 * HUD ride actors[0] while every other actor is stepped alongside it in the same frame loop. */
static FBActorList gActors;
/* The browser's unit registry (units/FBUnitRegistry): the same one object the headless runner builds —
 * filled once at boot, borrowed by every actor's sensors AND by FBWorld's drawing side. */
static FBUnitRegistry gUnits;
static FBSimUnit *gOwnship = nullptr;
/* This loop's monotonic sim-clock, for the monitors' sustain timers (LOC/deep-stall): the browser has no
 * discrete 10 Hz mission tick like the runner, it integrates whatever the rAF period gives it. */
static double gSimT = 0.0;
static constexpr double kConfigGroundM = 430.0;   /* boot fallback until the first real /elev sample */
static double LastMs = 0.0;
static double Olat = 47.179846, Olon = 7.411427;   /* ENU/home origin (config.js) */
static time_t SimUtc = 0;                /* FB_SIM_UTC override; 0 = real wall clock (live sky) */

/* Ground albedo (TAB in index.html eats the key before SDL) — flips SVS(OSM) <-> EVS(photo) on both
 * the streamer (lazy photo fetch) and the renderer (draw layer + which sun). One source of truth. */
static void GroundSet(int photo) {
  R.SetGroundMode(photo);
  W.SetGroundMode(photo);
  FBLog::Info("gpu", "ground_mode", {{"mode", photo ? "photo" : "osm"}});
}
extern "C" EMSCRIPTEN_KEEPALIVE void fb_toggle_ground(void) { GroundSet(!R.GetGroundMode()); }
extern "C" EMSCRIPTEN_KEEPALIVE void fb_set_ground(int photo) { GroundSet(photo); }

/* Camera/geodesy math is SHARED, not per-App: FBGeoToEcef (core/FBGeodesy.h) and FBCameraBasisEcef
 * (render/FBCamera.h, the silent-mirror-critical basis). This file used to carry its own copies of both
 * plus the cross/normalise helpers, character-identical to FBAppNative.cpp's set. */

static void frame(void) {
  double now = emscripten_get_now();
  double dt = LastMs > 0.0 ? (now - LastMs) / 1000.0 : 0.0;
  LastMs = now;
  if (dt > 0.1) dt = 0.1;   /* clamp a stall/tab-switch so the sim doesn't lurch */
  FBLog::SetTime(now / 1000.0);   /* wall-clock seconds since page load — correlates every log line this frame */

  /* BOOT LOADING GATE: until the target cut around the SPAWN is resident, stream + show the loading
   * screen and keep JSBSim FROZEN (no step, no [agl]). Then the scene turns on and the sim begins — so
   * the first flown frame is already full-resolution and the spawn DEM ground is loaded. Boot-only. */
  static bool gLoading = true;
  static double gLoadStart = 0.0;
  if (gLoading) {
    if (gLoadStart == 0.0) gLoadStart = now;
    FBUnitPose lp = gOwnship->GetPose();
    double leye[3], lfwd[3], lright[3], lup[3];
    FBGeoToEcef(lp.LatDeg, lp.LonDeg, lp.ElevM, leye);
    FBCameraBasisEcef(lp.YawDeg, lp.PitchDeg, lp.RollDeg, lp.LatDeg, lp.LonDeg, lfwd, lright, lup);
    R.SetCameraBasis(leye, lfwd, lright, lup);
    W.Update(lp.LatDeg, lp.LonDeg, leye, lfwd, now);
    float pct = W.LoadProgress();
    R.SetLoadingScreen(true, pct, W.TargetReadyN(), W.TargetTotal());
    R.RenderFrame();
    const char *te = getenv("FB_LOAD_THRESH"); float thresh = te ? (float)atof(te) : 0.95f;
    static double lastLoadLog = 0;
    if (now - lastLoadLog > 500.0) {
      lastLoadLog = now;
      FBLog::Info("loading", "progress", {{"pct", (double)(pct * 100.0f)}, {"ready", W.TargetReadyN()}, {"total", W.TargetTotal()}});
    }
    const char *toe = getenv("FB_LOAD_TIMEOUT_MS"); double tmo = toe ? atof(toe) : 30000.0;
    bool done = (W.TargetTotal() > 0 && pct >= thresh);
    bool timedOut = (now - gLoadStart > tmo);   /* don't hang if a few tiles 204/miss (or headless never commits) */
    if (done || timedOut) {
      gLoading = false;
      R.SetLoadingScreen(false, 1.0f, 0, 0);
      FBLog::Info("loading", "done", {{"result", timedOut ? "TIMEOUT" : "converged"}, {"pct", (double)(pct * 100.0f)}});
    }
    return;
  }

  /* [cpuprof] (branch performance): wall-clock ms per main-loop section, 1 Hz summary. WebGPU GPU work is
   * async, so RenderFrame's time here is CPU-side command RECORDING + submit, not the GPU render itself. */
  double cp_a = emscripten_get_now();

  /* FDM ground floor = the SAME DEM the renderer draws (crash contract): feed JSBSim the terrain ASL
   * under the aircraft BEFORE stepping, so gear/contact/crash collide against real terrain (~430 m at
   * Grenchen), not init-ASL 0. A cold /elev reply keeps the last good value — and now keeps it for the
   * HUD/radar-alt path too (FBSimUnit::UpdateGroundAsl): the browser used to fall back to the config
   * constant for the module while the FDM kept the last real sample, so the two disagreed about where
   * the ground was the moment a fetch missed over real terrain. */
  for (auto &a : gActors) a->UpdateGroundAsl(fb_stream_ground(a->State().lat, a->State().lon));
  double gForHud = gOwnship->GroundAslM();
  double cp_b = emscripten_get_now();   /* end: ground/bridges */

  /* Advance EVERY actor in mission order: each module drives guidance -> FLCS-command -> JSBSim at
   * fixed 100 Hz substeps (spiral guard) plus its own system slots at their own rates (incl. FBPilot,
   * 10 Hz) — through the polymorphic FBModule handle the unit holds. The pose barrier after the loop is
   * the same snapshot discipline the headless runner uses (FBUnit::GetPose's contract): every actor
   * integrates against the previous frame's world, so tick order cannot change the outcome. */
  for (auto &a : gActors) { FBLogUnitScope us(a->LogLabel()); a->Run(dt, &gUnits, &W); }
  for (auto &a : gActors) a->PublishPose();
  const fb_fdm_state &St = gOwnship->State();
  FBGuidance g = gOwnship->Module().LastGuidance();
  int nSub = gOwnship->Module().LastSubsteps();
  /* Waypoint sequencing (Akteurs-Verhalten, FBNavSystem's own job — doc/mission-format.md) already ran
   * INSIDE Run() above, module-internal; the App orchestrates nothing mission-specific here. */
  double cp_c = emscripten_get_now();   /* end: jsbsim substeps */

  /* The two incorruptible judges (CLAUDE.md "Kein Cheaten"), fed every frame — same classes/thresholds
   * FBMissionRunner.cpp's gym/native loop uses (core/FBFlightMonitor + core/FBMissionMonitor's own
   * banners): physical K.O. (a trip logs its own FBLog::Error and cuts the engine here; JSBSim's own
   * ground reactions keep running afterwards, "Crash -> Motor aus, kein Freeze") and, only when a
   * mission was actually booted (?ap=manual has none), the mission verdict (self-logs RESULT; a
   * touchdown off the assigned runway also cuts the engine). Neither stops or special-cases the render
   * loop below — "Konsolen-RESULT genügt" in the browser, there is no process exit here. */
  gSimT += dt;
  for (auto &a : gActors) { FBLogUnitScope us(a->LogLabel()); a->RunMonitors(gSimT); }

  /* HUD AGL (ASL - DEM ground) — the unit's own AGL, from the one ground sample above, so
   * FBRadarAltimeter and this read the same number. 1 Hz telemetry from THIS sim tick. */
  R.SetAgl((float)gOwnship->AglM());

  /* Camera = the aircraft eye. */
  FBUnitPose p = gOwnship->GetPose();
  double eye[3], fwd[3], right[3], up[3];
  FBGeoToEcef(p.LatDeg, p.LonDeg, p.ElevM, eye);
  FBCameraBasisEcef(p.YawDeg, p.PitchDeg, p.RollDeg, p.LatDeg, p.LonDeg, fwd, right, up);
  R.SetCameraBasis(eye, fwd, right, up);

  /* HUD pose from the live sim (ENU offset + home vector, MIL-STD-1787 relative bearing): the unit's
   * module telemetry (FBAirDataSystem/FBRadarAltimeter/FBNavSystem/...) with this tick's pose folded
   * in, then the browser-only home/mode/ephemeris fields below. */
  FBState hs = gOwnship->HudState();
  double homeE, homeN;   /* FBEnuOffsetM wraps the longitude: without it the antimeridian gives a
                          * ~360 deg delta -> HUD DIST 38,171,944 (core/FBGeodesy.h) */
  FBEnuOffsetM(Olat, Olon, St.lat, St.lon, homeE, homeN);
  hs.Platform.EastM = (float)homeE;
  hs.Platform.NorthM = (float)homeN;
  hs.Platform.HomeDistM = (float)std::sqrt((double)hs.Platform.EastM * hs.Platform.EastM + (double)hs.Platform.NorthM * hs.Platform.NorthM);
  double absBrg = std::atan2(-(double)hs.Platform.EastM, -(double)hs.Platform.NorthM) * 180.0 / kPi, rel = absBrg - St.yaw;
  while (rel > 180) rel -= 360;
  while (rel < -180) rel += 360;
  hs.Platform.HomeBearingDeg = (float)rel;
  hs.Platform.Mode = gOwnship->Module().Autopilot().GetMode();
  /* 1 Hz flight telemetry from the sim tick (device-loss-proof): [agl] + [home] (the HUD home BRG/DIST,
   * antimeridian-safe — the gate's measurement convention). */
  { static double accLog = 0.0; accLog += dt;
    if (accLog >= 1.0) { accLog = 0.0;
      FBLog::Info("flight", "agl", {{"alt", St.elev}, {"agl", gOwnship->AglM()}, {"ground", gForHud},
          {"fdmGnd", gOwnship->Fdm().GetGroundElevM()}, {"spd", St.speed}, {"cas", St.cas}, {"bank", St.roll},
          {"hdg", St.yaw}, {"vs", St.vy}, {"ringDist", g.RingDistM},
          {"mode", ModeLabel(g.Mode)}});
      FBLog::Info("flight", "home", {{"dist", (double)hs.Platform.HomeDistM}, {"brg", (double)hs.Platform.HomeBearingDeg},
          {"hdg", St.yaw}, {"lon", St.lon}});
      FBLog::Info("pilot", "phase", {{"phase", FBPilot::PhaseName(gOwnship->Module().PilotSystem().GetPhase())}});
    } }
  /* Real ephemeris sun + moon for EVS (the renderer uses them only in photo mode; SVS = constant day). */
  time_t utc = SimUtc ? SimUtc : time(nullptr);
  SunPos(St.lat, St.lon, utc, &hs.Env.SunElDeg, &hs.Env.SunAzDeg);
  MoonPos(St.lat, St.lon, utc, &hs.Env.MoonElDeg, &hs.Env.MoonAzDeg, &hs.Env.MoonPhase);
  R.SetSkyClock((double)utc);
  R.SetHud(hs, true);

  double cp_d = emscripten_get_now();   /* end: pose/HUD/ephemeris */

  W.SetNightLights(R.GetGroundMode() && hs.Env.SunElDeg < -3.0f);   /* EVS night -> stream /t/lights */
  W.Update(p.LatDeg, p.LonDeg, eye, fwd, now);   /* multi-LOD quadtree around the live flight */
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
      FBLog::Debug("cpuprof", "summary", {{"loopMs", loop / nF}, {"pctOfRaf", per > 0 ? 100.0 * (loop / nF) / per : 0.0},
          {"rafMs", per}, {"groundMs", aGround / nF}, {"jsbsimMs", aJsbsim / nF}, {"poseMs", aPose / nF},
          {"worldMs", aWorld / nF}, {"renderMs", aRender / nF}, {"draws", R.DrawCount()},
          {"tilebufB", R.DrawCount() * 32}, {"substepsPerFrame", (double)sSub / nF}, {"frames", (int)nF}});
      aGround = aJsbsim = aPose = aWorld = aRender = aPeriod = acc = 0; nF = 0; sSub = 0;
    }
  }
}

int main() {
  /* Log-Sink = stdout, level Debug (CLAUDE.md: the browser console must not visibly change — every
   * migrated call site used to print unconditionally). */
  static FBStdoutLogSink gLogSink;
  FBLog::SetSink(&gLogSink);
  FBLog::SetLevel(FBLogLevel::Debug);

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
   * editing the mission needs no rebuild), spawn its declarative FBSpawn via the SAME FBMissionSpawnActor
   * the native --mission runner uses (FBMissionBoot.h — no duplicated spawn logic), which arms FBPilot at
   * the phase matching the spawn (Preflight for a ground start, Route directly for an airborne one);
   * the module's own Run() already cycles PilotSys every tick, so that alone flies the mission.
   * ?ap=manual keeps a direct-stick sandbox airborne near the config origin instead (no live HOTAS
   * binding yet — FBInputSystem is still the NoOp default, systems/FBSystemSlots.h; this is the same
   * pass-through FBAutopilot::Manual has always offered).
   * EITHER path resolves its module by NAME through FBModuleRegistry (the mission's `module` line, or
   * kSandboxModule for the sandbox, which has no mission file to declare one) — same resolution, same
   * failure mode as fb-gym: an unknown name stops the boot with a logged error instead of flying
   * something the mission never asked for. */
  const char *jap = emscripten_run_script_string("(new URLSearchParams(location.search).get('ap')||'')");
  bool manualMode = jap && jap[0] == 'm';   /* only 'manual' is a recognised value */

  FBRegisterBuiltinModules();

  if (manualMode) {
    /* The sandbox has no mission file, so it builds its actor by hand — the ONE place this client still
     * touches the IC directly (app/ may: FBFdmBoot.h is app-only). Same end state as the mission path:
     * a spawned airframe + the module flying it, wrapped in one FBSimUnit, just without a plan to
     * judge, hence no FBMissionMonitor. */
    std::unique_ptr<FBModule> module = FBModuleRegistry::Create(kSandboxModule);
    if (!module) {
      FBLog::Error("gpu", "unknown_module", {{"module", kSandboxModule}, {"source", "?ap=manual sandbox"}});
      return 1;
    }
    double altAsl = kConfigGroundM + kAglM;
    double slat = olat + kRadiusM / kMPerDeg, slon = olon;   /* 8 km due N, heading E */
    FBFdmSpawn ic;
    ic.ModelsRoot = "/jsbsim/aircraft"; ic.Aircraft = module->FdmModelName();
    ic.LatDeg = slat; ic.LonDeg = slon; ic.GroundElevM = altAsl;
    ic.HeightOffsetM = 0.0;   /* airborne, no explicit offset — the IC's own provisional margin applies */
    ic.SpeedMs = kSpeedMs; ic.HeadingDeg = 90.0;
    std::unique_ptr<FBFdm> fdm = FBFdmBoot::Spawn(ic);
    if (!fdm) {
      FBLog::Error("gpu", "jsbsim_init_failed");
      return 1;
    }
    module->AttachFdm(*fdm);
    module->Autopilot().SetManual(0.0, 0.0, 0.0, 0.85);
    gActors.push_back(std::make_unique<FBSimUnit>(1, "sandbox", FBUnitKind::Aircraft, FBUnitTeam::Friendly, std::move(fdm),
                                                  std::move(module), fb_fdm_state{}, altAsl));
    FBLog::Info("gpu", "manual_boot", {{"lat", olat}, {"lon", olon}, {"altM", altAsl}, {"speedMs", kSpeedMs}});
  } else {
    static char missionText[8192];
    int n = fb_fetch_text(kDefaultMissionUrl, missionText, sizeof missionText);
    FlightBox::FBMission mission;
    std::string perr;
    if (n <= 0 || !FBParseMissionFile(missionText, mission, &perr)) {
      FBLog::Error("gpu", "mission_boot_failed", {{"url", kDefaultMissionUrl},
          {"reason", n <= 0 ? std::string("fetch (is fb-sim serving web/missions/?)") : perr}});
      return 1;
    }
    /* Every `unit` block the mission declares becomes an actor here, in file order — the browser flies
     * the FIRST one (camera/HUD) and steps the rest alongside it, exactly like the headless runner. */
    for (size_t i = 0; i < mission.Units.size(); i++) {
      const FBSpawn &sp = mission.Units[i].Spawn;
      FBLogUnitScope us(mission.Units.size() > 1 ? mission.Units[i].Id : std::string());
      /* Resolve the REAL DEM ground under the spawn point before the IC — a bounded blocking wait
       * (fb_stream_ground is async in WASM; ASYNCIFY already yields main() below for the star-catalogue
       * fetch, same pattern), falling back to the config default on a slow/dead fb-tiles rather than
       * hanging boot. */
      double groundAsl = kConfigGroundM;
      for (int t = 0; t < 40; t++) {
        double g = fb_stream_ground(sp.LatDeg, sp.LonDeg);
        if (FBElevationResolved(g)) { groundAsl = g; break; }
        emscripten_sleep(50);
      }
      /* The SAME actor spawn the headless runner performs (app/FBMissionBoot.h) — module by registry
       * name, one IC, mission data wired on, its own FBMissionMonitor — so the browser cannot drift
       * from fb-gym's notion of what a mission start IS. */
      std::string serr;
      std::unique_ptr<FBSimUnit> unit =
          FBMissionSpawnActor("/jsbsim/aircraft", mission, i, groundAsl, mission.TimeoutS, &serr);
      if (!unit) {
        FBLog::Error("gpu", "mission_boot_failed", {{"url", kDefaultMissionUrl}, {"reason", serr}});
        return 1;
      }
      FBLog::Info("gpu", "mission_boot", {{"name", mission.Name}, {"hdg", sp.HeadingDeg},
          {"lat", sp.LatDeg}, {"lon", sp.LonDeg}, {"groundM", groundAsl},
          {"waypoints", mission.Units[i].Plan.Size()}});
      gActors.push_back(std::move(unit));
    }
  }
  gOwnship = gActors.front().get();
  for (auto &a : gActors) a->PrimeState();   /* one step each to fill state before the first guidance step */

  /* HUD nav placeholder: one steerpoint 8 nm bearing 060 from the config origin, bullseye AT the origin
   * — a concrete, moving relative bearing so the guide's "diamond in FOV" and "crossed-out" cases both
   * occur across a run. */
  {
    const double kStptBrgDeg = 60.0, kStptRangeNm = 8.0;
    double brgRad = kStptBrgDeg * kDeg2Rad, rangeM = kStptRangeNm * kNmToM;
    double stptLat = olat + (rangeM * std::cos(brgRad)) / kMPerDeg;
    double stptLon = olon + (rangeM * std::sin(brgRad)) / (kMPerDeg * std::cos(olat * kPi / 180.0));
    gOwnship->Module().NavSystem().SetSteerpoint(stptLat, stptLon, kConfigGroundM * kMToFt + 50.0);
    gOwnship->Module().NavSystem().SetBullseye(olat, olon);
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
  FBLog::Info("gpu", "boot_ground", {{"mode", bootPhoto ? "photo/EVS" : "osm/SVS"}});
  /* Real-sky assets (EVS): NASA/GSFC CGI-Moon-Kit LROC albedo (public domain, embedded at /moon.jpg)
   * + the HYG star catalogue from fb-tiles. Optional — a miss leaves a grey moon / no stars, never
   * blocks startup. */
  {
    uint8_t *moon = 0; int mw = 0, mh = 0;
    if (fb_load_image_file("/moon.jpg", &moon, &mw, &mh)) {
      R.SetMoonTexture(moon, mw, mh);
      free(moon);
      FBLog::Info("gpu", "moon_texture", {{"w", mw}, {"h", mh}});
    } else FBLog::Warn("gpu", "moon_texture_missing", {{"path", "/moon.jpg"}});
    static uint8_t stars[262144];
    int sn = fb_fetch_stars(base, stars, (int)sizeof stars);
    if (sn > 0) { R.SetStars(stars, sn, olat, olon); FBLog::Info("gpu", "star_catalogue", {{"bytes", sn}, {"stars", sn / 6}}); }
    else FBLog::Warn("gpu", "star_catalogue_unreachable", {{"base", std::string(base)}});
  }
  R.SetHudDisplay(&gOwnship->Displays());   /* HUD symbology: the module's Displays slot (default HUD) */
  R.Init("#gpu", 1280, 720);
  if (!W.Open(&R, base, olat, olon, 32, viewM, 512)) {
    FBLog::Error("gpu", "world_open_failed", {{"base", std::string(base)}});
    return 1;
  }
  for (auto &a : gActors) gUnits.Register(a.get());   /* the App owns the units; everyone else borrows */
  W.SetUnits(&gUnits);
  FBLog::Info("gpu", "world_ready", {{"viewKm", viewM / 1000.0}});

  emscripten_set_main_loop(frame, 0, 1);
  return 0;
}
