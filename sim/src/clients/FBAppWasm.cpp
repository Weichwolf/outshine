/* The browser client: a live in-process F-16 flying the WebGPU engine, camera on the aircraft's eye,
 * FBWorld streaming z14 fb-tiles around it. */
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <memory>
#include <string>
#include <emscripten.h>
#include <emscripten/html5.h>
#include "FBRenderer.h"
#include "FBMapCamera.h"
#include "FBTacticalMap.h"
#include "FBForcePicture.h"
#include "FBTacticalOrder.h"
#include "FBMissionSim.h"
#include "FBOrdnance.h"
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
#include "FBSimTick.h"
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
static const char *kDefaultMissionName = "payerne-full";   /* the full autonomous sortie: ground start,
    waypoint loop, landing to a full stop. web/missions/ is a build-time copy of sim/missions/
    (make wasm) served by fb-sim's web/ mount — editable without a WASM rebuild */
static char gMissionUrl[192] = "/missions/payerne-full.fbm";

/* WHICH file this session flies — the player layer's only reach into the client (window.FB_MISSION,
 * read exactly like FB_TILES_URL / FB_ORIGIN_LAT above it). It selects a mission and changes nothing
 * about one: same parser, same spawn, same judges. A name is a FILENAME and never a path, so anything
 * outside [A-Za-z0-9._-] falls back to the default rather than composing a URL from a JS string. */
static void ResolveMissionUrl() {
  const char *js = emscripten_run_script_string(
      "(window.FB_MISSION||new URLSearchParams(location.search).get('mission')||'').toString()");
  char name[128];
  snprintf(name, sizeof name, "%s", js ? js : "");
  for (const char *p = name; *p; p++)
    if (!(isalnum((unsigned char)*p) || *p == '.' || *p == '_' || *p == '-')) { name[0] = 0; break; }
  snprintf(gMissionUrl, sizeof gMissionUrl, "/missions/%s.fbm",
           name[0] ? name : kDefaultMissionName);
}

static Render::FBRenderer R;
static World::FBWorld W;
/* The mission's whole cast, one entry per `unit` block. gOwnship is the FIRST actor and only that: the
 * browser has ONE eye and ONE HUD, so camera and HUD ride actors[0] while the rest are stepped
 * alongside. */
static Units::FBActorList gActors;
static Units::FBUnitRegistry gUnits;
static Units::FBSimUnit *gOwnship = nullptr;
/* WHAT LEAVES THE JET AND WHAT IT DOES — the identical object fb-gym drives (missions/FBOrdnance.h).
 * Without it the browser could press the pickle and the round would sit in the SMS queue forever. */
static Missions::FBOrdnance gOrdnance{kWasmModelRoots};
/* THE SIMULATION, and the browser does not step it — it asks it to run until it stops or until this
 * frame owes the display a picture (missions/FBMissionSim.h). The end rule, the judges and the tick's
 * phase order live in there, which is why this client cannot fly a wreck: there is no way to advance
 * without being told whether the run is over. */
static std::unique_ptr<Missions::FBMissionSim> gSim;
static bool gRunOver = false;

/* ---- THE SECOND VIEW, and it is an INFORMATION change and not a camera move. The cockpit draws ONE
 * unit's own published blocks; the map draws the FACTION's fused picture at the control node of its
 * net. Selecting a unit here and pressing TAB means sitting in ITS cockpit, which sees LESS than the
 * map — if it ever sees more, this build is wrong (doc/player-layer.md §9.7). */
enum class FBView { Cockpit, Map };
static FBView gView = FBView::Cockpit;
static FBForcePicture gPic;
static Render::FBTacticalMap gMap;
static Systems::FBHudGeometry gMapGeo;
/* ZOOM AND SCROLL ARE VIEW STATE. They live here, in the client, in the object that owns them; there
 * is no path from FBMapCamera into a tick, and the sim cannot tell where the commander is looking. */
static Render::FBMapCamera gMapCam{Render::FBMapSheetStage::kTs};
static int gSelected = 0;              /* index into gActors of the unit the commander is talking to */
static uint32_t gOrderSeq = 0;
/* NOT -90: FBCameraBasisEcef builds its right vector from fwd x world-up, and straight down makes those
 * parallel — the basis collapses and the frame comes back empty. A tenth of a degree costs H*tan(0.1) =
 * 21 m of centre offset at 12 km, i.e. under a pixel. */
static constexpr double kMapPitchDeg = -89.9;

/* GROUND TRUTH FOR THE SIMULATION, from the same DEM the renderer draws, so gear/contact/crash collide
 * against real terrain. A cold /elev keeps the last good value for BOTH the FDM and the HUD/radar-alt
 * path (units/FBSimUnit::UpdateGroundAsl) — they must not disagree about where the ground is. The wall
 * time spent in here is the [cpuprof] `groundMs` field: this IS the browser's ground cost, measured
 * where it is paid. */
class FBStreamElevation : public FBElevationProvider {
public:
  double GroundElevM(double latDeg, double lonDeg) const override {
    double t0 = emscripten_get_now();
    double g = fb_stream_ground(latDeg, lonDeg);
    MsAcc_ += emscripten_get_now() - t0;
    return g;
  }
  double TakeMs() { double ms = MsAcc_; MsAcc_ = 0.0; return ms; }

private:
  mutable double MsAcc_ = 0.0;
};
static FBStreamElevation gElevation;

/* WHERE THE EYE WAS AT THE LAST TWO TICKS — filled by the simulation's own tick hook, which fires after
 * the pose barrier. The camera is carried between them; the sim never reads any of it. */
class FBBrowserEye : public Missions::FBMissionTickHook {
public:
  void OnTick(const Units::FBActorList &actors, double simT) override;
};
static FBBrowserEye gEye;

/* WHERE THE EYE IS BETWEEN TWO TICKS, and nothing else: the sim never sees this pose. The simulation
 * keeps the tick clock and its own accumulator (FBMissionSim::TickPhase); what is left here is the
 * CAMERA's business and this frame's profile counters. */
static Units::FBUnitPose gPosePrev, gPoseCur;
static bool gHavePose = false;
static double gTickSubsteps = 0.0;
static int gTicks = 0;
static constexpr double kMaxFrameS = 0.25;   /* a stall/tab-switch clamp, so the sim does not lurch */
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

/* ---- THE HUMAN IN THE SEAT ---------------------------------------------------------------------
 * The keyboard is bound HERE and reaches exactly one object: the ownship's FBInputSystem slot. It
 * writes no FDM property, no autopilot setter and no weapon system — the analogue half becomes Manual
 * guidance through the module's own ApplyPilotCommands, the discrete half becomes an FBCommandBus post
 * with the AI's latency, occupancy and rejection catalogue. No new extern "C" symbol either: the
 * callbacks below are registered with emscripten's own HTML5 event API, so the browser calls a static
 * C++ function by POINTER and nothing has to be exported by name. doc/player-layer.md §10.2. */
static struct FBHeldKeys {
  bool NoseUp = false, NoseDown = false, RollL = false, RollR = false;
  bool YawL = false, YawR = false, ThrUp = false, ThrDn = false;
  bool Brake = false, Trigger = false;
} gKeys;

static Systems::FBInputSystem *Hands() {
  return gOwnship ? gOwnship->Module().HumanInput() : nullptr;
}

/* A refusal the BUS would never see, because the hands are not on the jet at all — said out loud in the
 * same channel every other refusal uses, so the player learns the rule rather than pressing into a void. */
static void NoStick(const char *action) {
  FBLog::Warn("hotas", "NO_STICK", {{"action", action}, {"hint", "press P to take the stick"}});
}

/* THE NODE WHOSE PICTURE THE MAP IS. The unit says whose control net it is on (its mission's own `net`
 * block, read back); if nobody names one, the map is the ownship's own picture — the smaller claim,
 * never the bigger one. */
static Units::FBSimUnit *MapNode() {
  if (!gOwnship) return nullptr;
  const char *node = gOwnship->Module().NetControlNode();
  if (node && node[0]) {
    for (const auto &u : gActors)
      if (u && u->GetName() == node) return u.get();
  }
  return gOwnship;
}

/* ONE COMMANDER'S WORD, to the selected unit's AI. It writes no state and reaches no system directly:
 * pilot/FBPilot decides, its own boxes may refuse, and both halves are in events.log. */
static void PostOrder(FBOrderKind kind, double latDeg, double lonDeg, double altM, double value) {
  if (gSelected < 0 || gSelected >= (int)gActors.size() || !gActors[gSelected]) return;
  Units::FBSimUnit &u = *gActors[gSelected];
  FBTacticalOrder o;
  o.Seq = ++gOrderSeq;
  o.Kind = kind;
  o.LatDeg = latDeg; o.LonDeg = lonDeg; o.AltM = altM;
  o.Value = value;
  o.IssuedS = gSim ? gSim->SimTimeS() : 0.0;
  FBLogUnitScope us(u.GetName());
  u.Module().PilotSystem().ReceiveOrder(o);
  char st[96];
  snprintf(st, sizeof st, "ORDER %s -> %s (SEQ %u)", FBOrderKindStr(kind), u.GetName().c_str(),
           (unsigned)o.Seq);
  gMap.SetStatus(st);
}

/* The map's nearest UNKNOWN datum to the selected unit — what a commander points at when he says
 * "engage". It is the map's own symbol and therefore the faction's own measurement; there is no path
 * from here to where anything actually is. */
static bool NearestUnknown(double &latDeg, double &lonDeg, double &altM) {
  const FBForceSymbol *best = nullptr;
  for (int i = 0; i < gPic.Count(); i++) {
    const FBForceSymbol &sy = gPic.At(i);
    if (sy.Aff != FBAffiliation::Unknown || !sy.HavePoint) continue;
    if (!best || sy.AgeS < best->AgeS) best = &sy;
  }
  if (!best) return false;
  latDeg = best->LatDeg; lonDeg = best->LonDeg; altM = best->AltM;
  return true;
}

static void SelectNext(int step) {
  if (gActors.empty()) return;
  int n = (int)gActors.size();
  for (int k = 1; k <= n; k++) {
    int i = ((gSelected + step * k) % n + n) % n;
    if (!gActors[i] || gActors[i]->GetTeam() != gOwnship->GetTeam()) continue;
    gSelected = i;
    gMap.SetSelected(gActors[i]->GetName().c_str());
    return;
  }
}

/* THE MAP SHEET'S BYTES, off the tile path that already exists: mode 0 is /bake/osm, the same page
 * the terrain streamer uploads as albedo, so the map's pages ride the worker pool's own cache. */
static int MapTileFetch(int z, int x, int y, int ts, unsigned char *dst) {
  return fb_stream_pyramid(z, (uint32_t)x, (uint32_t)y, 0, ts, dst);
}

/* THE MAP'S OWN KEYS. They exist only in the map view, so nothing here can be pressed by accident from
 * the seat — and every one of them ends in an order, a selection or the view scale. */
static bool HandleMapKey(const char *k, bool down, bool repeat) {
  auto is = [k](const char *t) { return std::strcmp(k, t) == 0; };
  /* THE NAVIGATION KEYS REPEAT — an order does not. A held arrow has to scroll, so these are tested
   * ahead of the once-per-press guard the rest of the map's keys sit behind. */
  if (down) {
    /* ZOOM: one OSM level a step, so the sheet stays at one texel per pixel and never resamples. */
    if (is("]") || is("+") || is("=") || is("PageUp")) { gMapCam.ZoomIn(); return true; }
    if (is("[") || is("-") || is("PageDown")) { gMapCam.ZoomOut(); return true; }
    /* SCROLL: a fifth of the frame a press. Home puts the map back on the node. */
    if (is("ArrowLeft")) { gMapCam.PanPixels(-(double)R.SceneW() * 0.2, 0.0); return true; }
    if (is("ArrowRight")) { gMapCam.PanPixels((double)R.SceneW() * 0.2, 0.0); return true; }
    if (is("ArrowUp")) { gMapCam.PanPixels(0.0, -(double)R.SceneH() * 0.2); return true; }
    if (is("ArrowDown")) { gMapCam.PanPixels(0.0, (double)R.SceneH() * 0.2); return true; }
    if (is("Home")) { gMapCam.Recentre(); return true; }
  }
  if (!down || repeat) return false;
  if (is("n") || is("N")) { SelectNext(1); return true; }
  if (is("b") || is("B")) { SelectNext(-1); return true; }
  if (is("e") || is("E")) { PostOrder(FBOrderKind::Emcon, 0, 0, 0, 1.0); return true; }
  if (is("r") || is("R")) { PostOrder(FBOrderKind::Emcon, 0, 0, 0, 0.0); return true; }
  if (is("h") || is("H")) {
    PostOrder(FBOrderKind::WeaponsControl, 0, 0, 0, (double)(int)FBWeaponsControl::Hold);
    return true;
  }
  if (is("g") || is("G")) {
    PostOrder(FBOrderKind::WeaponsControl, 0, 0, 0, (double)(int)FBWeaponsControl::Free);
    return true;
  }
  if (is("x") || is("X")) { PostOrder(FBOrderKind::Abort, 0, 0, 0, 0); return true; }
  if (is("a") || is("A")) {
    double la = 0, lo = 0, al = 0;
    if (NearestUnknown(la, lo, al)) PostOrder(FBOrderKind::Attack, la, lo, al, 0);
    else gMap.SetStatus("NO CONTACT ON THE MAP TO POINT AT");
    return true;
  }
  return false;
}

static bool HandleKey(const char *k, bool down, bool repeat) {
  /* TAB is the VIEW, and it is bound here rather than in the page because it changes what the player
   * knows and not what the page shows. The ground-albedo switch it used to throw moved to `t`. */
  if (std::strcmp(k, "Tab") == 0) {
    if (!down || repeat) return true;
    gView = gView == FBView::Cockpit ? FBView::Map : FBView::Cockpit;
    if (gView == FBView::Map && gOwnship) {
      for (int i = 0; i < (int)gActors.size(); i++)
        if (gActors[i].get() == gOwnship) gSelected = i;
      gMap.SetSelected(gOwnship->GetName().c_str());
    } else if (gView == FBView::Cockpit && gSelected >= 0 && gSelected < (int)gActors.size()) {
      /* SELECT A UNIT AND PRESS TAB = SIT IN ITS COCKPIT. It is an information change: from here on the
       * picture is THAT unit's own blocks and it sees less than the map did. */
      gOwnship = gActors[gSelected].get();
      R.SetHudDisplay(&gOwnship->Displays());
    }
    FBLog::Info("view", "MODE", {{"view", gView == FBView::Map ? "map" : "cockpit"},
                                 {"unit", gOwnship ? gOwnship->GetName() : std::string()}});
    return true;
  }
  if (gView == FBView::Map) return HandleMapKey(k, down, repeat);
  if (std::strcmp(k, "t") == 0 || std::strcmp(k, "T") == 0) {
    if (down && !repeat) GroundSet(!R.GetGroundMode());
    return true;
  }
  Systems::FBInputSystem *in = Hands();
  if (!in) return false;
  auto is = [k](const char *s) { return std::strcmp(k, s) == 0; };
  auto press = [&](Systems::FBHotasAction a, double v, const char *name) {
    if (!down || repeat) return true;   /* a switch is a FLANK; auto-repeat is the keyboard's, not a hand's */
    if (!in->Engaged()) { NoStick(name); return true; }
    if (!in->Press(a, v)) FBLog::Warn("hotas", "QUEUE_FULL", {{"action", name}});
    return true;
  };

  if (is("ArrowUp"))    { gKeys.NoseDown = down; return true; }   /* stick forward = nose down */
  if (is("ArrowDown"))  { gKeys.NoseUp = down; return true; }
  if (is("ArrowLeft"))  { gKeys.RollL = down; return true; }
  if (is("ArrowRight")) { gKeys.RollR = down; return true; }
  if (is(",") || is("<")) { gKeys.YawL = down; return true; }
  if (is(".") || is(">")) { gKeys.YawR = down; return true; }
  if (is("w") || is("W")) { gKeys.ThrUp = down; return true; }
  if (is("s") || is("S")) { gKeys.ThrDn = down; return true; }
  if (is("b") || is("B")) { gKeys.Brake = down; return true; }
  if (is(" ")) {
    gKeys.Trigger = down;
    if (down && !repeat && !in->Engaged()) NoStick("gun_trigger");
    return true;
  }
  if (is("Enter")) return press(Systems::FBHotasAction::WeaponRelease, 0.0, "weapon_release");
  if (is("m") || is("M")) {
    /* The switch has two positions and the jet knows which one it is in; the key is the THROW. */
    bool armed = gOwnship && gOwnship->Module().Telemetry().Stores.Arm == FBArmState::Arm;
    return press(Systems::FBHotasAction::MasterArm, armed ? 0.0 : 1.0, "master_arm");
  }
  if (k[0] >= '1' && k[0] <= '9' && k[1] == 0)
    return press(Systems::FBHotasAction::StationSelect, (double)(k[0] - '0'), "station_select");
  if (is("g") || is("G")) {
    if (!down || repeat) return true;
    if (!in->Engaged()) { NoStick("gear"); return true; }
    in->SetGearDown(!in->Stick().GearDown);
    return true;
  }
  if (is("p") || is("P")) {
    if (!down || repeat) return true;
    if (in->Engaged()) { in->ReleaseStick(); FBLog::Info("hotas", "STICK", {{"state", "released"}}); }
    else { in->TakeStick(); FBLog::Info("hotas", "STICK", {{"state", "taken"}}); }
    return true;
  }
  return false;
}

static EM_BOOL OnKey(int type, const EmscriptenKeyboardEvent *e, void *) {
  if (e->ctrlKey || e->metaKey || e->altKey) return EM_FALSE;
  return HandleKey(e->key, type == EMSCRIPTEN_EVENT_KEYDOWN, e->repeat != 0) ? EM_TRUE : EM_FALSE;
}

/* THE MOUSE, and it belongs to the MAP alone: the cockpit view lets every event through untouched.
 * A drag is measured in CSS pixels and the map is drawn in frame pixels, so the delta is scaled by
 * the canvas' own ratio — otherwise the ground slides at the wrong rate on any display size. */
static bool gMapDrag = false;
static double gDragX = 0.0, gDragY = 0.0;

static double MapPxPerCss(void) {
  double cssW = 0.0, cssH = 0.0;
  if (emscripten_get_element_css_size("#gpu", &cssW, &cssH) != EMSCRIPTEN_RESULT_SUCCESS || cssW < 1.0)
    return 1.0;
  return (double)R.SceneW() / cssW;
}

static EM_BOOL OnMapMouse(int type, const EmscriptenMouseEvent *e, void *) {
  if (gView != FBView::Map) { gMapDrag = false; return EM_FALSE; }
  if (type == EMSCRIPTEN_EVENT_MOUSEDOWN) {
    gMapDrag = true;
    gDragX = e->targetX;
    gDragY = e->targetY;
    return EM_TRUE;
  }
  if (type == EMSCRIPTEN_EVENT_MOUSEUP) { gMapDrag = false; return EM_TRUE; }
  if (!gMapDrag) return EM_FALSE;
  double s = MapPxPerCss();
  gMapCam.PanPixels((gDragX - e->targetX) * s, (gDragY - e->targetY) * s);
  gDragX = e->targetX;
  gDragY = e->targetY;
  return EM_TRUE;
}

static EM_BOOL OnMapWheel(int, const EmscriptenWheelEvent *e, void *) {
  if (gView != FBView::Map) return EM_FALSE;
  if (e->deltaY < 0.0) gMapCam.ZoomIn();
  else if (e->deltaY > 0.0) gMapCam.ZoomOut();
  return EM_TRUE;
}

/* THE ONE THING THIS CLIENT DOES PER TICK, and it is a READ: the eye's last two published poses and
 * this tick's substep count. FBMissionSim calls it after the pose barrier, so both poses are whole. */
void FBBrowserEye::OnTick(const Units::FBActorList &actors, double simT) {
  (void)simT;
  const Units::FBUnitPose now = actors.front()->GetPose();
  gPosePrev = gHavePose ? gPoseCur : now;
  gPoseCur = now;
  gHavePose = true;
  gTickSubsteps += gOwnship->Module().LastSubsteps();
  gTicks++;
}

/* WHERE THE EYE IS BETWEEN TWO TICKS, and nothing else: the sim never sees this pose. Extrapolated
 * rather than interpolated because interpolation would hold the camera a full tick behind the HUD,
 * which is drawn from the newest published state — this way both sit on the same instant at alpha 0
 * and drift apart by at most one tick's attitude change. */
static Units::FBUnitPose EyeAt(double alpha) {
  if (!gHavePose) return gOwnship->GetPose();   /* before the first tick there is nothing to carry */
  const Units::FBUnitPose &a = gPosePrev, &b = gPoseCur;
  Units::FBUnitPose p = b;
  p.LatDeg = b.LatDeg + alpha * (b.LatDeg - a.LatDeg);
  p.LonDeg = b.LonDeg + alpha * FBWrap180(b.LonDeg - a.LonDeg);
  p.ElevM = b.ElevM + alpha * (b.ElevM - a.ElevM);
  p.RollDeg = b.RollDeg + alpha * FBWrap180(b.RollDeg - a.RollDeg);
  p.PitchDeg = b.PitchDeg + alpha * FBWrap180(b.PitchDeg - a.PitchDeg);
  p.YawDeg = b.YawDeg + alpha * FBWrap180(b.YawDeg - a.YawDeg);
  return p;
}

static void frame(void) {
  double now = emscripten_get_now();
  double dt = LastMs > 0.0 ? (now - LastMs) / 1000.0 : 0.0;
  LastMs = now;
  if (dt > kMaxFrameS) dt = kMaxFrameS;   /* clamp a stall/tab-switch so the sim doesn't lurch */

  /* THE ATOMIC SWITCH, and the only place it can happen: before this frame reads the wind at all. */
  if (gWxIncoming) {
    gWeather = std::move(gWxIncoming);
    W.SetWeather(gWeather.get());   /* the drawing side borrows the same object, never a copy */
    gSim->SetWeather(*gWeather);    /* ...and the flying side, at the same instant */
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

  /* THE CLOCK SPLIT, and this round's whole point: the SIM advances only in whole kSimTickS steps —
   * the same tick fb-gym steps, so the same file gives the same run — while the CAMERA keeps the frame
   * rate below. THIS CLIENT DOES NOT WRITE THAT LOOP: it hands the simulation the wall time it owes a
   * picture for and is TOLD whether the run is still going (missions/FBMissionSim.h). Which is the
   * whole repair — the browser used to step a mission itself and had no way to end one, so a CFIT'd
   * F-16 kept being integrated. doc/clients/clients.md §5.5/§5.6. */
  gTickSubsteps = 0.0;
  gTicks = 0;
  /* THE HANDS, before anything is stepped: keys held into this frame become an axis INTENT, and the
   * slot's own ramp turns it into a deflection. Nothing else of the keyboard survives past this line. */
  if (Systems::FBInputSystem *in = Hands()) {
    in->SetAxisIntent((gKeys.RollR ? 1 : 0) - (gKeys.RollL ? 1 : 0),
                      (gKeys.NoseUp ? 1 : 0) - (gKeys.NoseDown ? 1 : 0),
                      (gKeys.YawR ? 1 : 0) - (gKeys.YawL ? 1 : 0));
    in->SetThrottleIntent((gKeys.ThrUp ? 1 : 0) - (gKeys.ThrDn ? 1 : 0));
    in->SetSpeedbrake(gKeys.Brake ? 1.0 : 0.0);
    in->SetTrigger(gKeys.Trigger);
  }
  if (!gRunOver && gSim->Advance(dt) == Missions::FBRunState::Concluded) {
    /* THE RUN IS OVER, and from here the browser only DRAWS it. The verdict is stated in the same line
     * the headless runner emits, because it is the same verdict off the same two judges — the player
     * layer already listens for it and opens the debriefing (web/fbmenu.js, doc/player-layer.md §5). */
    gRunOver = true;
    const Units::FBSimUnit *deciding = gSim->Deciding();
    const Fdm::fb_fdm_state &est = gOwnship->State();
    FBLog::SetTime(gSim->SimTimeS());
    FBLogUnitScope us(deciding ? deciding->LogLabel() : std::string());
    FBLog::Info("mission", "RESULT",
        {{"result", Missions::FBMissionResultStr(gSim->Result())}, {"reason", gSim->Reason()},
         {"lat", est.lat}, {"lon", est.lon}, {"altM", est.elev}, {"durationS", gSim->SimTimeS()}});
  }
  const double simT = gSim->SimTimeS();
  double cp_b = cp_a + gElevation.TakeMs();   /* the ground cost, measured where it is paid */
  double gForHud = gOwnship->GroundAslM();

  const Fdm::fb_fdm_state &St = gOwnship->State();
  Systems::FBGuidance g = gOwnship->Module().LastGuidance();
  double nSub = gTickSubsteps;
  double cp_c = emscripten_get_now();   /* end: the tick loop */

  R.SetAgl((float)gOwnship->AglM());   /* the unit's own AGL, so FBRadarAltimeter and the HUD agree */

  /* ---- THE MAP VIEW, and it returns before a single cockpit field is computed: the two views read
   * different things, and computing both would be the shared scene graph doc/player-layer.md §9.7
   * forbids. Same render pass, other strokes -- the per-frame Begin*Pass count is unchanged. */
  if (gView == FBView::Map) {
    Units::FBSimUnit *node = MapNode();
    if (node) {
      Units::FBUnitPose np = node->GetPose();
      gPic.Begin(simT, node->GetTeam());
      gPic.Ingest(node->HudState(), node->GetName().c_str(), np.LatDeg, np.LonDeg, np.ElevM,
                  np.HeadingDeg, np.SpeedMs);
      R.SetHud(node->HudState(), true);   /* before the view: the pass being on is what shifts the scene */
      /* The boresight is at ViewH/2, not the frame centre — the scene is shifted up by the bank. */
      gMapCam.SetFrame(R.SceneW(), R.SceneH(), 0.0f, (float)R.ViewH() * 0.5f);
      gMapCam.Track(np.LatDeg, np.LonDeg);
      Render::FBMapView mv = gMapCam.View();
      R.SetMapSheet(mv, true);
      R.PumpMapSheet();
      /* The scene pass still runs under the sheet; the camera is set so the sky LUTs see a real
       * place. The 3D terrain is NOT streamed in this view — the sheet is the ground now. */
      double meye[3], mfwd[3], mright[3], mup[3];
      FBGeoToEcef(mv.CentreLatDeg, mv.CentreLonDeg, 12000.0, meye);
      FBCameraBasisEcef(0.0, kMapPitchDeg, 0.0, mv.CentreLatDeg, mv.CentreLonDeg, mfwd, mright, mup);
      R.SetCameraBasis(meye, mfwd, mright, mup);
      gMapGeo.Reset();
      gMap.SetGroundNote(Render::FBMapSheetNote(R.MapSheetState()));
      gMap.Build(gPic, mv, gMapGeo);
      R.SetMapOverlay(&gMapGeo);
      R.RenderFrame();
      R.SetMapSheet(mv, false);
    }
    return;
  }
  R.SetMapOverlay(nullptr);

  /* Camera = the aircraft eye, carried across the sub-tick gap so the picture keeps the frame rate
   * while the sim keeps its own. Nothing downstream of here is read by the simulation. */
  Units::FBUnitPose p = EyeAt(gSim->TickPhase());
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
  { static double lastLogS = 0.0;
    if (simT - lastLogS >= 1.0) { lastLogS = simT;
      FBLog::Info("flight", "agl", {{"alt", St.elev}, {"agl", gOwnship->AglM()}, {"ground", gForHud},
          {"fdmGnd", gOwnship->Fdm() ? gOwnship->Fdm()->GetGroundElevM() : 0.0}, {"spd", St.speed}, {"cas", St.cas}, {"bank", St.roll},
          {"hdg", St.yaw}, {"vs", St.vy}, {"ringDist", g.RingDistM},
          {"mode", ModeLabel(g.Mode)}});
      FBLog::Info("flight", "home", {{"dist", (double)hs.Platform.HomeDistM}, {"brg", (double)hs.Platform.HomeBearingDeg},
          {"hdg", St.yaw}, {"lon", St.lon}});
      FBLog::Info("pilot", "phase", {{"phase", Pilot::FBPilot::PhaseName(gOwnship->Module().PilotSystem().GetPhase())}});
      /* THE ARMAMENT PANEL, and every field is a PUBLISHED BLOCK the seat's own boxes wrote — the same
       * numbers the HUD reads, on the same bus, so the player's panel can show nothing his instruments
       * do not. Beside the flight line and at the same rate, for the same reason. */
      Systems::FBInputSystem *in = Hands();
      FBLog::Info("hotas", "armament",
          {{"stick", in && in->Engaged() ? "human" : "ai"}, {"arm", hs.Stores.Arm == FBArmState::Arm},
           {"station", hs.Stores.SelectedStation}, {"loaded", hs.Stores.LoadedCount},
           {"released", hs.Stores.ReleasedCount}, {"rounds", hs.Gun.RoundsRemaining},
           {"gunReady", hs.Gun.Ready}, {"hits", gOwnship->Health().Hits()},
           {"effective", gOwnship->Health().CombatEffective()}});
    } }
  /* Used only in photo mode; SVS pins a constant day. */
  double utc = SimClock.Have ? SimClock.At(simT) : (double)(SimUtc ? SimUtc : time(nullptr));
  FBSunPos(St.lat, St.lon, utc, &hs.Env.SunElDeg, &hs.Env.SunAzDeg);
  FBMoonPos(St.lat, St.lon, utc, &hs.Env.MoonElDeg, &hs.Env.MoonAzDeg, &hs.Env.MoonPhase);
  R.SetSkyClock(utc);
  /* Same seam as the native oracle: the CLIENT samples the weather where the camera is and hands the
   * renderer the resulting decks — the renderer never sees a provider. core/FBCloudDensity.h. */
  if (gWeather) {
    const FBCloudSky sky = FBCloudSkyFromWeather(*gWeather, p.LatDeg, p.LonDeg, simT);
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
  W.SetSimTimeS(simT);   /* the MISSION clock the published effect ages are stamped against */
  W.Update(p.LatDeg, p.LonDeg, eye, fwd, now);   /* multi-LOD quadtree around the live flight */
  double cp_e = emscripten_get_now();   /* end: FBWorld update (quadtree + gain + lights poll) */

  R.RenderFrame();
  double cp_f = emscripten_get_now();   /* end: render (CPU-side record + submit) */

  { static double aGround = 0, aJsbsim = 0, aPose = 0, aWorld = 0, aRender = 0, aPeriod = 0, acc = 0;
    static long nF = 0, sTicks = 0;
    static double sSub = 0.0;
    aGround += cp_b - cp_a; aJsbsim += cp_c - cp_b; aPose += cp_d - cp_c;
    aWorld += cp_e - cp_d; aRender += cp_f - cp_e; aPeriod += dt * 1000.0;
    sSub += nSub; sTicks += gTicks; nF++; acc += dt;
    if (acc >= 1.0) {
      double loop = aGround + aJsbsim + aPose + aWorld + aRender;
      double per = aPeriod / nF;   /* mean rAF period (ms) — the 1/refresh budget */
      FBLog::Debug("cpuprof", "summary", {{"loopMs", loop / nF}, {"pctOfRaf", per > 0 ? 100.0 * (loop / nF) / per : 0.0},
          {"rafMs", per}, {"groundMs", aGround / nF}, {"jsbsimMs", aJsbsim / nF}, {"poseMs", aPose / nF},
          {"worldMs", aWorld / nF}, {"renderMs", aRender / nF}, {"draws", R.DrawCount()},
          {"tilebufB", R.DrawCount() * 32}, {"substepsPerFrame", sSub / nF},
          /* THE SPLIT this loop exists for: frames follow the display, ticks follow kSimTickS. */
          {"simTicksPerS", acc > 0 ? (double)sTicks / acc : 0.0}, {"frames", (int)nF}});
      aGround = aJsbsim = aPose = aWorld = aRender = aPeriod = acc = 0; nF = 0; sSub = 0.0; sTicks = 0;
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
  ResolveMissionUrl();

  Modules::FBRegisterBuiltinModules();

  /* Still air is what the session STARTS in, whatever it ends up flying: the fetch below is in flight
   * while the first frames already run, and a null provider would be a branch in the tick path. */
  gWeather = std::make_unique<FBCalmWeather>();

  /* WHAT THE SIMULATION NEEDS TO KNOW ABOUT THIS RUN, read off the mission below — the sandbox declares
   * neither. An `?ap=manual` sandbox has no plan and therefore no clock that could end it; only its
   * physical judge can, which is exactly what an infinite timeout says. */
  double timeoutS = std::numeric_limits<double>::infinity();
  bool needRanges = false;

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
    /* fb_fetch_text TRUNCATES at the cap, and a mission cut at a line boundary parses into a SMALLER
     * CAST — a run against a file nobody wrote. The buffer therefore holds the largest mission in the
     * tree with headroom (26,490 B, o1-10-mole-cricket.fbm) and a full buffer is refused rather than
     * flown, because a fit and a truncation are indistinguishable from the length alone. */
    static char missionText[65536];
    int n = fb_fetch_text(gMissionUrl, missionText, sizeof missionText);
    FlightBox::FBMission mission;
    std::string perr;
    if (n <= 0 || n >= (int)sizeof missionText - 1 || !FBParseMissionFile(missionText, mission, &perr)) {
      FBLog::Error("gpu", "mission_boot_failed", {{"url", gMissionUrl},
          {"reason", n <= 0 ? std::string("fetch (is fb-sim serving web/missions/?)")
                   : n >= (int)sizeof missionText - 1 ? std::string("mission file exceeds the client buffer")
                   : perr}});
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
        FBLog::Error("gpu", "mission_boot_failed", {{"url", gMissionUrl}, {"reason", serr}});
        return 1;
      }
      FBLog::Info("gpu", "mission_boot", {{"name", mission.Name}, {"hdg", sp.HeadingDeg},
          {"lat", sp.LatDeg}, {"lon", sp.LonDeg}, {"groundM", groundAsl},
          {"waypoints", mission.Units[i].Plan.Size()}});
      gActors.push_back(std::move(unit));
    }
    for (const auto &u : mission.Units)
      for (const auto &o : u.Objectives)
        needRanges = needRanges || o.Kind == FBObjectiveKind::Identify;
    timeoutS = mission.TimeoutS;
    /* The mission's CLOCK, resolved in the one place all three clients share. A file that declares
     * `time` while window.FB_SIM_UTC is set stops the boot: a sky nobody asked for is worse than no
     * picture (missions/FBClockBoot.h). */
    std::string clkErr;
    if (!Missions::FBResolveMissionClock(mission, SimUtc != 0, SimClock, &clkErr)) {
      FBLog::Error("gpu", "mission_boot_failed", {{"url", gMissionUrl}, {"reason", clkErr}});
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
      FBLog::Info("wx", "source", {{"source", "mission"}, {"url", gMissionUrl}});
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
  /* The exact ceiling once every loadout is applied — one further actor per loaded station and not one
   * more, the same count FBMissionRunner computes. Everything index-parallel is sized for it here, so
   * no buffer grows while a frame is holding a reference into it. */
  size_t maxActors = gActors.size();
  for (auto &a : gActors) maxActors += (size_t)a->Module().MaxReleases();
  gActors.reserve(maxActors);
  gOrdnance.Reserve(maxActors);
  /* THE SIMULATION, built once every actor exists and the cast's ceiling is fixed. From here this
   * client owns no tick and no end rule — it owns a camera, a HUD and a keyboard. */
  gSim = std::make_unique<Missions::FBMissionSim>(gActors, gUnits, gOrdnance, gElevation, *gWeather,
                                                 timeoutS);
  gSim->SetClock(SimClock);
  gSim->SetRangeAware(needRanges);
  gSim->SetHook(&gEye);
  gSim->SetWorld(&W);   /* the terrain side, for the module slots entitled to it */
  gSim->Prime();        /* one step each, so the first FRAME reads a filled state */

  /* The keyboard, on the WINDOW so a canvas that never took focus still flies. Returning EM_TRUE from
   * the handler is what keeps the arrow keys from scrolling the page under the cockpit. */
  emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, 1, OnKey);
  emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, 1, OnKey);
  /* The map's wheel and drag, on the CANVAS: outside the map view every one of these returns EM_FALSE
   * and the page keeps the event it always had. */
  emscripten_set_wheel_callback("#gpu", nullptr, 1, OnMapWheel);
  emscripten_set_mousedown_callback("#gpu", nullptr, 1, OnMapMouse);
  emscripten_set_mousemove_callback("#gpu", nullptr, 1, OnMapMouse);
  emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, 1, OnMapMouse);

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
  R.SetMapSheetSource(&MapTileFetch);
  /* The airframe meshes, preloaded into emscripten's FS by the wasm target. A miss leaves the units
   * invisible and never blocks startup — exactly as a missing moon texture does. */
  if (!R.AddUnitModel("f16", "/fb/models"))
    FBLog::Warn("gpu", "unit_model_missing", {{"type", "f16"}, {"dir", "/fb/models"}});
  R.Init("#gpu", 1280, 720);
  if (!W.Open(&R, base, olat, olon, 32, viewM, 512)) {
    FBLog::Error("gpu", "world_open_failed", {{"base", std::string(base)}});
    return 1;
  }
  for (auto &a : gActors) gUnits.Register(a.get());   /* the App owns the units; everyone else borrows */
  W.SetUnits(&gUnits);
  W.SetEyeUnitId(gOwnship->GetId());   /* the camera rides this one: drawing it would draw its inside */
  W.SetWeather(gWeather.get());   /* the DATA side only; the frame swap below re-points it when /wx lands */
  FBLog::Info("gpu", "world_ready", {{"viewKm", viewM / 1000.0}});

  emscripten_set_main_loop(frame, 0, 1);
  return 0;
}
