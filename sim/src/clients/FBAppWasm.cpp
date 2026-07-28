/* The browser client: a live in-process F-16 flying the WebGPU engine, camera on the aircraft's eye,
 * FBWorld streaming z14 fb-tiles around it. */
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <memory>
#include <string>
#include <emscripten.h>
#include "FBRenderer.h"
#include "FBWorld.h"
#include "FBCamera.h"
#include "FBModuleRegistry.h"
#include "FBMissionBoot.h"
#include "FBMissionMonitor.h"
#include "FBWeatherBoot.h"
#include "FBCivilTime.h"
#include "FBClockBoot.h"
#include "FBCloudDensity.h"
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
/* A path in emscripten's embedded FS, filled by the wasm target's --embed-file line. */
/* No asset root: the browser embeds the model tree and nothing else, so a mission's `wx fixture` has
 * nothing to resolve against here — live /wx is this client's weather (app/FBWeatherBoot.h). */
static const FlightBox::Missions::FBModelRoots kWasmModelRoots{"/fb/aircraft", ""};
static const char *kDefaultMissionUrl = "/missions/payerne-full.fbm";   /* the full autonomous sortie:
    ground start, waypoint loop, landing to a full stop. web/missions/ is a build-time copy of
    sim/missions/ (make wasm) served by fb-sim's web/ mount — editable without a WASM rebuild */

static Render::FBRenderer R;
static World::FBWorld W;
/* The mission's whole cast, one entry per `unit` block. gOwnship is the FIRST actor and only that: the
 * browser has ONE eye and ONE HUD, so camera and HUD ride actors[0] while the rest are stepped
 * alongside. A mission trip only logs RESULT — the browser has no process exit. */
static Units::FBActorList gActors;
static Units::FBUnitRegistry gUnits;
static std::vector<FBUnitObservation> gRoster;   /* reserved at boot, refilled in place every frame */
static std::vector<const Units::FBSimUnit *> gRosterUnit;   /* index-parallel: whose pose each entry is */
/* Only a mission that declares `identify` has anything to read a range with, and only then is one
 * computed — the same gate the headless runner uses (missions/FBMissionRunner.cpp). */
static bool gNeedRanges = false;
static Units::FBSimUnit *gOwnship = nullptr;
/* Monotonic sim clock for the monitors' sustain timers: this loop has no discrete 10 Hz mission tick,
 * it integrates whatever the rAF period gives it. */
static double gSimT = 0.0;
static constexpr double kConfigGroundM = 430.0;   /* boot fallback until the first real /elev sample */
static double LastMs = 0.0;
static double Olat = 47.179846, Olon = 7.411427;   /* ENU/home origin (config.js) */
static time_t SimUtc = 0;                /* FB_SIM_UTC override; 0 = real wall clock (live sky) */
/* The MISSION's clock, when the .fbm declares one — it outranks nothing, it EXCLUDES the override
 * above (missions/FBClockBoot.h): declaring both is a boot error, not a precedence. */
static Missions::FBMissionClock SimClock;

/* THE BROWSER'S WEATHER, and the one client whose default is LIVE. Three objects and one rule: `gWeather`
 * is what the sim flies in right now (calm until something better exists), `gWxIncoming` is a parsed blob
 * waiting to be adopted, and the adoption happens at the TOP OF A FRAME — never inside one. With
 * ASYNCIFY the frame can be suspended mid-tick (fb_stream_ground yields), so a fetch callback really can
 * land between two substeps; swapping there would fly half a tick in one atmosphere and half in another.
 * A mission that DECLARES `wx` replaces all of this at boot (the precedence rule, app/FBWeatherBoot.h). */
static std::unique_ptr<FlightBox::FBWeatherProvider> gWeather;
static std::unique_ptr<FlightBox::FBFixedWeather> gWxIncoming;
static bool gWxLive = true;    /* cleared once a mission declares its own: a late /wx must not override it */

/* Called BY NAME from the fetch glue below, hence extern "C" (EMSCRIPTEN_KEEPALIVE alone would let the
 * mangled name defeat the export). Takes ownership of `bytes`. */
extern "C" EMSCRIPTEN_KEEPALIVE void fb_wx_ready(uint8_t *bytes, int n) {
  if (!gWxLive) { free(bytes); return; }   /* the mission's own weather already won */
  auto wx = std::make_unique<FlightBox::FBFixedWeather>(bytes, (size_t)n);
  free(bytes);
  if (!wx->Ok()) {
    FBLog::Warn("wx", "live_rejected", {{"bytes", n}, {"reason", "not a well-formed FBWX blob"}});
    return;
  }
  char run[24];
  FBLog::Info("wx", "live_ready", {{"bytes", n}, {"run", FBWxIsoUtc(wx->RunEpoch(), run, sizeof run)},
      {"nx", (int)wx->Header().Nx}, {"ny", (int)wx->Header().Ny},
      {"gridStep", (int)wx->Header().GridStep}, {"fields", (int)wx->Header().FieldCount}});
  gWxIncoming = std::move(wx);
}

/* The /elev lesson applied to weather: an unreachable or broken endpoint is a LOG LINE, never a boot
 * failure — the session goes on flying the calm air it started in. */
extern "C" EMSCRIPTEN_KEEPALIVE void fb_wx_failed(int status) {
  FBLog::Warn("wx", "live_unavailable", {{"status", status}, {"flying", "calm"}});
}

/* ONE request per session: a GFS cycle is valid for six hours and the blob is a pure function of it, so
 * there is nothing to poll for. fetch(), not the synchronous XHR the tile paths use, because this must
 * not block the boot for the seconds a cold /wx can take (§9.9: 7 s through NOMADS). */
EM_JS(void, fb_wx_fetch, (const char *url), {
  var u = UTF8ToString(url);
  fetch(u).then(function (r) {
    if (!r.ok) { _fb_wx_failed(r.status | 0); return null; }
    return r.arrayBuffer();
  }).then(function (buf) {
    if (!buf) return;
    var src = new Uint8Array(buf);
    var p = _malloc(src.length);
    HEAPU8.set(src, p);
    _fb_wx_ready(p, src.length);
  }).catch(function () { _fb_wx_failed(0); });
})

/* One source of truth for SVS(OSM) <-> EVS(photo): the streamer's lazy photo fetch AND the renderer's
 * draw layer + sun choice. */
static void GroundSet(int photo) {
  R.SetGroundMode(photo);
  W.SetGroundMode(photo);
  FBLog::Info("gpu", "ground_mode", {{"mode", photo ? "photo" : "osm"}});
}
extern "C" EMSCRIPTEN_KEEPALIVE void fb_toggle_ground(void) { GroundSet(!R.GetGroundMode()); }
extern "C" EMSCRIPTEN_KEEPALIVE void fb_set_ground(int photo) { GroundSet(photo); }

static void frame(void) {
  double now = emscripten_get_now();
  double dt = LastMs > 0.0 ? (now - LastMs) / 1000.0 : 0.0;
  LastMs = now;
  if (dt > 0.1) dt = 0.1;   /* clamp a stall/tab-switch so the sim doesn't lurch */
  FBLog::SetTime(now / 1000.0);   /* wall-clock seconds since page load — correlates every log line this frame */

  /* THE ATOMIC SWITCH, and the only place it can happen: before this frame reads the wind at all. */
  if (gWxIncoming) {
    gWeather = std::move(gWxIncoming);
    W.SetWeather(gWeather.get());   /* the drawing side borrows the same object, never a copy */
    FBLog::Info("wx", "source", {{"source", "live"}, {"endpoint", "/wx"}});
  }

  /* Boot gate: JSBSim stays FROZEN until the cut around the spawn is resident, so the first flown
   * frame is already full-resolution and the spawn DEM ground is loaded. */
  static bool gLoading = true;
  static double gLoadStart = 0.0;
  if (gLoading) {
    if (gLoadStart == 0.0) gLoadStart = now;
    Units::FBUnitPose lp = gOwnship->GetPose();
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

  /* [cpuprof]: wall-clock ms per main-loop section. GPU work is async, so RenderFrame's time here is
   * CPU-side command recording + submit, not the render. */
  double cp_a = emscripten_get_now();

  /* JSBSim gets the terrain ASL under the aircraft BEFORE stepping, from the same DEM the renderer
   * draws, so gear/contact/crash collide against real terrain. A cold /elev keeps the last good value
   * for BOTH the FDM and the HUD/radar-alt path — they must not disagree about where the ground is. */
  for (auto &a : gActors) a->UpdateGroundAsl(fb_stream_ground(a->State().lat, a->State().lon));
  /* The air mass, from the same seam every client uses. Sampled per frame beside the ground for the same
   * reason: one number, one place, before anything integrates. */
  for (auto &a : gActors)
    a->UpdateWind(gWeather->WindNedMs(a->State().lat, a->State().lon, a->State().elev));
  double gForHud = gOwnship->GroundAslM();
  double cp_b = emscripten_get_now();   /* end: ground/bridges */

  /* Every actor in mission order, then the pose barrier — the same snapshot discipline the headless
   * runner uses, so tick order cannot change the outcome. */
  for (auto &a : gActors) { FBLogUnitScope us(a->LogLabel()); a->Run(dt, &gUnits, &W); }
  for (auto &a : gActors) a->PublishPose();
  const Fdm::fb_fdm_state &St = gOwnship->State();
  Systems::FBGuidance g = gOwnship->Module().LastGuidance();
  int nSub = gOwnship->Module().LastSubsteps();
  double cp_c = emscripten_get_now();   /* end: jsbsim substeps */

  /* The same two incorruptible judges the headless runner feeds, on the same classes and thresholds —
   * neither stops or special-cases the render loop below; a console RESULT is the whole verdict here. */
  gSimT += dt;
  gRoster.clear();
  gRosterUnit.clear();
  for (auto &a : gActors)
    if (a->GetKind() != Units::FBUnitKind::Weapon) {
      /* The radiating bit, off the signature this unit publishes at the barrier — the same
       * construction the two bits before it use, and the same one FBMissionRunner fills. */
      bool emitting = false;
      const Units::FBUnitSignature sig = a->GetSignature();
      for (int bi = 0; bi < kMaxEmitterBeams; bi++)
        emitting = emitting || sig.Radar[bi].Mode != FBEmitterMode::None;
      gRoster.push_back({a->GetName().c_str(), a->GetTeam(), a->Health().CombatEffective(),
                         a->ReleasedWeapon(), emitting, std::numeric_limits<double>::infinity()});
      gRosterUnit.push_back(a.get());
    }
  FBMissionRoster roster{gRoster.data(), (int)gRoster.size()};
  for (auto &a : gActors) {
    FBLogUnitScope us(a->LogLabel());
    if (gNeedRanges) {
      Units::FBUnitPose self = a->GetPose();
      for (size_t i = 0; i < gRoster.size(); i++) {
        Units::FBUnitPose q = gRosterUnit[i]->GetPose();
        gRoster[i].RangeM = FBPlanarDistM(self.LatDeg, self.LonDeg, q.LatDeg, q.LonDeg);
      }
    }
    a->RunMonitors(gSimT, roster);
  }

  R.SetAgl((float)gOwnship->AglM());   /* the unit's own AGL, so FBRadarAltimeter and the HUD agree */

  /* Camera = the aircraft eye. */
  Units::FBUnitPose p = gOwnship->GetPose();
  double eye[3], fwd[3], right[3], up[3];
  FBGeoToEcef(p.LatDeg, p.LonDeg, p.ElevM, eye);
  FBCameraBasisEcef(p.YawDeg, p.PitchDeg, p.RollDeg, p.LatDeg, p.LonDeg, fwd, right, up);
  R.SetCameraBasis(eye, fwd, right, up);

  /* The module's own telemetry, plus the browser-only home/mode/ephemeris fields below. */
  FBState hs = gOwnship->HudState();
  double homeE, homeN;   /* FBEnuOffsetM wraps the longitude: without it the antimeridian gives a
                          * ~360 deg delta -> HUD DIST 38,171,944 */
  FBEnuOffsetM(Olat, Olon, St.lat, St.lon, homeE, homeN);
  hs.Platform.EastM = (float)homeE;
  hs.Platform.NorthM = (float)homeN;
  hs.Platform.HomeDistM = (float)std::sqrt((double)hs.Platform.EastM * hs.Platform.EastM + (double)hs.Platform.NorthM * hs.Platform.NorthM);
  double absBrg = std::atan2(-(double)hs.Platform.EastM, -(double)hs.Platform.NorthM) * 180.0 / kPi, rel = absBrg - St.yaw;
  while (rel > 180) rel -= 360;
  while (rel < -180) rel += 360;
  hs.Platform.HomeBearingDeg = (float)rel;
  hs.Platform.Mode = gOwnship->Module().Autopilot().GetMode();
  /* 1 Hz flight telemetry from the sim tick, device-loss-proof: the gate's measurement convention. */
  { static double accLog = 0.0; accLog += dt;
    if (accLog >= 1.0) { accLog = 0.0;
      FBLog::Info("flight", "agl", {{"alt", St.elev}, {"agl", gOwnship->AglM()}, {"ground", gForHud},
          {"fdmGnd", gOwnship->Fdm() ? gOwnship->Fdm()->GetGroundElevM() : 0.0}, {"spd", St.speed}, {"cas", St.cas}, {"bank", St.roll},
          {"hdg", St.yaw}, {"vs", St.vy}, {"ringDist", g.RingDistM},
          {"mode", ModeLabel(g.Mode)}});
      FBLog::Info("flight", "home", {{"dist", (double)hs.Platform.HomeDistM}, {"brg", (double)hs.Platform.HomeBearingDeg},
          {"hdg", St.yaw}, {"lon", St.lon}});
      FBLog::Info("pilot", "phase", {{"phase", Pilot::FBPilot::PhaseName(gOwnship->Module().PilotSystem().GetPhase())}});
    } }
  /* Used only in photo mode; SVS pins a constant day. */
  double utc = SimClock.Have ? SimClock.At(gSimT) : (double)(SimUtc ? SimUtc : time(nullptr));
  FBSunPos(St.lat, St.lon, utc, &hs.Env.SunElDeg, &hs.Env.SunAzDeg);
  FBMoonPos(St.lat, St.lon, utc, &hs.Env.MoonElDeg, &hs.Env.MoonAzDeg, &hs.Env.MoonPhase);
  R.SetSkyClock(utc);
  /* Same seam as the native oracle: the CLIENT samples the weather where the camera is and hands the
   * renderer the resulting decks — the renderer never sees a provider. core/FBCloudDensity.h. */
  if (gWeather) {
    const FBCloudSky sky = FBCloudSkyFromWeather(*gWeather, p.LatDeg, p.LonDeg, gSimT);
    R.SetCloudSky(sky);
    hs.Env.CloudLow = sky.Deck[0].Cover;
    hs.Env.CloudMid = sky.Deck[1].Cover;
    hs.Env.CloudHigh = sky.Deck[2].Cover;
    hs.Env.CloudCover = std::max(sky.Deck[0].Cover, std::max(sky.Deck[1].Cover, sky.Deck[2].Cover));
    hs.Env.CloudBaseAglM = sky.Deck[0].Cover > 0.0f ? sky.Deck[0].BaseM : 0.0f;
  }
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
  /* Debug level: the browser console must not visibly change. */
  static Clients::FBStdoutLogSink gLogSink;
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

  /* Default boot = the mission with its pilot, spawned through the SAME FBMissionSpawnActor the native
   * runner uses; ?ap=manual is a direct-stick sandbox instead. Either way the module is resolved by
   * REGISTRY NAME, so an unknown name stops the boot rather than flying something unasked for. */
  const char *jap = emscripten_run_script_string("(new URLSearchParams(location.search).get('ap')||'')");
  bool manualMode = jap && jap[0] == 'm';   /* only 'manual' is a recognised value */

  Modules::FBRegisterBuiltinModules();

  /* Still air is what the session STARTS in, whatever it ends up flying: the fetch below is in flight
   * while the first frames already run, and a null provider would be a branch in the tick path. */
  gWeather = std::make_unique<FBCalmWeather>();

  if (manualMode) {
    /* No mission file, so this builds its actor by hand — the ONE place this client touches the IC
     * directly, and it ends in the same state minus a plan to judge (hence no FBMissionMonitor). */
    std::unique_ptr<Modules::FBModule> module = Modules::FBModuleRegistry::Create(kSandboxModule);
    if (!module) {
      FBLog::Error("gpu", "unknown_module", {{"module", kSandboxModule}, {"source", "?ap=manual sandbox"}});
      return 1;
    }
    double altAsl = kConfigGroundM + kAglM;
    double slat = olat + kRadiusM / kMPerDeg, slon = olon;   /* 8 km due N, heading E */
    Fdm::FBFdmSpawn ic;
    ic.ModelsRoot = kWasmModelRoots.Aircraft;
    ic.Aircraft = module->FdmModelName();
    ic.LatDeg = slat; ic.LonDeg = slon; ic.GroundElevM = altAsl;
    ic.HeightOffsetM = 0.0;   /* airborne, no explicit offset — the IC's own provisional margin applies */
    ic.SpeedMs = kSpeedMs; ic.HeadingDeg = 90.0;
    std::unique_ptr<Fdm::FBFdm> fdm = Fdm::FBFdmBoot::Spawn(ic);
    if (!fdm) {
      FBLog::Error("gpu", "jsbsim_init_failed");
      return 1;
    }
    module->AttachFdm(*fdm);
    module->Autopilot().SetManual(0.0, 0.0, 0.0, 0.85);
    gActors.push_back(std::make_unique<Units::FBSimUnit>(1, "sandbox", Units::FBUnitKind::Aircraft, FBUnitTeam::Friendly, std::move(fdm),
                                                  std::move(module), Fdm::fb_fdm_state{}, altAsl));
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
    for (size_t i = 0; i < mission.Units.size(); i++) {
      const FBSpawn &sp = mission.Units[i].Spawn;
      FBLogUnitScope us(mission.Units.size() > 1 ? mission.Units[i].Id : std::string());
      /* The real DEM ground before the IC. Bounded blocking wait (fb_stream_ground is async in WASM),
       * falling back to the config default rather than hanging boot on a slow or dead fb-tiles. */
      double groundAsl = kConfigGroundM;
      for (int t = 0; t < 40; t++) {
        double g = fb_stream_ground(sp.LatDeg, sp.LonDeg);
        if (FBElevationResolved(g)) { groundAsl = g; break; }
        emscripten_sleep(50);
      }
      /* The SAME spawn the headless runner performs, so the browser cannot drift from fb-gym's notion
       * of what a mission start IS. */
      std::string serr;
      std::unique_ptr<Units::FBSimUnit> unit =
          FBMissionSpawnActor(kWasmModelRoots, mission, i, groundAsl, mission.TimeoutS, &serr);
      if (!unit) {
        FBLog::Error("gpu", "mission_boot_failed", {{"url", kDefaultMissionUrl}, {"reason", serr}});
        return 1;
      }
      FBLog::Info("gpu", "mission_boot", {{"name", mission.Name}, {"hdg", sp.HeadingDeg},
          {"lat", sp.LatDeg}, {"lon", sp.LonDeg}, {"groundM", groundAsl},
          {"waypoints", mission.Units[i].Plan.Size()}});
      gActors.push_back(std::move(unit));
    }
    for (const auto &u : mission.Units)
      for (const auto &o : u.Objectives)
        gNeedRanges = gNeedRanges || o.Kind == FBObjectiveKind::Identify;
    /* The mission's CLOCK, resolved in the one place all three clients share. A file that declares
     * `time` while window.FB_SIM_UTC is set stops the boot: a sky nobody asked for is worse than no
     * picture (missions/FBClockBoot.h). */
    std::string clkErr;
    if (!Missions::FBResolveMissionClock(mission, SimUtc != 0, SimClock, &clkErr)) {
      FBLog::Error("gpu", "mission_boot_failed", {{"url", kDefaultMissionUrl}, {"reason", clkErr}});
      return 1;
    }
    if (SimClock.Have) {
      char iso[21];
      FBLog::Info("gpu", "mission_clock", {{"utc", FBFormatIsoUtc(SimClock.T0S, iso, sizeof iso)}});
    }
    /* THE PRECEDENCE RULE (app/FBWeatherBoot.h): a mission that declares its weather keeps it, in the
     * browser exactly as in fb-gym. Only a mission that does NOT gets this client's live default. */
    std::string werr;
    std::unique_ptr<FBWeatherProvider> declared = FBMakeMissionWeather(mission, kWasmModelRoots, &werr);
    if (declared) {
      gWeather = std::move(declared);
      gWxLive = false;   /* nothing fetched, and a late arrival could not override it either */
      FBLog::Info("wx", "source", {{"source", "mission"}, {"url", kDefaultMissionUrl}});
    } else if (!werr.empty()) {
      /* The browser embeds no fixture (live is its default), so a fixture mission degrades to live
       * rather than refusing to fly — the opposite of fb-gym, where the declaration IS the measurement. */
      FBLog::Warn("wx", "mission_weather_unavailable", {{"reason", werr}, {"falling_back", "live /wx"}});
    }
  }

  /* ONE /wx per session, off the same fb-tiles base the tiles come from, started as early as there is
   * something to hand the result to and never waited for. Until it lands (or does not) the sim flies
   * calm — the first frames are already running by then. */
  if (gWxLive) {
    static char wxUrl[224];
    snprintf(wxUrl, sizeof wxUrl, "%s/wx", base);
    FBLog::Info("wx", "source", {{"source", "calm"}, {"pending", wxUrl}});
    fb_wx_fetch(wxUrl);
  }

  gOwnship = gActors.front().get();
  gRoster.reserve(gActors.size());
  for (auto &a : gActors) a->PrimeState();   /* one step each to fill state before the first guidance step */

  /* HUD nav placeholder: a concrete moving relative bearing, so the guide's "diamond in FOV" and
   * "crossed-out" cases both occur across a run. */
  {
    const double kStptBrgDeg = 60.0, kStptRangeNm = 8.0;
    double brgRad = kStptBrgDeg * kDeg2Rad, rangeM = kStptRangeNm * kNmToM;
    double stptLat = olat + (rangeM * std::cos(brgRad)) / kMPerDeg;
    double stptLon = olon + (rangeM * std::sin(brgRad)) / (kMPerDeg * std::cos(olat * kPi / 180.0));
    gOwnship->Module().NavSystem().SetSteerpoint(stptLat, stptLon, kConfigGroundM * kMToFt + 50.0);
    gOwnship->Module().NavSystem().SetBullseye(olat, olon);
  }

  R.SetStreaming(512);
  /* The chosen mode is BOTH the eager base and the initial view. */
  const char *jg = emscripten_run_script_string(
      "(new URLSearchParams(location.search).get('ground')||'photo')");
  int bootPhoto = !(jg && jg[0] == 'o');   /* 'osm' -> OSM base; anything else -> photo base */
  R.SetDefaultMode(bootPhoto); W.SetDefaultMode(bootPhoto);
  R.SetGroundMode(bootPhoto);  W.SetGroundMode(bootPhoto);
  const char *jms = emscripten_run_script_string(
      "(typeof window.FB_MOON_SCALE==='number'?window.FB_MOON_SCALE:1).toString()");
  if (jms && jms[0]) R.SetMoonScale(atof(jms));
  FBLog::Info("gpu", "boot_ground", {{"mode", bootPhoto ? "photo/EVS" : "osm/SVS"}});
  /* NASA/GSFC CGI-Moon-Kit LROC albedo (public domain) + the HYG catalogue. Optional: a miss leaves a
   * grey moon / no stars and never blocks startup. */
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
  W.SetWeather(gWeather.get());   /* the DATA side only; the frame swap below re-points it when /wx lands */
  FBLog::Info("gpu", "world_ready", {{"viewKm", viewM / 1000.0}});

  emscripten_set_main_loop(frame, 0, 1);
  return 0;
}
