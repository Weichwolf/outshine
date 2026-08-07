/* THE PEDESTRIAN'S FRAME, browser. The same declared scene the native oracle renders, on the same
 * render/ + world/ path — no mission, no units, no menu, no mod scan. What the browser adds over
 * gpu_walk is a swapchain, a main loop and the ONE thing gpu_walk must never have: a viewer who can
 * walk away from the declared standpoint. That control path ends at Renderer::SetCameraBasis, which
 * already takes a complete ECEF basis, so render/ never learns that anyone is steering. */
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <emscripten.h>
#include <emscripten/html5.h>
#include "Renderer.h"
#include "World.h"
#include "Camera.h"
#include "CloudDensity.h"
#include "ConstantWindWeather.h"
#include "ElevationProvider.h"
#include "Ephemeris.h"
#include "Geodesy.h"
#include "Scene.h"
#include "SceneWeather.h"
#include "Snapshot.h"
#include "TerrainLoader.h"
#include "Units.h"
#include "GroundMaterials.h"
#include "VegetationTemplates.h"
#include "Log.h"
#include "LogSinks.h"

using namespace outshine;

namespace {

/* Preloaded into emscripten's virtual FS by the wasm target: the scene and the engine's vegetation
 * table are the only two files this client reads, and both are hard-wired. */
const char *kScenePath = "/scene.json";
const char *kVegetationPath = "/vegetation.json";
const char *kGroundMaterialPath = "/ground-materials.json";

const char *kCanvas = "#gpu";

/* [SET] A relaxed human walk. 1.4 m/s is the pedestrian design speed used for crossing timings, and
 * the ORDER OF MAGNITUDE is all this asks for — nothing in the bench is measured against it. */
constexpr double kWalkSpeedMs = 1.4;
/* [SET] by the owner: Shift covers ground, it does not jog. 100 x 1.4 = 140 m/s — a travel speed for
 * reaching a standpoint, not a gait, and the streaming budget it costs is measured in
 * doc/clients/clients.md rather than assumed. */
constexpr double kRunFactor = 100.0;
/* [SET] Degrees of turn per pixel of locked pointer travel. Its DERIVED consequence is the number
 * that matters: a 180 deg about-face is 180/0.12 = 1500 px, a little over one 1280 px canvas. */
constexpr double kMouseDegPerPx = 0.12;
/* The basis is built from yaw and pitch alone (no roll), so looking straight up is degenerate: the
 * cross product that makes screen-right vanishes. 89 keeps a whole degree of margin. */
constexpr double kMaxPitchDeg = 89.0;
/* A backgrounded tab hands back one enormous dt on return. A walker must not teleport, so the step
 * is capped at 100 ms = 14 cm at walking pace. */
constexpr double kMaxStepS = 0.1;
/* One and a half 60 Hz periods: past this the compositor has skipped at least one vsync. */
constexpr double kLateMs = 25.0;

struct FrameCost {
  double Cpu, World, Mesh, Albedo, Upload, Building, BuildingDecode, Encode;
};
FrameCost gLast{};

Render::Renderer gR;
World::World gW;
Clients::Scene gScene;
World::GroundMaterials gMats;
World::VegetationTemplates gVeg;
State gState{};
double gEye[3], gFwd[3], gRight[3], gUp[3];
double gLastTriLog = 0.0;

/* THE VIEWER'S OWN CAMERA. Position is geodetic and height is NOT stored: the eye rides the DEM, so
 * only the last RESOLVED ground answer is kept and a cold tile leaves the walker at the height it
 * last knew instead of dropping him to sea level. */
struct WalkCam {
  double Lat = 0.0, Lon = 0.0;
  double YawDeg = 0.0, PitchDeg = 0.0;
  double GroundM = 0.0;
};
WalkCam gCam;

enum class Move { Fwd, Back, Left, Right, Count };
bool gHeld[(int)Move::Count] = {};
bool gFast = false;
bool gLocked = false;          /* pointer lock; without it the mouse does not turn the head */
double gMouseDx = 0.0, gMouseDy = 0.0;   /* pixels accumulated since the last frame */
double gPrevMs = 0.0;
/* L is deferred to the END of the frame it was pressed in, because the picture the canvas holds is
 * the one this frame just drew and the browser hands it back only while that task is still running. */
bool gShotPending = false;
unsigned gShotNo = 0;

void ResetToScene(void) {
  gCam.Lat = gScene.Lat();
  gCam.Lon = gScene.Lon();
  gCam.YawDeg = gScene.YawDeg();
  gCam.PitchDeg = gScene.PitchDeg();
}

bool OnKey(int type, const EmscriptenKeyboardEvent *e, void *) {
  if (e->ctrlKey || e->metaKey || e->altKey) return false;   /* Ctrl+R is the browser's, not the walker's */
  const bool down = (type == EMSCRIPTEN_EVENT_KEYDOWN);
  gFast = e->shiftKey;
  if (!strcmp(e->code, "KeyW")) gHeld[(int)Move::Fwd] = down;
  else if (!strcmp(e->code, "KeyS")) gHeld[(int)Move::Back] = down;
  else if (!strcmp(e->code, "KeyA")) gHeld[(int)Move::Left] = down;
  else if (!strcmp(e->code, "KeyD")) gHeld[(int)Move::Right] = down;
  else if (down && !strcmp(e->code, "KeyR")) ResetToScene();
  else if (down && !strcmp(e->code, "KeyL")) gShotPending = true;
  /* Chromium releases the pointer on Escape by itself when the keystroke is a real one, and does not
   * when it was synthesised — which is every automated proof there is. Asking for the exit here makes
   * the release the SAME event in both cases; exiting a lock that is already gone is a no-op. */
  else if (down && !strcmp(e->code, "Escape")) emscripten_exit_pointerlock();
  else return false;   /* F (fullscreen, index.html) and everything else stays the page's */
  return true;
}

/* Deltas are ACCUMULATED and consumed once per frame: the browser can deliver several move events
 * between two rAF callbacks, and integrating each one separately would make the turn rate depend on
 * event density rather than on how far the hand moved. */
bool OnMouseMove(int, const EmscriptenMouseEvent *e, void *) {
  if (!gLocked) return false;
  gMouseDx += (double)e->movementX;
  gMouseDy += (double)e->movementY;
  return true;
}

bool OnClick(int, const EmscriptenMouseEvent *, void *) {
  if (!gLocked) emscripten_request_pointerlock(kCanvas, true);
  return true;
}

/* ESC is never seen here: the browser exits pointer lock itself and only reports the change. */
bool OnLockChange(int, const EmscriptenPointerlockChangeEvent *e, void *) {
  gLocked = e->isActive;
  if (!gLocked) for (int i = 0; i < (int)Move::Count; i++) gHeld[i] = false;   /* no key-up arrives once focus is gone */
  Log::Info("walk", "pointerlock", {{"locked", gLocked}});
  return true;
}

/* One integration step: look first, then walk in the plane the new heading defines. */
void StepCamera(double dtS) {
  if (gLocked && (gMouseDx != 0.0 || gMouseDy != 0.0)) {
    gCam.YawDeg = Wrap180(gCam.YawDeg + gMouseDx * kMouseDegPerPx);
    gCam.PitchDeg -= gMouseDy * kMouseDegPerPx;   /* screen-down is +Y, and pushing the mouse down looks down */
    if (gCam.PitchDeg > kMaxPitchDeg) gCam.PitchDeg = kMaxPitchDeg;
    if (gCam.PitchDeg < -kMaxPitchDeg) gCam.PitchDeg = -kMaxPitchDeg;
  }
  gMouseDx = gMouseDy = 0.0;

  const double f = (gHeld[(int)Move::Fwd] ? 1.0 : 0.0) - (gHeld[(int)Move::Back] ? 1.0 : 0.0);
  const double r = (gHeld[(int)Move::Right] ? 1.0 : 0.0) - (gHeld[(int)Move::Left] ? 1.0 : 0.0);
  if (f == 0.0 && r == 0.0) return;
  const double len = std::sqrt(f * f + r * r);   /* diagonals are not faster */
  const double step = kWalkSpeedMs * (gFast ? kRunFactor : 1.0) * dtS / len;
  /* THE VIEW PLANE IS HORIZONTAL: walking is yaw-only, so looking up does not lift the walker off
   * the ground. Yaw 0 = north, 90 = east (core/Camera.h's convention). */
  const double yaw = gCam.YawDeg * kDeg2Rad;
  const double north = (f * std::cos(yaw) - r * std::sin(yaw)) * step;
  const double east = (f * std::sin(yaw) + r * std::cos(yaw)) * step;
  gCam.Lat += north / kMPerDeg;
  const double coslat = std::cos(gCam.Lat * kDeg2Rad);
  gCam.Lon += east / (kMPerDeg * (coslat > 1e-6 ? coslat : 1e-6));
}

/* THE STANDPOINT LOG (doc/clients/clients.md): one press of L, one line in fb-sim's `shots.jsonl`
 * and the picture beside it. The channel runs in ONE direction — the client posts what it saw and
 * nothing the server holds ever reaches a frame, so no world state hangs off the network.
 *
 * The picture is posted rather than left to be re-rendered: what makes an entry worth having is that
 * it shows what the BROWSER drew, and a server-side re-render would replace exactly the half of the
 * comparison that cannot be reconstructed. The snapshot beside it is what makes the other half
 * reproducible.
 *
 * PNG FIRST, then the line that names it: a reader tailing the log must never reach an entry whose
 * image is not on disk yet. */
void PostShot(void) {
  char name[64];
  const time_t t = time(nullptr);
  struct tm g = {};
  gmtime_r(&t, &g);
  snprintf(name, sizeof name, "shot-%04d%02d%02dT%02d%02d%02dZ-%03u", g.tm_year + 1900, g.tm_mon + 1,
           g.tm_mday, g.tm_hour, g.tm_min, g.tm_sec, ++gShotNo);

  Clients::Snapshot snap;
  snap.SetName(name);
  snap.SetScene(gScene);
  snap.SetCamera(gCam.Lat, gCam.Lon, gCam.YawDeg, gCam.PitchDeg);
  snap.SetDerived(gCam.GroundM, gCam.GroundM + gScene.EyeM(), gState.Env.SunElDeg,
                  gState.Env.SunAzDeg);
  snap.SetClient("wasm", emscripten_get_now());
  const std::string line = snap.Text();

  /* toDataURL is SYNCHRONOUS and stays in this task on purpose: a WebGPU canvas hands its pixels
   * back only until the frame is presented, and a callback would run after that. */
/* `$0` is EM_ASM's only way to reach an argument and -Wpedantic sees a C identifier; the suppression
 * is around the one block that has to say it. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
  EM_ASM({
    var shotName = UTF8ToString($0);
    var shotLine = UTF8ToString($1);
    var shotCanvas = document.getElementById('gpu');
    var shotUrl = shotCanvas.toDataURL('image/png');
    var shotB64 = shotUrl.slice(shotUrl.indexOf(';base64,') + 8);
    var shotBin = atob(shotB64);
    var shotBuf = new Uint8Array(shotBin.length);
    for (var i = 0; i < shotBin.length; i++) shotBuf[i] = shotBin.charCodeAt(i);
    fetch('/shot/' + shotName + '.png', {method: 'POST', body: shotBuf})
      .then(function (r) { return fetch('/shot/' + shotName + '.json', {method: 'POST', body: shotLine}); })
      .then(function (r) { console.log('{"ev":"shot_posted","name":"' + shotName + '","pngBytes":' + shotBuf.length + ',"http":' + r.status + '}'); })
      .catch(function (e) { console.log('{"ev":"shot_failed","name":"' + shotName + '","why":"' + e + '"}'); });
  }, name, line.c_str());
#pragma clang diagnostic pop

  Log::Info("walk", "shot", {{"name", std::string(name)}, {"lat", gCam.Lat}, {"lon", gCam.Lon},
      {"yawDeg", gCam.YawDeg}, {"pitchDeg", gCam.PitchDeg}, {"groundM", gCam.GroundM}});
}

void Frame(void) {
  const double now = emscripten_get_now();
  const double tFrame = now;
  const double dtMs = gPrevMs > 0.0 ? now - gPrevMs : 0.0;
  double dt = dtMs / 1000.0;
  gPrevMs = now;
  if (dt > kMaxStepS) dt = kMaxStepS;
  StepCamera(dt);

  const double g = fb_stream_ground(gCam.Lat, gCam.Lon);
  if (ElevationResolved(g)) gCam.GroundM = g;
  const double altAsl = gCam.GroundM + gScene.EyeM();
  GeoToEcef(gCam.Lat, gCam.Lon, altAsl, gEye);
  CameraBasisEcef(gCam.YawDeg, gCam.PitchDeg, 0.0, gCam.Lat, gCam.Lon, gFwd, gRight, gUp);
  gR.SetCameraBasis(gEye, gFwd, gRight, gUp);
  /* The atmosphere rebases its ground radius off this altitude, so it has to follow the walker. */
  gState.Platform.AltM = (float)altAsl;
  gState.Platform.YawDeg = (float)gCam.YawDeg;
  gState.Platform.PitchDeg = (float)gCam.PitchDeg;
  gR.SetSceneState(gState);

  /* The WIND clock is wall time and the SKY clock is not: the scene declares one moment of the day
   * and the flow over the ground runs anyway. */
  gR.SetWindClock(now * 0.001);
  gW.Update(gCam.Lat, gCam.Lon, gEye, gFwd, now);
  const double tEnc = emscripten_get_now();
  gR.RenderFrame();
  const double encMs = emscripten_get_now() - tEnc;
  const double cpuMs = emscripten_get_now() - tFrame;
  /* A DROPPED FRAME IS ONLY VISIBLE IN THE NEXT CALLBACK'S DELTA, so the verdict is passed forward:
   * what this prints is the frame that took too long, together with what it spent the time on. If
   * `cpuMs` is small and the delta is a whole vsync period, the main thread was not the cause. */
  if (dtMs > kLateMs)
    Log::Debug("walk", "late", {{"deltaMs", dtMs}, {"cpuMs", gLast.Cpu}, {"worldMs", gLast.World},
        {"meshMs", gLast.Mesh}, {"albedoMs", gLast.Albedo}, {"uploadMs", gLast.Upload},
        {"buildingMs", gLast.Building}, {"bDecodeMs", gLast.BuildingDecode},
        {"encodeMs", gLast.Encode},
        {"built", (double)gW.BuiltCount()}, {"draws", gR.DrawCount()}});
  gLast = {cpuMs, gW.UpdateMs(), gW.MeshMs(), gW.AlbedoMs(), gW.UploadMs(), gW.BuildingMs(),
           gW.BuildingDecodeMs(), encMs};
  if (gShotPending) {
    gShotPending = false;
    PostShot();
  }
  if (now - gLastTriLog > 2000.0) {
    gLastTriLog = now;
    Log::Info("walk", "frame", {{"draws", gR.DrawCount()}, {"triangles", (double)gR.TriangleCount()},
        {"buildingVerts", (int)gR.BuildingVertexCount()},
        {"progress", (double)gW.LoadProgress()},
        {"lat", gCam.Lat}, {"lon", gCam.Lon}, {"yawDeg", gCam.YawDeg}, {"pitchDeg", gCam.PitchDeg},
        {"groundM", gCam.GroundM}, {"locked", gLocked}});
  }
}

}  // namespace

int main(void) {
  static Clients::StdoutLogSink gLogSink;
  Log::SetSink(&gLogSink);
  Log::SetLevel(LogLevel::Debug);

  static char base[192];
  const char *ju = emscripten_run_script_string(
      "(window.FB_TILES_URL||'http://localhost:8081').toString()");
  snprintf(base, sizeof base, "%s", ju && ju[0] ? ju : "http://localhost:8081");

  if (!gScene.Load(kScenePath)) {
    Log::Error("walk", "scene_load_failed", {{"path", kScenePath}, {"why", gScene.Error()}});
    return 1;
  }
  const double clk = (double)gScene.UtcS();
  float sunEl = 0.0f, sunAz = 0.0f;
  SunPos(gScene.Lat(), gScene.Lon(), clk, &sunEl, &sunAz);
  Log::Info("walk", "scene", {{"path", gScene.Path()}, {"lat", gScene.Lat()}, {"lon", gScene.Lon()},
      {"eyeM", gScene.EyeM()}, {"yawDeg", gScene.YawDeg()}, {"pitchDeg", gScene.PitchDeg()},
      {"fovDeg", gScene.FovDeg()}, {"utc", gScene.Utc()}, {"utcS", (double)gScene.UtcS()},
      {"windDeg", gScene.WindDeg()}, {"windMs", gScene.WindMs()},
      {"cloudCover", gScene.CloudCover()}, {"sunElDeg", (double)sunEl}, {"sunAzDeg", (double)sunAz}});

  if (!gMats.Load(kGroundMaterialPath)) {
    Log::Error("walk", "ground_materials_failed", {{"path", kGroundMaterialPath}, {"why", gMats.Error()}});
    return 1;
  }
  if (!gVeg.Load(kVegetationPath, gMats)) {
    Log::Error("walk", "vegetation_table_failed", {{"path", kVegetationPath}, {"why", gVeg.Error()}});
    return 1;
  }

  gR.SetVegetationTable(gVeg.Rows(), gVeg.RowBytes());
  gR.SetStreaming(512);
  gR.SetGroundMode(0);   /* OSM classification is where land cover comes from (doc/goal.md §1) */
  gR.SetSkyClock(clk);
  gR.SetLiveSun(true);
  gR.SetWind(gScene.WindDeg(), gScene.WindMs());
  gR.SetFovDeg(gScene.FovDeg());
  {
    uint8_t *moon = 0; int mw = 0, mh = 0;
    if (fb_load_image_file("/moon.jpg", &moon, &mw, &mh)) { gR.SetMoonTexture(moon, mw, mh); free(moon); }
    else Log::Warn("walk", "moon_texture_missing", {{"path", "/moon.jpg"}});
    static uint8_t stars[262144];
    const int sn = fb_fetch_stars(base, stars, (int)sizeof stars);
    if (sn > 0) gR.SetStars(stars, sn, gScene.Lat(), gScene.Lon());
    else Log::Warn("walk", "star_catalogue_unreachable", {{"base", std::string(base)}});
  }
  gR.Init("#gpu", 1280, 720);

  if (!gW.Open(&gR, base, gScene.Lat(), gScene.Lon(), 32, 60000.0, 512)) {
    Log::Error("walk", "world_open_failed", {{"base", std::string(base)}});
    return 1;
  }

  /* WHERE THE FEET ARE. The scene declares a height above ground, so the ground is the DEM's answer.
   * A cold tile is WAITED OUT rather than substituted — a made-up plateau would move the whole
   * picture, and this client renders nothing else. */
  double ground = kFBElevationUnresolved;
  for (int t = 0; t < 200 && !ElevationResolved(ground); t++) {
    ground = fb_stream_ground(gScene.Lat(), gScene.Lon());
    if (!ElevationResolved(ground)) emscripten_sleep(50);
  }
  if (!ElevationResolved(ground)) {
    Log::Error("walk", "ground_unresolved", {{"lat", gScene.Lat()}, {"lon", gScene.Lon()},
        {"base", std::string(base)}});
    return 1;
  }
  const double altAsl = ground + gScene.EyeM();

  ResetToScene();
  gCam.GroundM = ground;
  GeoToEcef(gCam.Lat, gCam.Lon, altAsl, gEye);
  CameraBasisEcef(gCam.YawDeg, gCam.PitchDeg, 0.0, gCam.Lat, gCam.Lon, gFwd, gRight, gUp);

  gState.Platform.AltM = (float)altAsl;
  gState.Platform.YawDeg = (float)gCam.YawDeg;
  gState.Platform.PitchDeg = (float)gCam.PitchDeg;
  gState.Platform.Mode = Mode::Manual;
  /* The sun and moon stay where the SCENE's time and place put them: a walk of a few hundred metres
   * moves neither by a measurable angle, and a sky that drifted with the walker would make two runs
   * from the same declaration incomparable. */
  gState.Env.SunElDeg = sunEl;
  gState.Env.SunAzDeg = sunAz;
  MoonPos(gScene.Lat(), gScene.Lon(), clk, &gState.Env.MoonElDeg, &gState.Env.MoonAzDeg,
          &gState.Env.MoonPhase);
  gState.Env.CloudCover = (float)gScene.CloudCover();
  gR.SetSceneState(gState);
  gR.SetCameraBasis(gEye, gFwd, gRight, gUp);

  /* Keys on the WINDOW, pointer on the CANVAS: WASD must work before the first click, while a turn
   * may only follow a pointer the canvas actually owns. */
  emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, OnKey);
  emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, OnKey);
  emscripten_set_click_callback(kCanvas, nullptr, false, OnClick);
  emscripten_set_mousemove_callback(kCanvas, nullptr, false, OnMouseMove);
  emscripten_set_pointerlockchange_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, false,
                                            OnLockChange);

  /* Borrowed by World for the whole session, so it must outlive main's frame. */
  static Clients::SceneWeather gWind(gScene);
  const WindNed w = gWind.WindNedMs(gScene.Lat(), gScene.Lon(), altAsl);
  /* The field's horizontal origin is the scene's own point, so walking does not carry the sky along. */
  gR.SetCloudSky(CloudSkyFromWeather(gWind, gScene.Lat(), gScene.Lon(), clk,
                                     gScene.Lat(), gScene.Lon()));
  gW.SetVegetation(&gVeg);
  gW.SetGroundMode(0);
  gW.SetSunElevationDeg(sunEl);
  gW.SetWeather(&gWind);
  Log::Info("walk", "stand", {{"groundM", ground}, {"eyeM", gScene.EyeM()}, {"aslM", altAsl},
      {"sunElDeg", (double)sunEl}, {"sunAzDeg", (double)sunAz},
      {"moonElDeg", (double)gState.Env.MoonElDeg}, {"cloudCover", (double)gState.Env.CloudCover},
      {"windN", w.N}, {"windE", w.E}, {"windD", w.D}});

  emscripten_set_main_loop(Frame, 0, 1);
  return 0;
}
