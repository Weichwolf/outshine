/* Native headless WebGPU harness (Dawn, not WASM/emdawnwebgpu): drives FBRenderer's OFFSCREEN mode
 * through the same HDR+ACES-tonemap pipeline FBAppWasm.cpp exercises in-browser, dumping PNG frames.
 * This is the verification path a headless-browser SwiftShader can't give us: native Dawn actually
 * renders. Terrain streams from FBWorld's multi-LOD quadtree (Stage 7) — the SAME code FBAppWasm.cpp
 * runs in-browser, fetched via libcurl (fb_terrain.c's native branch) since there is no browser event
 * loop. Camera sits at ~1500 m AGL looking to the horizon so the far LOD gradient is visible.
 * stb_image_write (public domain, geo/osmmesh/src/3rdparty/stb_image_write.h) writes the PNGs. */
#include "FBRenderer.h"
#include "FBWorld.h"
#include "FBEphemeris.h"
#include "FBModule.h"
#include "FBF16Module.h"
#include "FBTerrainField.h"
#include "FBPathPlan.h"
#include "FBTerrainLoader.h"
#include "FBTelemetry.h"
#include "jsbsim_adapter.h"
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>
#include <vector>
#include <sys/stat.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

static const double kPi = 3.14159265358979323846;
static void GeoToEcef(double latDeg, double lonDeg, double alt, double out[3]) {
  const double a = 6378137.0, e2 = 6.69437999014e-3;
  double lat = latDeg * kPi / 180.0, lon = lonDeg * kPi / 180.0;
  double sl = std::sin(lat), cl = std::cos(lat);
  double N = a / std::sqrt(1.0 - e2 * sl * sl);
  out[0] = (N + alt) * cl * std::cos(lon);
  out[1] = (N + alt) * cl * std::sin(lon);
  out[2] = (N * (1.0 - e2) + alt) * sl;
}

/* Aircraft attitude -> ECEF camera basis (mirrors FBAppWasm.cpp cameraBasis: render-space yaw/pitch/
 * roll, then rotated into ECEF at lat/lon). Kept in sync so the oracle frames the flight exactly as
 * the browser does. */
static void EnuAxes(double latDeg, double lonDeg, double E[3], double N[3], double U[3]) {
  double P = latDeg * kPi / 180.0, L = lonDeg * kPi / 180.0;
  double sP = std::sin(P), cP = std::cos(P), sL = std::sin(L), cL = std::cos(L);
  E[0] = -sL;      E[1] = cL;       E[2] = 0.0;
  N[0] = -sP * cL; N[1] = -sP * sL; N[2] = cP;
  U[0] = cP * cL;  U[1] = cP * sL;  U[2] = sP;
}
static void CrossN(const double a[3], const double b[3], double o[3]) {
  o[0] = a[1] * b[2] - a[2] * b[1]; o[1] = a[2] * b[0] - a[0] * b[2]; o[2] = a[0] * b[1] - a[1] * b[0];
}
static void CameraBasis(double yawDeg, double pitchDeg, double rollDeg, double latDeg, double lonDeg,
                        double fwd[3], double right[3], double up[3]) {
  double yaw = yawDeg * kPi / 180.0, pitch = pitchDeg * kPi / 180.0, roll = rollDeg * kPi / 180.0;
  double fR[3] = {std::cos(pitch) * std::sin(yaw), std::sin(pitch), -std::cos(pitch) * std::cos(yaw)};
  double wup[3] = {0, 1, 0}, s[3], u[3];
  CrossN(fR, wup, s);
  { double l = std::sqrt(s[0]*s[0]+s[1]*s[1]+s[2]*s[2]); if (l < 1e-12) l = 1.0; s[0]/=l; s[1]/=l; s[2]/=l; }
  CrossN(s, fR, u);
  double upR[3], srR[3];
  for (int i = 0; i < 3; i++) {
    upR[i] = u[i] * std::cos(roll) + s[i] * std::sin(roll);
    srR[i] = s[i] * std::cos(roll) - u[i] * std::sin(roll);
  }
  double E[3], N[3], U[3];
  EnuAxes(latDeg, lonDeg, E, N, U);
  auto toEcef = [&](const double rv[3], double out[3]) {
    double e = rv[0], uu = rv[1], nn = -rv[2];
    for (int i = 0; i < 3; i++) out[i] = E[i] * e + N[i] * nn + U[i] * uu;
  };
  toEcef(fR, fwd); toEcef(srR, right); toEcef(upR, up);
}

namespace {

bool EnsureDir(const std::string &dir) {
  std::string cur;
  for (size_t i = 0; i <= dir.size(); i++) {
    if (i == dir.size() || dir[i] == '/') {
      if (!cur.empty() && mkdir(cur.c_str(), 0755) != 0 && errno != EEXIST) return false;
      if (i < dir.size()) cur += '/';
    } else {
      cur += dir[i];
    }
  }
  return true;
}

void Usage(const char *argv0) {
  fprintf(stderr,
          "usage: %s [--lat D] [--lon D] [--ground M] [--agl M] [--view KM] [--yaw DEG] [--pitch DEG]\n"
          "          [--albedo osm|photo] [--utc SECS] [--cloud C] [--cloudq Q] [--moonscale S] [--moon PATH]\n"
          "          [--base URL] [--seconds N] [--interval M] [--out DIR]\n"
          "  --cloudlab [--labx PARAM] [--laby PARAM]  render an N x M cloud-parameter grid to ONE PNG.\n"
          "    PARAM in {coverage,density,extinct,suni,detail}; fixed cloud-bank camera, no terrain.\n"
          "  --fly   in-process F-16 loiter (JSBSim+FLCS+autopilot) at --lat/--lon; camera = the eye,\n"
          "    live HUD + real DEM ground floor; PNGs every --interval s + 1 Hz [agl] telemetry.\n",
          argv0);
}

/* Sweep range for a lab parameter. */
static void LabRange(const std::string &p, float &lo, float &hi) {
  if (p == "coverage") { lo = 0.30f; hi = 0.82f; }
  else if (p == "density") { lo = 2.0f; hi = 10.0f; }
  else if (p == "extinct") { lo = 0.03f; hi = 0.16f; }
  else if (p == "suni") { lo = 6.0f; hi = 40.0f; }
  else if (p == "detail") { lo = 0.4f; hi = 2.6f; }
  else { lo = 0.0f; hi = 1.0f; }
}

/* Cloud lab: render a cols x rows grid of parameter variants into ONE PNG. Fixed camera on a cloud
 * bank, no terrain streaming (fast); the two swept axes are --labx (columns) / --laby (rows). */
int RunCloudLab(double lat, double lon, time_t utc, double cloudQ, double ground, double aglM,
                const std::string &labx, const std::string &laby, const std::string &moonPath,
                const std::string &outDir, bool singleCell = false, float cov0 = 0.6f,
                float den0 = 5.0f, float det0 = 1.3f, double pitchOverrideDeg = -999.0,
                double camBelowM = -5000.0, double bankKm = 12.0, int frames = 24) {   /* default: view the deck from ABOVE (AC7 vantage) */
  const int W = 1280, H = 720;
  const double kPi2 = 3.14159265358979;
  /* FRONTAL framing: place the camera below the deck base and aim at a bank ~12 km ahead at the deck's
   * mid-height, so the silhouette/underside structure reads face-on instead of grazing the deck edge-on
   * at the horizon. Deck geometry mirrors the shader default (base 1500 AGL, top ~4100 AGL). aglM is
   * ignored here — the lab camera is deck-relative so the framing is deterministic across invocations. */
  const double deckBaseAGL = 1500.0, deckTopAGL = 4100.0, deckMidAGL = 0.5 * (deckBaseAGL + deckTopAGL);
  const double camAGL = deckBaseAGL - camBelowM, bankDistM = bankKm * 1000.0;
  (void)aglM;
  double altMSL = ground + camAGL;
  double eye[3];
  GeoToEcef(lat, lon, altMSL, eye);
  FlightBox::FBState hs{};
  hs.alt = (float)altMSL; hs.gs = 220.f; hs.airspeed = 220.f; hs.state = FlightBox::FBMode::Loiter;
  FlightBox::SunPos(lat, lon, utc, &hs.sun_el, &hs.sun_az);
  FlightBox::MoonPos(lat, lon, utc, &hs.moon_el, &hs.moon_az, &hs.moon_phase);
  double P = lat * kPi2 / 180.0, L = lon * kPi2 / 180.0;
  double sP = std::sin(P), cP = std::cos(P), sL = std::sin(L), cL = std::cos(L);
  double E3[3] = {-sL, cL, 0}, N3[3] = {-sP * cL, -sP * sL, cP}, U3[3] = {cP * cL, cP * sL, sP};
  /* Aim 42 deg OFF the sun azimuth so the deck is side-lit (edge light + self-shadow visible), not
   * blinded by the sun disc. Pitch follows from the deck-relative aim point, not a fixed angle. */
  double yawDeg = hs.sun_az + 42.0, yaw = yawDeg * kPi2 / 180.0;
  double riseM = (pitchOverrideDeg > -900.0) ? bankDistM * std::tan(pitchOverrideDeg * kPi2 / 180.0)
                                             : deckMidAGL - camAGL;
  hs.yaw = (float)yawDeg; hs.pitch = (float)(std::atan2(riseM, bankDistM) * 180.0 / kPi2);
  double hdir[3], target[3];
  for (int a = 0; a < 3; a++) hdir[a] = N3[a] * std::cos(yaw) + E3[a] * std::sin(yaw);
  for (int a = 0; a < 3; a++) target[a] = eye[a] + hdir[a] * bankDistM + U3[a] * riseM;

  FlightBox::FBRenderer R;
  R.SetStreaming(512);
  R.SetDefaultMode(1);
  R.SetGroundMode(1);      /* EVS -> the cloud pass runs */
  R.SetSkyClock((double)utc);
  R.SetCloudQuality(cloudQ);
  { uint8_t *moon = 0; int mw = 0, mh = 0;
    if (fb_load_image_file(moonPath.c_str(), &moon, &mw, &mh)) { R.SetMoonTexture(moon, mw, mh); free(moon); } }
  R.SetCamera(eye, target);
  R.SetHud(hs, true);
  R.SetHudEnabled(false);   /* no HUD clutter in the lab cells */
  R.InitOffscreen(W, H);
  if (!R.Ready()) { fprintf(stderr, "cloudlab: device init failed\n"); return 1; }

  if (getenv("FB_SHAPEHIST")) {   /* numeric base-shape histogram, then exit (numbers, not eyes) */
    R.SetCloudLab(cov0, den0, 0.06f, 18.0f, det0);
    R.RenderFrame();   /* ensure clouds/noise are created */
    R.ShapeStats(cov0, 0.0f, 0.0f);
    return 0;
  }
  if (singleCell) {   /* one FULL-RES cell — no 4x downscale hiding the shard geometry */
    R.SetCloudLab(cov0, den0, 0.06f, 18.0f, det0);
    if (getenv("FB_CLOUD_ACCUM")) R.SetAccumMode(true);   /* TAA proof: true 1/N average over the jitter phases */
    R.ResetCloudHistory();
    for (int f = 0; f < frames; f++) R.RenderFrame();
    std::vector<uint8_t> img;
    if (!R.ReadPixels(img)) { fprintf(stderr, "cloudcell: readback failed\n"); return 1; }
    char cpath[512];
    snprintf(cpath, sizeof cpath, "%s/cloudcell_cov%.2f_den%.1f_det%.1f_p%.1f.png",
             outDir.c_str(), cov0, den0, det0, (double)hs.pitch);
    stbi_write_png(cpath, W, H, 4, img.data(), W * 4);
    printf("[cloudcell] wrote %s (%dx%d) cov=%.2f den=%.1f det=%.1f pitch=%.1f camBelow=%.0f bank=%.0fkm sun_el=%.1f\n",
           cpath, W, H, cov0, den0, det0, (double)hs.pitch, camBelowM, bankKm, hs.sun_el);
    return 0;
  }
  const int cols = 4, rows = 3, cw = W / 4, ch = H / 4, gw = cols * cw, gh = rows * ch;
  std::vector<uint8_t> grid((size_t)gw * gh * 4, 25);
  float lox, hix, loy, hiy;
  LabRange(labx, lox, hix);
  LabRange(laby, loy, hiy);
  printf("[cloudlab] grid %dx%d cells, columns=%s [%.3f..%.3f], rows=%s [%.3f..%.3f], sun el=%.1f\n",
         cols, rows, labx.c_str(), lox, hix, laby.c_str(), loy, hiy, hs.sun_el);
  for (int r = 0; r < rows; r++)
    for (int c = 0; c < cols; c++) {
      float cov = 0.55f, den = 5.0f, ext = 0.06f, sun = 18.0f, det = 1.3f;
      float vx = lox + (hix - lox) * (float)c / (float)(cols - 1);
      float vy = loy + (hiy - loy) * (float)r / (float)(rows - 1);
      auto apply = [&](const std::string &p, float v) {
        if (p == "coverage") cov = v; else if (p == "density") den = v; else if (p == "extinct") ext = v;
        else if (p == "suni") sun = v; else if (p == "detail") det = v;
      };
      apply(labx, vx); apply(laby, vy);
      R.SetCloudLab(cov, den, ext, sun, det);
      R.ResetCloudHistory();                 /* fresh accumulation per cell */
      for (int f = 0; f < 24; f++) R.RenderFrame();   /* let the temporal history converge */
      std::vector<uint8_t> img;
      if (!R.ReadPixels(img)) { fprintf(stderr, "cloudlab: readback failed\n"); return 1; }
      for (int y = 0; y < ch; y++)
        for (int x = 0; x < cw; x++) {   /* 4x4 box downscale W*H -> cw*ch */
          int sr = 0, sg = 0, sb = 0;
          for (int dy = 0; dy < 4; dy++)
            for (int dx = 0; dx < 4; dx++) {
              const uint8_t *s = &img[(((size_t)(y * 4 + dy) * W) + x * 4 + dx) * 4];
              sr += s[0]; sg += s[1]; sb += s[2];
            }
          uint8_t *d = &grid[(((size_t)(r * ch + y) * gw) + c * cw + x) * 4];
          d[0] = (uint8_t)(sr / 16); d[1] = (uint8_t)(sg / 16); d[2] = (uint8_t)(sb / 16); d[3] = 255;
        }
      printf("[cloudlab] cell col=%d row=%d  %s=%.3f  %s=%.3f\n", c, r, labx.c_str(), vx, laby.c_str(), vy);
    }
  char path[512];
  snprintf(path, sizeof path, "%s/cloudlab_%s_x_%s.png", outDir.c_str(), labx.c_str(), laby.c_str());
  stbi_write_png(path, gw, gh, 4, grid.data(), gw * 4);
  printf("[cloudlab] wrote %s (%dx%d)\n", path, gw, gh);
  printf("[cloudlab] REF: cauliflower silhouette | flat-ish base | self-shadow (dark base, lit top) | "
         "sun-side edge light | NO straight edges >20px\n");
  return 0;
}

/* --fly: the REAL in-process loiter (JSBSim + FBFlightControl F16 FLCS + FBAutopilot LOITER) drives
 * the oracle exactly as FBAppWasm.cpp drives the browser — camera is the aircraft eye, HUD reads the
 * live state, terrain streams around the live position, and the FDM ground floor tracks the real DEM
 * (fb_stream_ground -> fb_jsbsim_set_ground). Deterministic fixed timestep (Prinzip 4). Renders PNGs
 * every `interval` s and emits the 1 Hz [agl] telemetry from the sim tick. */
int RunFly(double lat, double lon, double ground0, double aglM, double viewKm, time_t utc,
           int groundPhoto, double moonScale, const std::string &moonPath, const std::string &base,
           double seconds, double interval, const std::string &outDir) {
  const int width = 1280, height = 720, fps = 15;   /* render cadence; sim substeps stay 100 Hz, so
                                                       lowering it only cuts native render count, not
                                                       flight fidelity (Prinzip 4: wall-clock-free) */
  const double kRadiusM = 8000.0, kSpeedMs = 220.0;

  FlightBox::FBRenderer R;
  R.SetDefaultMode(groundPhoto);
  R.SetGroundMode(groundPhoto);
  R.SetStreaming(512);
  R.SetMoonScale(moonScale);
  time_t clk = utc ? utc : time(nullptr);
  R.SetSkyClock((double)clk);
  {   /* real-sky assets (EVS): NASA moon albedo + HYG stars — optional, degrade gracefully */
    uint8_t *moon = 0; int mw = 0, mh = 0;
    if (fb_load_image_file(moonPath.c_str(), &moon, &mw, &mh)) { R.SetMoonTexture(moon, mw, mh); free(moon); }
    static uint8_t stars[262144];
    int sn = fb_fetch_stars(base.c_str(), stars, (int)sizeof stars);
    if (sn > 0) R.SetStars(stars, sn, lat, lon);
  }

  /* Open the streamer FIRST: fb_stream_ground needs fb_base set (fb_stream_open -> fbs_init). */
  FlightBox::FBWorld W;
  if (!W.Open(&R, base.c_str(), lat, lon, 32, viewKm * 1000.0, 512)) {
    fprintf(stderr, "gpu_native --fly: FBWorld open FAILED — is fb-tiles reachable at %s ?\n", base.c_str());
    return 1;
  }
  W.SetDefaultMode(groundPhoto);
  W.SetGroundMode(groundPhoto);
  { float sel = 0, saz = 0; FlightBox::SunPos(lat, lon, clk, &sel, &saz);
    W.SetNightLights(groundPhoto && sel < -3.0f); }

  /* LOWLEVEL autopilot (FB_AP_MODE=lowlevel): terrain-following AGL-hold + look-ahead. Stage 1 flies a
   * fixed heading (default south, toward the Alps) low over the terrain. Env knobs override the defaults. */
  /* LOWLEVEL is the DEFAULT (matches the browser); loiter-based recipes (gate/[home] checks) set
   * FB_AP_MODE=loiter explicitly. */
  const char *apMode = getenv("FB_AP_MODE");
  bool lowlevel = !(apMode && std::strcmp(apMode, "loiter") == 0);
  double llAgl    = getenv("FB_LL_AGL")     ? atof(getenv("FB_LL_AGL"))     : 150.0;
  double llSpeed  = getenv("FB_LL_SPEED")   ? atof(getenv("FB_LL_SPEED"))   : 230.0;
  double llHdg    = getenv("FB_LL_HEADING") ? atof(getenv("FB_LL_HEADING")) : 180.0;
  int    llDemZ   = getenv("FB_LL_DEM_Z")   ? atoi(getenv("FB_LL_DEM_Z"))   : 13;   /* match /elev (FB_DEM_Z=13) so the look-ahead sees the same peaks the AGL metric does */

  /* Seed the FDM ground at the origin BEFORE init so the trim/IC floor is the real terrain, not 0. */
  double gSeed = fb_stream_ground(lat, lon);
  double ground = gSeed > -1e8 ? gSeed : ground0;
  double spawnAgl = lowlevel ? llAgl : aglM;
  double altAsl = ground + spawnAgl;
  /* Loiter spawns 8 km due N heading E; LOWLEVEL spawns AT the origin already on the commanded heading. */
  double slat = lowlevel ? lat : lat + kRadiusM / 111320.0, slon = lon;
  double spawnHdg = lowlevel ? llHdg : 90.0;
  double spawnSpd = lowlevel ? llSpeed : kSpeedMs;
  if (fb_jsbsim_init("vendor/jsbsim/aircraft", "f16", slat, slon, altAsl, 0.0, spawnSpd, spawnHdg, 0) != 0) {
    fprintf(stderr, "gpu_native --fly: JSBSim init FAILED\n");
    return 1;
  }
  if (gSeed > -1e8) fb_jsbsim_set_ground(gSeed);

  /* Polymorphic handle: activeModule is what Run() is called through below (the real dispatch
   * mechanism, not a shortcut around it); F16 is the concrete handle this scope needs for
   * F-16-specific setup (Autopilot/FlightControl gains, PathPlan) outside the generic FBModule
   * contract. */
  auto F16 = std::make_unique<FlightBox::FBF16Module>();
  FlightBox::FBModule *activeModule = F16.get();
  FlightBox::FBAutopilot &AP = F16->Autopilot();
  R.SetHudDisplay(&F16->Displays());   /* HUD symbology: the module's Displays slot (default HUD) */
  FlightBox::FBTerrainField terrainField(llDemZ);
  /* Planner terrain field is COARSER (z9) — a 500 km A* only needs valley/pass structure, and coarse
   * keeps the tile count tiny. Separate instance from the z12 vertical look-ahead field. */
  int llPlanZ  = getenv("FB_LL_PLAN_Z")  ? atoi(getenv("FB_LL_PLAN_Z"))  : 9;
  double llRadKm = getenv("FB_LL_RADIUS_KM") ? atof(getenv("FB_LL_RADIUS_KM")) : 500.0;
  unsigned llSeed = getenv("FB_LL_SEED") ? (unsigned)atol(getenv("FB_LL_SEED")) : 1u;
  FlightBox::FBTerrainField planField(llPlanZ);
  F16->ConfigurePathPlan(&planField, lat, lon, llRadKm * 1000.0, llSeed);   /* the module OWNS the planner */
  FlightBox::FBPathPlan &plan = *F16->PathPlan();
  bool llPlanned = lowlevel && getenv("FB_LL_PLANNER") != nullptr;   /* A* far-planner: opt-in (fan is default) */
  bool llFixHdg  = lowlevel && getenv("FB_LL_FIXHDG") != nullptr;    /* fixed heading, fan off (Stage-1 recipe) */
  const bool noRender = getenv("FB_NORENDER") != nullptr;   /* flight/planner oracle without the flaky render */
  if (lowlevel) {
    AP.SetTerrain(&terrainField, fb_stream_ground);
    AP.SetLowLevel(llAgl, llSpeed, llHdg);           /* enables the reactive fan by default */
    AP.SetFence(lat, lon, llRadKm * 1000.0);
    if (const char *e = getenv("FB_FAN_N"))     AP.FanN = atoi(e);
    if (const char *e = getenv("FB_FAN_ARC"))   AP.FanArcDeg = atof(e);
    if (const char *e = getenv("FB_FAN_RANGE")) AP.FanRangeM = atof(e);
    if (const char *e = getenv("FB_FAN_BIAS"))  AP.FanStraightBias = atof(e);
    if (const char *e = getenv("FB_FAN_EASE"))  AP.FanEase = atof(e);
    if (const char *e = getenv("FB_FAN_TURNDB")) AP.FanTurnDeadbandDeg = atof(e);
    if (llPlanned) {
      if (getenv("FB_LL_GOAL_LAT") && getenv("FB_LL_GOAL_LON"))
        plan.SetFixedGoal(atof(getenv("FB_LL_GOAL_LAT")), atof(getenv("FB_LL_GOAL_LON")));
      plan.Update(slat, slon);   /* initial plan at the spawn */
      printf("gpu_native --fly: LOWLEVEL+PLAN agl=%.0f m, %.0f m/s, DEM z=%d, plan z=%d, fence %.0f km, seed %u | goal %.4f/%.4f route %d wp (expanded %ld)\n",
             llAgl, llSpeed, terrainField.Zoom(), planField.Zoom(), llRadKm, llSeed, plan.GoalLat(), plan.GoalLon(), plan.RouteSize(), plan.LastExpanded());
      for (auto &wp : plan.Waypoints()) printf("[route] %.5f %.5f\n", wp.first, wp.second);
      fflush(stdout);
    } else if (llFixHdg) {
      AP.SetLowLevelHeading(llHdg);   /* fan off */
      printf("gpu_native --fly: LOWLEVEL agl=%.0f m, %.0f m/s, FIXED hdg=%.0f, DEM z=%d\n", llAgl, llSpeed, llHdg, terrainField.Zoom());
    } else {
      printf("gpu_native --fly: LOWLEVEL+FAN agl=%.0f m, %.0f m/s, DEM z=%d, fence %.0f km (reactive terrain fan, wings-level)\n",
             llAgl, llSpeed, terrainField.Zoom(), llRadKm);
    }
  } else {
    AP.SetLoiter(lat, lon, altAsl, kRadiusM, 1, kSpeedMs);
  }
  fb_fdm_state St;
  fb_jsbsim_step(&St);
  printf("gpu_native --fly: F-16 loiter %.4f/%.4f alt %.0f m ASL (ground %.0f), R %.0f m, %.0f m/s\n",
         lat, lon, altAsl, ground, kRadiusM, kSpeedMs);

  /* HUD nav placeholder: one steerpoint 8 nm bearing 060 from the origin, bullseye AT the origin — a
   * concrete, moving relative bearing so the guide's "diamond in FOV" and "crossed-out" cases both
   * occur across a run (loiter's own turn, or LOWLEVEL's fan steering, sweep the relative bearing). */
  {
    const double kStptBrgDeg = 60.0, kStptRangeNm = 8.0;
    double brgRad = kStptBrgDeg * kPi / 180.0, rangeM = kStptRangeNm * 1852.0;
    double stptLat = lat + (rangeM * std::cos(brgRad)) / 111320.0;
    double stptLon = lon + (rangeM * std::sin(brgRad)) / (111320.0 * std::cos(lat * kPi / 180.0));
    F16->Nav().SetSteerpoint(stptLat, stptLon, ground / 0.3048 + 50.0);
    F16->Nav().SetBullseye(lat, lon);
  }

  R.InitOffscreen(width, height);
  if (!R.Ready()) { fprintf(stderr, "gpu_native --fly: WebGPU device init failed\n"); return 1; }

  /* BOOT LOADING PHASE: stream the target cut at the SPAWN camera and show the loading screen while
   * JSBSim stays FROZEN (no step above the one init tick, no [agl]). Only when the visible target cut
   * is resident does the scene turn on and the sim begin — so the first scene frame is full-resolution
   * and the spawn DEM ground is guaranteed loaded. */
  {
    double leye[3], lfwd[3], lright[3], lup[3];
    GeoToEcef(St.lat, St.lon, St.elev, leye);
    CameraBasis(St.yaw, St.pitch, St.roll, St.lat, St.lon, lfwd, lright, lup);
    R.SetCameraBasis(leye, lfwd, lright, lup);
    const char *te = getenv("FB_LOAD_THRESH"); float thresh = te ? (float)atof(te) : 0.95f;
    const char *toe = getenv("FB_LOAD_TIMEOUT"); int tmax = toe ? atoi(toe) : 1500;
    int lshot = 0;
    for (int lf = 0; lf < tmax; lf++) {
      if (noRender && lf >= 1) break;   /* oracle: no render -> no need to wait for tile streaming */
      W.Update(St.lat, St.lon, leye, lfwd, (double)lf * 1000.0 / fps);
      float pct = W.LoadProgress();
      R.SetLoadingScreen(true, pct, W.TargetReadyN(), W.TargetTotal());
      if (!noRender) R.RenderFrame();
      if (lf % 20 == 0) { printf("[loading] %.0f%% (%d/%d tiles)\n", pct * 100.0f, W.TargetReadyN(), W.TargetTotal()); fflush(stdout); }
      if (!noRender && interval > 0.0 && (lf == 6 || lf == 30)) {   /* boot-sequence proof: a couple of loading frames */
        std::vector<uint8_t> rgba;
        if (R.ReadPixels(rgba)) { char p[512]; snprintf(p, sizeof p, "%s/loading_%04d.png", outDir.c_str(), lshot++);
          if (stbi_write_png(p, width, height, 4, rgba.data(), width * 4)) printf("gpu_native --fly: wrote %s\n", p); }
      }
      if (W.TargetTotal() > 0 && pct >= thresh) { printf("[loading] converged %.0f%% -> scene on, sim start\n", pct * 100.0f); fflush(stdout); break; }
    }
    R.SetLoadingScreen(false, 1.0f, 0, 0);
  }

  const double dt = 1.0 / fps;
  const int totalFrames = (int)(seconds * fps + 0.5);
  const int everyFrames = interval > 0.0 ? (int)(interval * fps + 0.5) : 0;
  int shot = 0;
  double accLog = 0.0;
  FlightBox::FBGuidance g{};
  for (int f = 0; f < totalFrames; f++) {
    /* FB_TOGGLE_S: flip the DISPLAY ground mode (SVS<->EVS, the TAB switch) every N sim-seconds — the
     * SVS/EVS-strictness repro. The base stays the boot mode; the overlay is lazy, so the transition
     * exercises the mode-aware readiness gate. */
    if (const char *tg = getenv("FB_TOGGLE_S")) {
      double period = atof(tg);
      static double accTg = 0; accTg += dt;
      static bool once = getenv("FB_TOGGLE_ONCE") != nullptr; static bool did = false;
      if (period > 0 && accTg >= period && !(once && did)) { accTg = 0; did = true;
        int m = !R.GetGroundMode(); R.SetGroundMode(m); W.SetGroundMode(m);
        printf("[toggle] display -> %s\n", m ? "EVS/photo" : "SVS/osm"); fflush(stdout);
      }
    }
    /* FDM ground floor = the live DEM under the aircraft (crash contract), fed BEFORE stepping. */
    double gnd = fb_stream_ground(St.lat, St.lon);
    if (gnd > -1e8) { ground = gnd; fb_jsbsim_set_ground(gnd); }
    F16->SetGroundAsl((float)ground);   /* FBRadarAltimeter reuses this SAME sample, see its banner */

    /* LOWLEVEL lateral: DEFAULT = the reactive terrain fan (chooses the heading toward the lowest valley,
     * wings-level). FB_LL_PLANNER: the A* far-planner steers by pure-pursuit instead. FB_LL_FIXHDG: neither. */
    if (lowlevel && llPlanned) {
      plan.Update(St.lat, St.lon);
      AP.SetLowLevelHeading(plan.DesiredTrackDeg(St.lat, St.lon));
    } else if (lowlevel && !llFixHdg) {
      AP.UpdateLowLevelSteering(St, dt);   /* reactive fan, once per frame */
    }

    activeModule->Run(St, dt, &W);
    g = F16->LastGuidance();

    double eye[3], fwd[3], right[3], up[3];
    GeoToEcef(St.lat, St.lon, St.elev, eye);
    CameraBasis(St.yaw, St.pitch, St.roll, St.lat, St.lon, fwd, right, up);
    R.SetCameraBasis(eye, fwd, right, up);

    FlightBox::FBState hs = F16->Telemetry();   /* seed: FBAirDataSystem/FBRadarAltimeter/FBNavSystem/... */
    hs.roll = (float)St.roll; hs.pitch = (float)St.pitch; hs.yaw = (float)St.yaw;
    hs.alt = (float)St.elev; hs.gs = (float)St.gs; hs.airspeed = (float)St.speed; hs.vs = (float)St.vy;
    double coslat = std::cos(lat * kPi / 180.0);
    double dlon = St.lon - lon;   /* Wrap180: without it the antimeridian gives a ~360° delta -> HUD DIST 38,171,944 */
    while (dlon > 180.0) dlon -= 360.0;
    while (dlon < -180.0) dlon += 360.0;
    hs.y = (float)((St.lat - lat) * 111320.0);
    hs.x = (float)(dlon * 111320.0 * coslat);
    hs.home_dist = (float)std::sqrt((double)hs.x * hs.x + (double)hs.y * hs.y);
    double absBrg = std::atan2(-(double)hs.x, -(double)hs.y) * 180.0 / kPi, rel = absBrg - St.yaw;
    while (rel > 180) rel -= 360;
    while (rel < -180) rel += 360;
    hs.home_bearing = (float)rel;
    hs.state = AP.GetMode();
    FlightBox::SunPos(St.lat, St.lon, clk, &hs.sun_el, &hs.sun_az);
    FlightBox::MoonPos(St.lat, St.lon, clk, &hs.moon_el, &hs.moon_az, &hs.moon_phase);
    R.SetHud(hs, true);
    R.SetAgl((float)(St.elev - ground));

    W.Update(St.lat, St.lon, eye, fwd, (double)f * 1000.0 / fps);
    /* FB_NORENDER: autopilot/flight oracle without the (flaky, lavapipe-stalling) render — sim + planner
     * + telemetry run free, so a long terrain-following track completes deterministically. */
    if (!noRender) R.RenderFrame();

    accLog += dt;
    if (accLog >= 1.0) { accLog = 0.0; FlightBox::FBLogAgl(St, g.Mode, g.RingDistM, ground, fb_jsbsim_get_ground());
      printf("[home] dist=%.0f brg=%.0f hdg=%.0f lon=%.4f\n", hs.home_dist, hs.home_bearing, St.yaw, St.lon);
      if (lowlevel)
        printf("[lowlevel] agl=%.0f tgtAgl=%.0f gndHere=%.0f gndAhead=%.0f tgtVs=%.1f vs=%.1f alt=%.0f demZ=%d decodes=%ld\n",
               St.elev - AP.LlGroundHere(), AP.LlTargetAgl(), AP.LlGroundHere(), AP.LlGroundAhead(),
               AP.LlTargetVs(), St.vy, St.elev, terrainField.Zoom(), terrainField.Decodes());
      if (lowlevel && !llPlanned && !llFixHdg)
        printf("[fan] acPos=%.5f/%.5f hdg=%.0f tgtHdg=%.0f bank=%.1f cost min/mid/max=%.0f/%.0f/%.0f chosen=%d\n",
               St.lat, St.lon, St.yaw, AP.LlHeading(), St.roll, AP.FanMinCost(), AP.FanMidCost(), AP.FanMaxCost(), AP.FanChosen());
      if (lowlevel && llPlanned) {
        printf("[plan] acPos=%.5f/%.5f goal=%.4f/%.4f goalDist=%.0fkm wp=%d/%d desTrk=%.0f replans=%ld expanded=%ld planDecodes=%ld\n",
               St.lat, St.lon, plan.GoalLat(), plan.GoalLon(), plan.GoalDistM(St.lat, St.lon) / 1000.0, plan.ActiveWp(),
               plan.RouteSize(), plan.DesiredTrackDeg(St.lat, St.lon), plan.Replans(), plan.LastExpanded(), planField.Decodes());
        static long lastRep = -1;
        if (plan.Replans() != lastRep) { lastRep = plan.Replans();
          for (auto &wp : plan.Waypoints()) printf("[route] %.5f %.5f\n", wp.first, wp.second); }
      }
      fflush(stdout); }

    bool last = (f == totalFrames - 1);
    bool due = everyFrames > 0 && (f % everyFrames) == (everyFrames - 1);
    if (!due && !(everyFrames == 0 && last)) continue;
    if (noRender) continue;   /* oracle: telemetry only, no frame capture */
    std::vector<uint8_t> rgba;
    if (!R.ReadPixels(rgba)) continue;   /* device may be lost late in a long run — telemetry lives on */
    char path[512];
    snprintf(path, sizeof path, "%s/fly_%04d.png", outDir.c_str(), shot++);
    if (stbi_write_png(path, width, height, 4, rgba.data(), width * 4))
      printf("gpu_native --fly: wrote %s\n", path);
  }
  return 0;
}

}  // namespace

int main(int argc, char **argv) {
  double lat = 47.18, lon = 7.41, seconds = 3.0, interval = 1.0;
  double ground = 430.0, aglM = 1500.0, viewKm = 240.0, yawDeg = 0.0, pitchDeg = -3.0, cloudCover = 0.0, moonScale = 1.0, cloudQ = 1.0;
  int groundPhoto = 0, cloudLab = 0, cloudCell = 0, fly = 0;
  float cellCov = 0.6f, cellDen = 5.0f, cellDet = 1.3f;
  double cellPitch = -999.0, cellBelow = -5000.0, cellBank = 12.0;   /* default: above the deck (AC7 vantage) */
  int cellFrames = 24;
  std::string labx = "coverage", laby = "density";
  time_t utc = 0;   /* 0 = real wall clock */
  std::string base = "http://localhost:8081", outDir = ".", moonPath = "flightbox/web/moon.jpg";
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--lat" && i + 1 < argc) lat = atof(argv[++i]);
    else if (a == "--lon" && i + 1 < argc) lon = atof(argv[++i]);
    else if (a == "--ground" && i + 1 < argc) ground = atof(argv[++i]);   /* terrain elevation, metres */
    else if (a == "--albedo" && i + 1 < argc) groundPhoto = (std::string(argv[++i]) == "photo");
    else if (a == "--utc" && i + 1 < argc) utc = (time_t)atof(argv[++i]);
    else if (a == "--cloud" && i + 1 < argc) cloudCover = atof(argv[++i]);
    else if (a == "--moon" && i + 1 < argc) moonPath = argv[++i];
    else if (a == "--moonscale" && i + 1 < argc) moonScale = atof(argv[++i]);
    else if (a == "--cloudq" && i + 1 < argc) cloudQ = atof(argv[++i]);
    else if (a == "--cloudlab") cloudLab = 1;
    else if (a == "--fly") fly = 1;
    else if (a == "--cell" && i + 3 < argc) { cloudCell = 1; cellCov = atof(argv[++i]); cellDen = atof(argv[++i]); cellDet = atof(argv[++i]); }
    else if (a == "--cellpitch" && i + 1 < argc) cellPitch = atof(argv[++i]);
    else if (a == "--cellbelow" && i + 1 < argc) cellBelow = atof(argv[++i]);
    else if (a == "--cellbank" && i + 1 < argc) cellBank = atof(argv[++i]);
    else if (a == "--cellframes" && i + 1 < argc) cellFrames = atoi(argv[++i]);
    else if (a == "--labx" && i + 1 < argc) labx = argv[++i];
    else if (a == "--laby" && i + 1 < argc) laby = argv[++i];
    else if (a == "--agl" && i + 1 < argc) aglM = atof(argv[++i]);
    else if (a == "--view" && i + 1 < argc) viewKm = atof(argv[++i]);
    else if (a == "--yaw" && i + 1 < argc) yawDeg = atof(argv[++i]);
    else if (a == "--pitch" && i + 1 < argc) pitchDeg = atof(argv[++i]);
    else if (a == "--base" && i + 1 < argc) base = argv[++i];
    else if (a == "--seconds" && i + 1 < argc) seconds = atof(argv[++i]);
    else if (a == "--interval" && i + 1 < argc) interval = atof(argv[++i]);
    else if (a == "--out" && i + 1 < argc) outDir = argv[++i];
    else { Usage(argv[0]); return 1; }
  }
  if (!EnsureDir(outDir)) { fprintf(stderr, "gpu_native: cannot create --out %s\n", outDir.c_str()); return 1; }

  if (fly)
    return RunFly(lat, lon, ground, aglM, viewKm, utc, groundPhoto, moonScale, moonPath, base,
                  seconds, interval, outDir);
  if (cloudLab)
    return RunCloudLab(lat, lon, utc ? utc : time(nullptr), cloudQ, ground, aglM, labx, laby, moonPath, outDir);
  if (cloudCell)
    return RunCloudLab(lat, lon, utc ? utc : time(nullptr), cloudQ, ground, aglM, labx, laby, moonPath,
                       outDir, true, cellCov, cellDen, cellDet, cellPitch, cellBelow, cellBank, cellFrames);

  const int width = 1280, height = 720, fps = 60;

  /* Camera: eye at ~aglM above ground, aimed along (yaw, pitch). Default pitch aims at the horizon;
   * --pitch DEG (+ = up) lets a shot frame the sky (moon/stars). Built in the eye's ENU frame. */
  double eye[3], target[3];
  GeoToEcef(lat, lon, ground + aglM, eye);
  double E3[3], N3[3], U3[3];
  { double P = lat * kPi / 180.0, L = lon * kPi / 180.0;
    double sP = std::sin(P), cP = std::cos(P), sL = std::sin(L), cL = std::cos(L);
    E3[0] = -sL; E3[1] = cL; E3[2] = 0.0;
    N3[0] = -sP * cL; N3[1] = -sP * sL; N3[2] = cP;
    U3[0] = cP * cL; U3[1] = cP * sL; U3[2] = sP; }
  double look = yawDeg * kPi / 180.0, pitch = pitchDeg * kPi / 180.0;
  double cp = std::cos(pitch);
  double fwd[3];
  for (int a = 0; a < 3; a++)
    fwd[a] = N3[a] * cp * std::cos(look) + E3[a] * cp * std::sin(look) + U3[a] * std::sin(pitch);
  { double l = std::sqrt(fwd[0]*fwd[0] + fwd[1]*fwd[1] + fwd[2]*fwd[2]); fwd[0]/=l; fwd[1]/=l; fwd[2]/=l; }
  for (int a = 0; a < 3; a++) target[a] = eye[a] + fwd[a] * 80000.0;

  /* Plausible HUD pose (no live sim here): level flight on the camera heading, loitering. */
  FlightBox::FBState hs{};
  hs.roll = 0.f; hs.pitch = (float)pitchDeg; hs.yaw = (float)yawDeg;
  hs.alt = (float)(ground + aglM); hs.gs = 220.f; hs.airspeed = 220.f; hs.vs = 0.f;
  hs.home_dist = 8000.f; hs.home_bearing = 45.f;
  hs.state = FlightBox::FBMode::Loiter;
  hs.cloud = (float)cloudCover;
  /* Real ephemeris sun + moon (EVS only; SVS renders a constant day regardless). */
  time_t clk = utc ? utc : time(nullptr);
  FlightBox::SunPos(lat, lon, clk, &hs.sun_el, &hs.sun_az);
  FlightBox::MoonPos(lat, lon, clk, &hs.moon_el, &hs.moon_az, &hs.moon_phase);
  printf("gpu_native: utc=%ld sun el=%.1f az=%.1f | moon el=%.1f az=%.1f phase=%.2f\n",
         (long)clk, hs.sun_el, hs.sun_az, hs.moon_el, hs.moon_az, hs.moon_phase);

  FlightBox::FBRenderer R;
  R.SetStreaming(512);
  R.SetDefaultMode(groundPhoto);   /* the --albedo mode is both the eager base and the initial view */
  R.SetGroundMode(groundPhoto);
  R.SetMoonScale(moonScale);
  R.SetCloudQuality(cloudQ);
  R.SetSkyClock((double)clk);
  {   /* NASA moon albedo + HYG star catalogue (EVS sky); optional, degrade gracefully */
    uint8_t *moon = 0; int mw = 0, mh = 0;
    if (fb_load_image_file(moonPath.c_str(), &moon, &mw, &mh)) {
      R.SetMoonTexture(moon, mw, mh); free(moon);
      printf("gpu_native: moon texture %dx%d (%s)\n", mw, mh, moonPath.c_str());
    } else printf("gpu_native: moon texture missing (%s) — grey fallback\n", moonPath.c_str());
    static uint8_t stars[262144];
    int sn = fb_fetch_stars(base.c_str(), stars, (int)sizeof stars);
    if (sn > 0) { R.SetStars(stars, sn, lat, lon); printf("gpu_native: star catalogue %d bytes (%d stars)\n", sn, sn / 6); }
    else printf("gpu_native: star catalogue unreachable (%s/t/stars)\n", base.c_str());
  }
  R.SetCamera(eye, target);
  R.SetHud(hs, true);
  static FlightBox::FBDisplaySystem hudDisplay;   /* no live module here — the generic default HUD */
  R.SetHudDisplay(&hudDisplay);
  R.InitOffscreen(width, height);
  if (!R.Ready()) { fprintf(stderr, "gpu_native: WebGPU device init failed\n"); return 1; }

  FlightBox::FBWorld W;
  if (!W.Open(&R, base.c_str(), lat, lon, 32, viewKm * 1000.0, 512)) {
    fprintf(stderr, "gpu_native: FBWorld open FAILED — is fb-tiles reachable at %s ?\n", base.c_str());
    return 1;
  }
  W.SetDefaultMode(groundPhoto);
  W.SetGroundMode(groundPhoto);
  W.SetNightLights(groundPhoto && hs.sun_el < -3.0f);   /* EVS night -> stream /t/lights */
  printf("gpu_native: streaming quadtree at %.4f/%.4f, %.0f m AGL, view %.0f km, albedo=%s, night=%d\n", lat, lon,
         aglM, viewKm, groundPhoto ? "photo" : "osm", (groundPhoto && hs.sun_el < -3.0f) ? 1 : 0);

  const int totalFrames = (int)(seconds * fps + 0.5);
  const int everyFrames = interval > 0.0 ? (int)(interval * fps + 0.5) : 0;
  int shot = 0;
  for (int f = 0; f < totalFrames; f++) {
    W.Update(lat, lon, eye, fwd, (double)f * 1000.0 / fps);
    R.RenderFrame();
    bool last = (f == totalFrames - 1);
    bool due = everyFrames > 0 && (f % everyFrames) == (everyFrames - 1);
    if (!due && !(everyFrames == 0 && last)) continue;
    std::vector<uint8_t> rgba;
    if (!R.ReadPixels(rgba)) { fprintf(stderr, "gpu_native: readback failed at frame %d\n", f); return 1; }
    char path[512];
    snprintf(path, sizeof path, "%s/frame_%04d.png", outDir.c_str(), shot++);
    if (!stbi_write_png(path, width, height, 4, rgba.data(), width * 4)) {
      fprintf(stderr, "gpu_native: PNG write failed: %s\n", path);
      return 1;
    }
    printf("gpu_native: wrote %s\n", path);
  }
  return 0;
}
