/* THE PEDESTRIAN'S FRAME, browser — the entry point and the output medium, and nothing else. What it
 * shows is Outshine's, exactly as the native oracle's is; what the browser adds is a swapchain, a
 * main loop and a viewer who can walk away from the declared standpoint. */
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>
#include <emscripten.h>
#include <emscripten/html5.h>

#include "Env.h"
#include "Log.h"
#include "LogSinks.h"
#include "Mod.h"
#include "RunIdentity.h"
#include "SceneRunner.h"
#include "ServerArtifacts.h"
#include "Outshine.h"
#include "ServerLog.h"
#include "ServerTelemetry.h"
#include "Snapshot.h"
#include "Walker.h"

using namespace outshine;

namespace {

/* Preloaded into emscripten's virtual FS by the wasm target. `/mods` is the SAME directory the
 * native oracle reads off disk, so `<mod> <scene>` means the same two words in both translations. */
const char *kModRoot = "/mods";
const char *kCanvas = "#gpu";
/* THE SPECIES DIRECTORY IS MOUNTED WHOLE, at the SAME relative path the native oracle reads off
 * disk: a subject run names a species and the two translations have to resolve that name to one
 * file. A single preloaded `/species.json` could only ever serve one species, and `subject-beech`
 * died on `subject_species_unreadable` because of it. */
const Clients::Outshine::Assets kAssets{"/vegetation.json", "/ground-materials.json",
                                        "assets/world/species/beech.json", "/moon.jpg"};

/* One and a half 60 Hz periods: past this the compositor has skipped at least one vsync. */
constexpr double kLateMs = 25.0;
constexpr double kLogEveryMs = 2000.0;

Clients::Mod gMod;
const Clients::Scene *gScene = nullptr;
std::unique_ptr<Clients::Outshine> gApp;
std::unique_ptr<Clients::ServerLog> gLog;
std::unique_ptr<Clients::ServerTelemetry> gTelemetry;
std::unique_ptr<Clients::ServerArtifacts> gArtifacts;
std::unique_ptr<Clients::RunIdentity> gIdentity;
std::string gSimUrl;
Clients::Walker gWalker;

double gPrevMs = 0.0, gLastLogMs = 0.0, gLastCpuMs = 0.0, gLastEncodeMs = 0.0;
bool gLocked = false;          /* pointer lock; without it the mouse does not turn the head */
/* L is deferred to the END of the frame it was pressed in, because the picture the canvas holds is
 * the one this frame just drew and the browser hands it back only while that task is still running. */
bool gShotPending = false;
unsigned gShotNo = 0;

Clients::Outshine::Stance DeclaredStance(void) {
  return {gScene->Lat(), gScene->Lon(), gScene->YawDeg(), gScene->PitchDeg()};
}

bool OnKey(int type, const EmscriptenKeyboardEvent *e, void *) {
  if (e->ctrlKey || e->metaKey || e->altKey) return false;   /* Ctrl+R is the browser's, not the walker's */
  const bool down = (type == EMSCRIPTEN_EVENT_KEYDOWN);
  gWalker.SetFast(e->shiftKey);
  using Move = Clients::Walker::Move;
  if (!strcmp(e->code, "KeyW")) gWalker.Hold(Move::Fwd, down);
  else if (!strcmp(e->code, "KeyS")) gWalker.Hold(Move::Back, down);
  else if (!strcmp(e->code, "KeyA")) gWalker.Hold(Move::Left, down);
  else if (!strcmp(e->code, "KeyD")) gWalker.Hold(Move::Right, down);
  else if (down && !strcmp(e->code, "KeyR")) gWalker.Reset(DeclaredStance());
  else if (down && !strcmp(e->code, "KeyL")) gShotPending = true;
  /* Chromium releases the pointer on Escape by itself when the keystroke is a real one, and does not
   * when it was synthesised — which is every automated proof there is. Asking for the exit here makes
   * the release the SAME event in both cases; exiting a lock that is already gone is a no-op. */
  else if (down && !strcmp(e->code, "Escape")) emscripten_exit_pointerlock();
  else return false;   /* F (fullscreen, index.html) and everything else stays the page's */
  return true;
}

bool OnMouseMove(int, const EmscriptenMouseEvent *e, void *) {
  if (!gLocked) return false;
  gWalker.AddLook((double)e->movementX, (double)e->movementY);
  return true;
}

bool OnClick(int, const EmscriptenMouseEvent *, void *) {
  if (!gLocked) emscripten_request_pointerlock(kCanvas, true);
  return true;
}

/* ESC is never seen here: the browser exits pointer lock itself and only reports the change. */
bool OnLockChange(int, const EmscriptenPointerlockChangeEvent *e, void *) {
  gLocked = e->isActive;
  if (!gLocked) gWalker.ReleaseAll();   /* no key-up arrives once focus is gone */
  Log::Info("walk", "pointerlock", {{"locked", gLocked}});
  return true;
}

/* THE STANDPOINT LOG: one press of L, one line in fb-sim's `shots.jsonl`
 * and the picture beside it. The channel runs in ONE direction — the client posts what it saw and
 * nothing the server holds ever reaches a frame, so no world state hangs off the network.
 *
 * The picture is posted rather than left to be re-rendered: what makes an entry worth having is that
 * it shows what the BROWSER drew, and a server-side re-render would replace exactly the half of the
 * comparison that cannot be reconstructed.
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
  snap.SetScene(*gScene);
  snap.SetCamera(gApp->Lat(), gApp->Lon(), gApp->YawDeg(), gApp->PitchDeg());
  const Clients::Outshine::Counters c = gApp->Measured();
  snap.SetDerived(c.GroundAslM, c.AltAslM, gApp->SunElDeg(), gApp->SunAzDeg());
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

  Log::Info("walk", "shot", {{"name", std::string(name)}, {"lat", gApp->Lat()},
      {"lon", gApp->Lon()}, {"yawDeg", gApp->YawDeg()}, {"pitchDeg", gApp->PitchDeg()},
      {"groundM", c.GroundAslM}});
}

void Frame(void) {
  const double now = emscripten_get_now();
  const double dtMs = gPrevMs > 0.0 ? now - gPrevMs : 0.0;
  gPrevMs = now;

  gApp->Look(gWalker.Step(dtMs / 1000.0));
  /* The WIND clock is wall time and the SKY clock is not: the scene declares one moment of the day
   * and the flow over the ground runs anyway. */
  gApp->SetWindClock(now * 0.001);
  const Clients::Outshine::Progress p = gApp->Stream(now);
  const double tEnc = emscripten_get_now();
  gApp->Frame();
  const double encMs = emscripten_get_now() - tEnc;

  /* A DROPPED FRAME IS ONLY VISIBLE IN THE NEXT CALLBACK'S DELTA, so the verdict is passed forward:
   * what this prints is the frame that took too long, together with what it spent the time on. */
  const Clients::Outshine::Counters c = gApp->Measured();
  if (dtMs > kLateMs)
    Log::Debug("walk", "late", {{"deltaMs", dtMs}, {"cpuMs", gLastCpuMs},
        {"worldMs", c.WorldMs}, {"meshMs", c.MeshMs}, {"albedoMs", c.AlbedoMs},
        {"uploadMs", c.UploadMs}, {"buildingMs", c.BuildingMs},
        {"bDecodeMs", c.BuildingDecodeMs}, {"encodeMs", gLastEncodeMs},
        {"built", (double)c.Built}, {"draws", c.Draws}});
  gLastCpuMs = emscripten_get_now() - now;
  gLastEncodeMs = encMs;

  if (gShotPending) {
    gShotPending = false;
    PostShot();
  }
  if (now - gLastLogMs > kLogEveryMs) {
    gLastLogMs = now;
    Log::Info("walk", "frame", {{"draws", c.Draws}, {"triangles", (double)c.Triangles},
        {"buildingVerts", (int)c.BuildingVerts}, {"treeTris", (double)c.TreeTriangles},
        {"treeStands", (double)c.TreeStands},
        {"progress", (double)p.Fraction}, {"resident", p.Resident},
        {"lat", gApp->Lat()}, {"lon", gApp->Lon()}, {"yawDeg", gApp->YawDeg()},
        {"pitchDeg", gApp->PitchDeg()}, {"groundM", c.GroundAslM}, {"locked", gLocked}});
  }
}

void BindInput(void) {
  /* Keys on the WINDOW, pointer on the CANVAS: WASD must work before the first click, while a turn
   * may only follow a pointer the canvas actually owns. */
  emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, OnKey);
  emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, OnKey);
  emscripten_set_click_callback(kCanvas, nullptr, false, OnClick);
  emscripten_set_mousemove_callback(kCanvas, nullptr, false, OnMouseMove);
  emscripten_set_pointerlockchange_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, false,
                                            OnLockChange);
}

/* THE PAGE IS THE ENVIRONMENT here, exactly as getenv is natively (Env.h): which mod, which scene,
 * where the tiles are, which build this is. Nothing about the world comes through this door. */
std::string PageValue(const char *expr, const char *fallback) {
  const char *js = emscripten_run_script_string(expr);
  return js && js[0] ? std::string(js) : std::string(fallback);
}

/* Boot in its own function so main() stays an entry point (verify-clients, F.3). */
bool Boot(void) {
  const std::string modName = PageValue("(window.FB_MOD||'demo').toString()", "demo");
  const std::string sceneId = PageValue("(window.FB_SCENE||'walk').toString()", "walk");
  if (!gMod.Load(kModRoot, modName)) {
    Log::Error("run", "mod_load_failed", {{"why", gMod.Error()}});
    return false;
  }
  gScene = gMod.Find(sceneId);
  if (!gScene) {
    Log::Error("run", "unknown_scene", {{"mod", modName}, {"asked", sceneId},
        {"has", gMod.Ids()}});
    return false;
  }
  gSimUrl = PageValue("(window.FB_SIM_URL||'').toString()", "");
  gLog = std::make_unique<Clients::ServerLog>(
      gSimUrl,
      Clients::ServerLog::Identity{modName, sceneId, "wasm",
                                   PageValue("(window.FB_BUILD||'').toString()", ""),
                                   PageValue("location.host", "")});
  static Clients::CompositeLogSink both;
  static Clients::StdoutLogSink console;
  both.Add(&console);
  both.Add(gLog.get());
  Log::SetSink(&both);

  /* THE BROWSER VERSION IS PART OF EVERY MEASUREMENT (CLAUDE.md), and only the page can say it. */
  const Clients::Scene::Resolution &res = gScene->RenderResolution();
  gIdentity = std::make_unique<Clients::RunIdentity>(Clients::RunIdentity::Fields{
      modName, sceneId, "wasm", PageValue("(window.FB_BUILD||'').toString()", ""),
      PageValue("navigator.userAgent", ""), res.Width, res.Height});
  gTelemetry = std::make_unique<Clients::ServerTelemetry>(gSimUrl, gLog->RunId());
  gArtifacts = std::make_unique<Clients::ServerArtifacts>(gSimUrl, gLog->RunId());
  gApp = std::make_unique<Clients::Outshine>(*gScene, kAssets);
  gApp->SetTelemetryIdentity(gIdentity.get());
  gApp->SetTelemetrySink(gTelemetry.get());
  gApp->SetTilesBase(PageValue("(window.FB_TILES_URL||'http://localhost:8081').toString()",
                               "http://localhost:8081"));
  /* The canvas is the target either way — the offscreen path is native Dawn's and there is no second
   * one here — and it contributes nothing but its own size to scale to. Only PREPARE here: the
   * subject bench stops between the two phases (Outshine.h), and it is the run that decides. */
  return gApp->Prepare({kCanvas});
}

/* THE DECLARED RUN, IN THE BROWSER. Same SceneRunner, same order, same numbers to compare against —
 * only the destination of the products differs, and that is the whole point of Artifacts.h. */
void Record(void) {
  Clients::SceneRunner runner(*gApp, *gScene, *gArtifacts);
  const int rc = runner.IsSubjectBench() ? runner.RunSubject()
                 : gApp->Open()          ? runner.Run()
                                         : 1;
  Log::Info("run", "finished", {{"rc", rc}, {"runId", gLog->RunId()}});
  gTelemetry->Flush();
  gLog->Flush();
/* `$0` is EM_ASM's only way to reach an argument and -Wpedantic sees a C identifier. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
  EM_ASM({ window.FB_RUN_DONE = $0; }, rc);
#pragma clang diagnostic pop
}

}  // namespace

int main(void) {
  static Clients::StdoutLogSink boot;
  Log::SetSink(&boot);
  Log::SetLevel(LogLevel::Debug);
  if (!Boot()) return 1;
  if (gScene->What() == Clients::Scene::Kind::Run) {
    Record();
    return 0;
  }
  /* SZENE LADEN, DANN SPIELEN. The loop below starts when the world is there and then runs
   * through; what streams in afterwards arrives beside it. */
  if (!gApp->Open() || !gApp->Load()) return 1;
  gWalker.Reset(DeclaredStance());
  BindInput();
  emscripten_set_main_loop(Frame, 0, 1);
  return 0;
}
