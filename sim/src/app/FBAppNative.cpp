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
#include "FBF16Module.h"
#include "FBMissionFile.h"
#include "FBMissionBoot.h"
#include "FBTerrainLoader.h"
#include "FBTelemetry.h"
#include "jsbsim_adapter.h"
#include <cerrno>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <sys/stat.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

/* jsbsim_adapter's fb_jsbsim_ functions and fb_fdm_state now live in namespace FlightBox (no longer a
 * C ABI — see jsbsim_adapter.h's banner); this file calls them unqualified throughout (matches
 * FBAppWasm.cpp). */
using namespace FlightBox;

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
          "  --mission FILE [--timeout N] [--interval S]  ground-spawn a .fbm mission (doc/mission-format.md)\n"
          "    on its runway threshold and run headless (JSBSim + FBF16Pilot's phase machine, NO renderer/\n"
          "    GPU device unless --interval > 0, in which case PNGs are written every --interval sim-\n"
          "    seconds -- this is the flying-frame oracle, the --fly replacement) until SUCCESS/CRASH/\n"
          "    TIMEOUT/FAIL; writes --out/telemetry.csv + --out/events.log, exit code 0/1/2/3. --timeout\n"
          "    overrides the mission file's own value.\n",
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
  hs.alt = (float)altMSL; hs.gs = 220.f; hs.airspeed = 220.f; hs.state = FlightBox::FBMode::Manual;
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

const char *PhaseStr(FlightBox::FBPilot::Phase p) {
  switch (p) {
    case FlightBox::FBPilot::Phase::Idle: return "Idle";
    case FlightBox::FBPilot::Phase::Preflight: return "Preflight";
    case FlightBox::FBPilot::Phase::Takeoff: return "Takeoff";
    case FlightBox::FBPilot::Phase::Climb: return "Climb";
    case FlightBox::FBPilot::Phase::Route: return "Route";
    case FlightBox::FBPilot::Phase::Approach: return "Approach";
    case FlightBox::FBPilot::Phase::Flare: return "Flare";
    case FlightBox::FBPilot::Phase::Rollout: return "Rollout";
    case FlightBox::FBPilot::Phase::Shutdown: return "Shutdown";
  }
  return "?";
}

enum class FBMissionResult { Success, Fail, Crash, Timeout };
const char *ResultStr(FBMissionResult r) {
  switch (r) {
    case FBMissionResult::Success: return "SUCCESS";
    case FBMissionResult::Fail: return "FAIL";
    case FBMissionResult::Crash: return "CRASH";
    case FBMissionResult::Timeout: return "TIMEOUT";
  }
  return "?";
}

/* "abseits der Runway" test for the CRASH gate: project (lat,lon) onto the runway's along/across-track
 * axes (centreline from the threshold on TrueHeadingDeg) — on the runway iff within its length (+
 * marginAlongM before/after) and half-width (+ marginAcrossM either side). WidthM <= 1 (the mission
 * format leaves it unset) falls back to a generous 60 m generic-runway half-width. */
bool OnRunway(const FlightBox::FBRunway &rwy, double lat, double lon, double marginAlongM, double marginAcrossM) {
  double hdg = rwy.TrueHeadingDeg * kPi / 180.0;
  double coslat = std::cos(rwy.ThresholdLatDeg * kPi / 180.0);
  double dy = (lat - rwy.ThresholdLatDeg) * 111320.0;
  double dx = (lon - rwy.ThresholdLonDeg) * 111320.0 * coslat;
  double along = dx * std::sin(hdg) + dy * std::cos(hdg);
  double across = dx * std::cos(hdg) - dy * std::sin(hdg);
  double halfW = (rwy.WidthM > 1.0 ? rwy.WidthM : 60.0) * 0.5 + marginAcrossM;
  return along >= -marginAlongM && along <= rwy.LengthM + marginAlongM && std::fabs(across) <= halfW;
}

void WriteCsvHeader(FILE *f) {
  fprintf(f, "t,phase,lat,lon,altM,aglM,casKt,mach,vsMs,pitchDeg,rollDeg,hdgDeg,aoaDeg,nz,throttleNorm,"
             "gearPos,wow,speedbrake,activeWp,distToWpM\n");
}

void WriteCsvRow(FILE *f, double t, FlightBox::FBPilot::Phase phase, const fb_fdm_state &s, double groundAsl,
                 double throttleNorm, double gearPos, int wow, double speedbrake, int activeWp, double distToWpM) {
  const double kMsToKt = 1.9438444924406;
  fprintf(f, "%.2f,%s,%.6f,%.6f,%.1f,%.1f,%.2f,%.4f,%.2f,%.2f,%.2f,%.1f,%.2f,%.3f,%.3f,%.3f,%d,%.3f,%d,%.0f\n",
          t, PhaseStr(phase), s.lat, s.lon, s.elev, s.elev - groundAsl, s.cas * kMsToKt, s.mach, s.vy,
          s.pitch, s.roll, s.yaw, s.alphaDeg, s.nz, throttleNorm, gearPos, wow, speedbrake, activeWp, distToWpM);
}

/* --mission: ground-spawns the mission's F-16 on its runway threshold via FBMissionGroundSpawn (WOW=1,
 * engines running, wheel brakes set, FBAutopilot in neutral Manual so the FLCS holds wings-level/
 * idle-throttle until FBPilot's Preflight hold ends) and steps it headless — FBPilot's phase machine
 * (systems/FBPilot.h) actually flies the takeoff/climb/route chain — until SUCCESS/CRASH/TIMEOUT/FAIL.
 * Default loop is CPU-only (JSBSim + fb_stream_ground for the DEM floor) — no FBRenderer/FBWorld
 * construction, hence no WebGPU/Dawn device init, unless
 * `renderIntervalS > 0` opts into periodic proof-frame PNGs (rendering is then a bolt-on inside the
 * loop, never a dependency of the physics/telemetry/termination logic above it). Deterministic fixed
 * 10 Hz decision tick (Prinzip 4: sim-seconds, not wall-clock) — FBF16Module substeps the FDM 100 Hz
 * internally per Run() call. */
int RunMission(const std::string &missionPath, double timeoutOverride, double renderIntervalS,
              const std::string &base, const std::string &outDir) {
  std::string csvPath = outDir + "/telemetry.csv", evPath = outDir + "/events.log";
  FILE *evf = fopen(evPath.c_str(), "w");
  if (!evf) { fprintf(stderr, "mission: cannot open %s for writing\n", evPath.c_str()); return 1; }
  double tLog0 = 0.0;
  auto logEvent = [&](double t, const char *fmt, ...) {
    fprintf(evf, "t=%.1f ", t);
    va_list ap; va_start(ap, fmt); vfprintf(evf, fmt, ap); va_end(ap);
    fprintf(evf, "\n"); fflush(evf);
  };

  std::ifstream in(missionPath);
  if (!in) {
    fprintf(stderr, "mission: cannot open %s\n", missionPath.c_str());
    logEvent(tLog0, "RESULT result=FAIL reason=\"cannot open %s\"", missionPath.c_str());
    fclose(evf);
    return 1;
  }
  std::stringstream buf;
  buf << in.rdbuf();
  FlightBox::FBMission mission;
  std::string perr;
  if (!FlightBox::FBParseMissionFile(buf.str(), mission, &perr)) {
    fprintf(stderr, "mission: parse FAILED: %s\n", perr.c_str());
    logEvent(tLog0, "RESULT result=FAIL reason=\"parse: %s\"", perr.c_str());
    fclose(evf);
    return 1;
  }

  double timeoutS = timeoutOverride > 0.0 ? timeoutOverride : mission.TimeoutS;
  const FlightBox::FBRunway &rwy = mission.Runway;
  logEvent(tLog0, "MISSION_START name=%s runway_hdg=%.0f timeout=%.0f", mission.Name.c_str(),
           rwy.TrueHeadingDeg, timeoutS);

  /* fb_stream_ground (and, later, FBWorld::Open) need fb_base set first — fb_stream_open does that
   * (FBTerrainLoader.h banner); the default no-render path never constructs an FBWorld, so it must be
   * called here directly instead of relying on FBWorld::Open to do it. */
  if (!fb_stream_open(base.c_str(), rwy.ThresholdLatDeg, rwy.ThresholdLonDeg, 8)) {
    fprintf(stderr, "mission: fb_stream_open FAILED (%s)\n", base.c_str());
    logEvent(tLog0, "RESULT result=FAIL reason=\"stream open\"");
    fclose(evf);
    return 1;
  }
  double groundAsl = fb_stream_ground(rwy.ThresholdLatDeg, rwy.ThresholdLonDeg);
  if (groundAsl <= -1e8) {
    fprintf(stderr, "mission: fb-tiles unreachable at spawn (%s) — is fb-tiles up?\n", base.c_str());
    logEvent(tLog0, "RESULT result=FAIL reason=\"tiles unreachable at spawn\"");
    fclose(evf);
    return 1;
  }
  printf("mission: %s spawn %.5f/%.5f DEM ground=%.1f m (mission file elev %.1f m) hdg=%.1f len=%.0f m\n",
         mission.Name.c_str(), rwy.ThresholdLatDeg, rwy.ThresholdLonDeg, groundAsl, rwy.ThresholdElevM,
         rwy.TrueHeadingDeg, rwy.LengthM);

  auto F16 = std::make_unique<FlightBox::FBF16Module>();
  fb_fdm_state St{};
  /* hoff_m < 0 = "sit on the gear" (see the adapter banner): re-places the CG at the model's true
   * gear-down clearance after IC, so the spawn altitude is geometry-true, not an assumed offset.
   * speed_ms=0, fbw_override=0 (the F-16's OWN FLCS stays the controller, our FCS just commands it, per
   * CLAUDE.md's F-16 edge). FBMissionGroundSpawn also arms FBPilot at Preflight — the SAME ground-hold
   * setup FBAppWasm.cpp's mission boot uses (FBMissionBoot.h: no duplicated spawn logic). */
  if (!FlightBox::FBMissionGroundSpawn("vendor/jsbsim/aircraft", mission, groundAsl, *F16, St)) {
    fprintf(stderr, "mission: JSBSim init FAILED\n");
    logEvent(tLog0, "RESULT result=FAIL reason=\"jsbsim init\"");
    fclose(evf);
    return 1;
  }

  const bool wantRender = renderIntervalS > 0.0;
  std::unique_ptr<FlightBox::FBRenderer> R;
  std::unique_ptr<FlightBox::FBWorld> W;
  const int width = 1280, height = 720;
  int shot = 0;
  if (wantRender) {
    R = std::make_unique<FlightBox::FBRenderer>();
    R->SetDefaultMode(0);
    R->SetGroundMode(0);
    R->SetStreaming(512);
    time_t clk = time(nullptr);
    R->SetSkyClock((double)clk);
    { uint8_t *moon = 0; int mw = 0, mh = 0;
      if (fb_load_image_file("flightbox/web/moon.jpg", &moon, &mw, &mh)) { R->SetMoonTexture(moon, mw, mh); free(moon); } }
    W = std::make_unique<FlightBox::FBWorld>();
    if (!W->Open(R.get(), base.c_str(), rwy.ThresholdLatDeg, rwy.ThresholdLonDeg, 32, 30000.0, 512)) {
      fprintf(stderr, "mission: FBWorld open FAILED — is fb-tiles reachable at %s ?\n", base.c_str());
      logEvent(tLog0, "RESULT result=FAIL reason=\"world open\"");
      fclose(evf);
      return 1;
    }
    R->SetHudDisplay(&F16->Displays());
    R->InitOffscreen(width, height);
    if (!R->Ready()) {
      fprintf(stderr, "mission: WebGPU device init failed\n");
      logEvent(tLog0, "RESULT result=FAIL reason=\"gpu init\"");
      fclose(evf);
      return 1;
    }
    /* Warm the terrain cut around the spawn before the first PNG (no full loading-screen gate here —
     * the jet is stationary this round, an approximate cut is enough for the proof frame). */
    double eye0[3], fwd0[3], right0[3], up0[3];
    GeoToEcef(rwy.ThresholdLatDeg, rwy.ThresholdLonDeg, groundAsl + 2.0, eye0);
    CameraBasis(rwy.TrueHeadingDeg, -2.0, 0.0, rwy.ThresholdLatDeg, rwy.ThresholdLonDeg, fwd0, right0, up0);
    for (int i = 0; i < 60; i++) W->Update(rwy.ThresholdLatDeg, rwy.ThresholdLonDeg, eye0, fwd0, (double)i * 1000.0 / 15.0);
  }

  FILE *csv = fopen(csvPath.c_str(), "w");
  if (!csv) {
    fprintf(stderr, "mission: cannot open %s for writing\n", csvPath.c_str());
    logEvent(tLog0, "RESULT result=FAIL reason=\"cannot open telemetry.csv\"");
    fclose(evf);
    return 1;
  }
  WriteCsvHeader(csv);

  const double dt = 0.1;             /* 10 Hz decision tick == FBF16Module's Sensor/Pilot cadence */
  const double kWpCaptureM = 500.0;  /* generic waypoint capture radius */
  double simT = 0.0, renderAcc = 0.0;
  FBMissionResult result = FBMissionResult::Timeout;
  std::string reason = "sim time exceeded the mission timeout";
  auto lastPhase = F16->PilotSystem().GetPhase();
  bool warnedStall = false, warnedOverspeed = false, warnedSink = false;
  clock_t wallStart = clock();

  while (simT < timeoutS) {
    double gnd = fb_stream_ground(St.lat, St.lon);
    if (gnd > -1e8) groundAsl = gnd;
    F16->SetGroundAsl((float)groundAsl);
    fb_jsbsim_set_ground(groundAsl);

    F16->Run(St, dt, nullptr);   /* world=nullptr: no world-facing system is real yet (FBSystemSlots.h NoOp) */
    simT += dt;

    auto phase = F16->PilotSystem().GetPhase();
    if (phase != lastPhase) { logEvent(simT, "PHASE from=%s to=%s", PhaseStr(lastPhase), PhaseStr(phase)); lastPhase = phase; }

    FlightBox::FBFlightPlan &plan = F16->FlightPlan();
    int activeWp = plan.ActiveIndex();
    double distToWpM = -1.0;
    if (const FlightBox::FBWaypoint *wp = plan.ActiveWaypoint()) {
      double dy = (St.lat - wp->LatDeg) * 111320.0, dx = (St.lon - wp->LonDeg) * 111320.0 * std::cos(St.lat * kPi / 180.0);
      distToWpM = std::sqrt(dx * dx + dy * dy);
    }
    int reachedWp = FlightBox::FBMissionAdvanceWaypoint(plan, St.lat, St.lon, kWpCaptureM);
    if (reachedWp >= 0) {
      const FlightBox::FBWaypoint &wp = plan.At(reachedWp);
      logEvent(simT, "WP_REACHED idx=%d lat=%.5f lon=%.5f distM=%.0f", reachedWp, wp.LatDeg, wp.LonDeg, distToWpM);
    }

    /* AoA is numerically undefined at near-zero airspeed (atan2 of two near-zero velocity components) —
     * a stationary/taxiing jet reads tens of noise degrees on the FIRST settle-transient ticks (measured:
     * up to ~168 deg for <2s after a ground spawn, see the mission-format proof run); gate both checks on
     * a real airspeed the same way a real AoA/Mach indicator is unreliable/inhibited near zero. */
    bool haveAirspeed = St.cas > 15.0;   /* ~30 kt */
    if (haveAirspeed && St.alphaDeg > 25.0 && !warnedStall) { logEvent(simT, "WARN kind=stall aoaDeg=%.1f", St.alphaDeg); warnedStall = true; }
    else if (!haveAirspeed || St.alphaDeg < 20.0) warnedStall = false;
    if (St.mach > 1.2 && !warnedOverspeed) { logEvent(simT, "WARN kind=overspeed mach=%.2f", St.mach); warnedOverspeed = true; }
    else if (St.mach < 1.1) warnedOverspeed = false;
    double aglM = St.elev - groundAsl;
    if (aglM < 150.0 && St.vy < -15.0 && !warnedSink) { logEvent(simT, "WARN kind=sink vsMs=%.1f aglM=%.0f", St.vy, aglM); warnedSink = true; }
    else if (St.vy > -5.0) warnedSink = false;

    bool wow = fb_jsbsim_get_wow(-1) != 0;
    bool penetration = aglM < -3.0;
    bool offRunwayContact = wow && !OnRunway(rwy, St.lat, St.lon, 50.0, 30.0);
    if (penetration || offRunwayContact) {
      result = FBMissionResult::Crash;
      reason = penetration ? "ground penetration" : "hard contact off the runway";
      F16->Controls().EngineCutoff();   /* CLAUDE.md: crash -> motor aus, ground reactions do the rest */
      logEvent(simT, "WARN kind=crash detail=\"%s\" lat=%.5f lon=%.5f aglM=%.1f", reason.c_str(), St.lat, St.lon, aglM);
      break;
    }

    double throttle = fb_jsbsim_get_throttle();
    double gearPos = fb_jsbsim_get_gear_pos();
    double speedbrake = fb_jsbsim_get_speedbrake_pos();
    WriteCsvRow(csv, simT, phase, St, groundAsl, throttle, gearPos, wow ? 1 : 0, speedbrake, activeWp, distToWpM);

    if (wantRender) {
      renderAcc += dt;
      if (renderAcc >= renderIntervalS) {
        renderAcc = 0.0;
        double eye[3], fwd[3], right[3], up[3];
        GeoToEcef(St.lat, St.lon, St.elev, eye);
        CameraBasis(St.yaw, St.pitch, St.roll, St.lat, St.lon, fwd, right, up);
        R->SetCameraBasis(eye, fwd, right, up);
        FlightBox::FBState hs = F16->Telemetry();
        hs.roll = (float)St.roll; hs.pitch = (float)St.pitch; hs.yaw = (float)St.yaw;
        hs.alt = (float)St.elev; hs.gs = (float)St.gs; hs.airspeed = (float)St.speed; hs.vs = (float)St.vy;
        hs.state = FlightBox::FBMode::Manual;
        FlightBox::SunPos(St.lat, St.lon, time(nullptr), &hs.sun_el, &hs.sun_az);
        FlightBox::MoonPos(St.lat, St.lon, time(nullptr), &hs.moon_el, &hs.moon_az, &hs.moon_phase);
        R->SetHud(hs, true);
        R->SetAgl((float)aglM);
        W->Update(St.lat, St.lon, eye, fwd, simT * 1000.0);
        R->RenderFrame();
        std::vector<uint8_t> rgba;
        if (R->ReadPixels(rgba)) {
          char path[512];
          snprintf(path, sizeof path, "%s/mission_%04d.png", outDir.c_str(), shot++);
          if (stbi_write_png(path, width, height, 4, rgba.data(), width * 4))
            printf("gpu_native --mission: wrote %s\n", path);
        }
      }
    }

    if (phase == FlightBox::FBPilot::Phase::Shutdown) { result = FBMissionResult::Success; reason = "mission phases complete"; break; }
  }

  double wallS = (double)(clock() - wallStart) / CLOCKS_PER_SEC;
  logEvent(simT, "RESULT result=%s reason=\"%s\" phase=%s lat=%.5f lon=%.5f altM=%.1f durationS=%.1f",
           ResultStr(result), reason.c_str(), PhaseStr(F16->PilotSystem().GetPhase()), St.lat, St.lon, St.elev, simT);
  fclose(evf);
  fclose(csv);

  printf("summary result=%s durationS=%.1f wallS=%.2f speedup=%.0fx phase=%s lat=%.5f lon=%.5f altM=%.1f\n",
         ResultStr(result), simT, wallS, wallS > 0.0 ? simT / wallS : 0.0,
         PhaseStr(F16->PilotSystem().GetPhase()), St.lat, St.lon, St.elev);

  switch (result) {
    case FBMissionResult::Success: return 0;
    case FBMissionResult::Fail: return 1;
    case FBMissionResult::Crash: return 2;
    case FBMissionResult::Timeout: return 3;
  }
  return 1;
}

}  // namespace

int main(int argc, char **argv) {
  double lat = 47.18, lon = 7.41, seconds = 3.0, interval = 1.0;
  double ground = 430.0, aglM = 1500.0, viewKm = 240.0, yawDeg = 0.0, pitchDeg = -3.0, cloudCover = 0.0, moonScale = 1.0, cloudQ = 1.0;
  int groundPhoto = 0, cloudLab = 0, cloudCell = 0;
  float cellCov = 0.6f, cellDen = 5.0f, cellDet = 1.3f;
  double cellPitch = -999.0, cellBelow = -5000.0, cellBank = 12.0;   /* default: above the deck (AC7 vantage) */
  int cellFrames = 24;
  std::string labx = "coverage", laby = "density";
  time_t utc = 0;   /* 0 = real wall clock */
  std::string base = "http://localhost:8081", outDir = ".", moonPath = "flightbox/web/moon.jpg";
  std::string missionPath;
  double missionTimeout = 0.0;
  bool intervalSet = false;   /* --mission: renderer/GPU device is opt-in ONLY when --interval was given */
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
    else if (a == "--interval" && i + 1 < argc) { interval = atof(argv[++i]); intervalSet = true; }
    else if (a == "--out" && i + 1 < argc) outDir = argv[++i];
    else if (a == "--mission" && i + 1 < argc) missionPath = argv[++i];
    else if (a == "--timeout" && i + 1 < argc) missionTimeout = atof(argv[++i]);   /* --mission: overrides the .fbm's own timeout */
    else { Usage(argv[0]); return 1; }
  }
  if (!EnsureDir(outDir)) { fprintf(stderr, "gpu_native: cannot create --out %s\n", outDir.c_str()); return 1; }

  if (!missionPath.empty())
    return RunMission(missionPath, missionTimeout, intervalSet ? interval : 0.0, base, outDir);
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
  hs.state = FlightBox::FBMode::Manual;
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
