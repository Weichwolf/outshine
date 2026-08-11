/* THE PEDESTRIAN'S FRAME, browser — the entry point and the output medium, and nothing else. What it
 * shows is Outshine's, exactly as the native oracle's is; what the browser adds is a swapchain, a
 * main loop and a viewer who can walk away from the declared standpoint. */
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>
#include <emscripten.h>
#include <emscripten/eventloop.h>
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
/* The cadence an interactive session's log leaves the client on. Five seconds is a fifth of a
 * request per frame at 60 Hz and it bounds how much evidence one closed tab can take with it. */
constexpr double kSinkEveryMs = 5000.0;
/* [SET] What the collector may still owe when a run has nothing left to do. The same bound the
 * products get (SceneRunner.cpp), because they travel the same wire. */
constexpr double kDrainWaitMs = 20000.0;

Clients::Mod gMod;
const Clients::Scene *gScene = nullptr;
std::unique_ptr<Clients::Outshine> gApp;
std::unique_ptr<Clients::ServerLog> gLog;
std::unique_ptr<Clients::ServerTelemetry> gTelemetry;
std::unique_ptr<Clients::ServerArtifacts> gArtifacts;
std::unique_ptr<Clients::RunIdentity> gIdentity;
std::string gSimUrl;
Clients::Walker gWalker;
std::unique_ptr<Clients::SceneRunner> gRunner;
/* Negative while the run is still going; the run's exit code once it is not. */
int gRc = -1;
double gDrainFromMs = 0.0;

double gPrevMs = 0.0, gLastLogMs = 0.0, gLastSinkMs = 0.0, gLastCpuMs = 0.0, gLastEncodeMs = 0.0;
bool gLocked = false;          /* pointer lock; without it the mouse does not turn the head */
/* L is deferred to the END of the frame it was pressed in, because the picture the canvas holds is
 * the one this frame just drew and the browser hands it back only while that task is still running. */
bool gShotPending = false;
unsigned gShotNo = 0;

Clients::Outshine::Stance DeclaredStance(void) {
  return {gScene->Lat(), gScene->Lon(), gScene->YawDeg(), gScene->PitchDeg()};
}

bool OnKey(int type, const EmscriptenKeyboardEvent *e, void *) {
  Log::Debug("walk", "key", {{"code", std::string(e->code)}, {"key", std::string(e->key)},
      {"down", type == EMSCRIPTEN_EVENT_KEYDOWN}, {"repeat", (bool)e->repeat}});
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

/* THE REQUEST HAS AN ANSWER AND IT IS NOT ALWAYS YES: deferred, unsupported, refused outside a user
 * gesture, unknown target. Dropping it left "the mouse does not turn the head" with no reason
 * anywhere, which is the same silence a registration nobody checks leaves behind. */
bool OnClick(int, const EmscriptenMouseEvent *, void *) {
  if (gLocked) return true;
  const EMSCRIPTEN_RESULT r = emscripten_request_pointerlock(kCanvas, true);
  if (r != EMSCRIPTEN_RESULT_SUCCESS)
    Log::Error("walk", "pointerlock_refused", {{"result", (int)r}, {"target", std::string(kCanvas)}});
  return true;
}

/* THE REFUSAL ARRIVES AFTER THE REQUEST SUCCEEDED. `emscripten_request_pointerlock` answers whether
 * the CALL was well formed; whether the engine grants the lock is a separate DOM event, and without
 * this one a browser that declines leaves "the mouse does not turn the head" with no reason on
 * record anywhere. */
bool OnLockError(int, const void *, void *) {
  Log::Error("walk", "pointerlock_denied", {{"target", std::string(kCanvas)}});
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
  snap.SetCamera(gApp->Simulation().Lat(), gApp->Simulation().Lon(), gApp->Simulation().YawDeg(), gApp->Simulation().PitchDeg());
  const Clients::Outshine::Counters c = gApp->Measured();
  snap.SetDerived(c.GroundAslM, c.AltAslM, gApp->Simulation().SunElDeg(), gApp->Simulation().SunAzDeg());
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

  Log::Info("walk", "shot", {{"name", std::string(name)}, {"lat", gApp->Simulation().Lat()},
      {"lon", gApp->Simulation().Lon()}, {"yawDeg", gApp->Simulation().YawDeg()}, {"pitchDeg", gApp->Simulation().PitchDeg()},
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
        {"worldMs", c.WorldMs}, {"meshMs", c.MeshMs},
        {"uploadMs", c.UploadMs}, {"buildingMs", c.BuildingMs},
        {"bDecodeMs", c.BuildingDecodeMs}, {"encodeMs", gLastEncodeMs},
        {"built", (double)c.Built}, {"draws", c.Draws}});
  gLastCpuMs = emscripten_get_now() - now;
  gLastEncodeMs = encMs;

  if (gShotPending) {
    gShotPending = false;
    PostShot();
  }
  /* AN INTERACTIVE SESSION HAS NO END, so a batch that only goes out at a byte threshold or at the
   * end of a run never goes out at all: 28 browser walks left telemetry on the host and not one log
   * line, which is why nothing about them could be diagnosed. The post is asynchronous, so the frame
   * pays for the copy and not for the wire. */
  if (now - gLastSinkMs > kSinkEveryMs) {
    gLastSinkMs = now;
    gLog->Flush();
  }
  if (now - gLastLogMs > kLogEveryMs) {
    gLastLogMs = now;
    Log::Info("walk", "frame", {{"draws", c.Draws}, {"triangles", (double)c.Triangles},
        {"buildingVerts", (int)c.BuildingVerts}, {"treeTris", (double)c.TreeTriangles},
        {"treeStands", (double)c.TreeStands},
        {"progress", (double)p.Fraction}, {"resident", p.Resident},
        {"lat", gApp->Simulation().Lat()}, {"lon", gApp->Simulation().Lon()}, {"yawDeg", gApp->Simulation().YawDeg()},
        {"pitchDeg", gApp->Simulation().PitchDeg()}, {"groundM", c.GroundAslM}, {"locked", gLocked}});
  }
}

/* A REGISTRATION THAT CANNOT FAIL LOUDLY is the defect class this tree keeps paying for: the call
 * hands back a code, nobody reads it, and the picture comes up with no input and no reason. */
EMSCRIPTEN_RESULT Bound(const char *event, EMSCRIPTEN_RESULT r) {
  if (r != EMSCRIPTEN_RESULT_SUCCESS)
    Log::Error("walk", "input_unbound", {{"event", std::string(event)}, {"result", (int)r}});
  return r;
}

void BindInput(void) {
  /* Keys on the WINDOW, pointer on the CANVAS: WASD must work before the first click, while a turn
   * may only follow a pointer the canvas actually owns. */
  const EMSCRIPTEN_RESULT down =
      Bound("keydown", emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr,
                                                       true, OnKey));
  const EMSCRIPTEN_RESULT up =
      Bound("keyup", emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true,
                                                   OnKey));
  const EMSCRIPTEN_RESULT click =
      Bound("click", emscripten_set_click_callback(kCanvas, nullptr, false, OnClick));
  const EMSCRIPTEN_RESULT move =
      Bound("mousemove", emscripten_set_mousemove_callback(kCanvas, nullptr, false, OnMouseMove));
  const EMSCRIPTEN_RESULT lock =
      Bound("pointerlockchange",
            emscripten_set_pointerlockchange_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr,
                                                      false, OnLockChange));
  const EMSCRIPTEN_RESULT denied =
      Bound("pointerlockerror",
            emscripten_set_pointerlockerror_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr,
                                                     false, OnLockError));
  Log::Info("walk", "input_bound", {{"canvas", std::string(kCanvas)}, {"keydown", (int)down},
      {"keyup", (int)up}, {"click", (int)click}, {"mousemove", (int)move}, {"lock", (int)lock},
      {"lockError", (int)denied}});
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

/* A TURN IS A TASK HERE, and a task is what lets a fetch and a GPU map complete: the browser's one
 * thread runs the run's next turn and then hands the thread back.
 *
 * `set_immediate` AND NOT A TIMEOUT, and this is measured: a nested setTimeout(0) is clamped to
 * about 4 ms, and a profiled frame that has to poll the queue twice paid that clamp twice —
 * `demo/crossing` read p50 33.2 ms against 19.8 for the same drawn frames. The immediate posts a
 * message instead, so the cost of handing the thread back is the host's own and not a declared
 * minimum. The display's clock is the wrong one here for the opposite reason: it would put a whole
 * frame period between two streaming passes. */
void Turn(void *);

void NextTurn(void) { emscripten_set_immediate(Turn, nullptr); }

/* THE DECLARED RUN, IN THE BROWSER. Same SceneRunner, same order, same numbers to compare against —
 * only the destination of the products differs, and that is the whole point of Artifacts.h. */
void Finished(int rc) {
  Log::Info("run", "finished", {{"rc", rc}, {"runId", gLog->RunId()}});
  gRc = rc;
  gDrainFromMs = emscripten_get_now();
}

/* THE RUN IS NOT OVER UNTIL ITS EVIDENCE HAS LANDED. Log and telemetry go out over the same fetch
 * the products do, so the page may only declare a result once the collector has taken all three —
 * a harness that closed the tab on FB_RUN_DONE would otherwise throw the last batch away. */
void Drain(void) {
  const bool sunk = gTelemetry->Flush() && gLog->Flush();
  if (!sunk && emscripten_get_now() - gDrainFromMs < kDrainWaitMs) {
    NextTurn();
    return;
  }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
  EM_ASM({ window.FB_RUN_DONE = $0; }, gRc);
#pragma clang diagnostic pop
}

void Turn(void *) {
  if (gRc >= 0) { Drain(); return; }
  if (gRunner) {
    if (gRunner->Step() == Clients::SceneRunner::Progress::Running) { NextTurn(); return; }
    Finished(gRunner->Result());
    gRunner.reset();
    NextTurn();
    return;
  }
  if (gApp->Busy()) { gApp->Step(); NextTurn(); return; }
  switch (gApp->Stage()) {
    case Clients::Outshine::Phase::Prepared: gApp->Open(); NextTurn(); return;
    case Clients::Outshine::Phase::Playing: break;
    default: Finished(1); NextTurn(); return;
  }
  /* THE INTERACTIVE SCENE, brought up. The display's clock takes the frame over from here; what
   * streams in afterwards arrives beside it. */
  gWalker.Reset(DeclaredStance());
  BindInput();
  emscripten_set_main_loop(Frame, 0, 0);
}

}  // namespace

int main(void) {
  static Clients::StdoutLogSink boot;
  Log::SetSink(&boot);
  Log::SetLevel(LogLevel::Debug);
  if (!Boot()) return 1;
  /* SCENE FIRST, THEN PLAY. A run scene is a SceneRunner's whole business; an interactive one is
   * brought up by the same turns and then hands the frame to the display. */
  if (gScene->What() == Clients::Scene::Kind::Run)
    gRunner = std::make_unique<Clients::SceneRunner>(*gApp, *gScene, *gArtifacts);
  NextTurn();
  return 0;
}
