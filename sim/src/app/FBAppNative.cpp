/* The native frame ORACLE: FBRenderer's offscreen mode through the same pipeline the browser runs,
 * dumping PNGs. This is the verification venue a headless-browser SwiftShader cannot give — native
 * Dawn actually renders. doc/flightbox/rendering.md, Abschnitt 1.1. */
#include "FBRenderer.h"
#include "FBWorld.h"
#include "FBCamera.h"
#include "FBEphemeris.h"
#include "FBGeodesy.h"
#include "FBUnits.h"
#include "FBMissionRunner.h"
#include "FBTerrainLoader.h"
#include "FBTilesElevation.h"
#include "FBLog.h"
#include "FBLogSinks.h"
#include "FBFdm.h"
#include <cerrno>
#include <cmath>
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

using namespace FlightBox;



namespace {

void Usage(const char *argv0) {
  fprintf(stderr,
          "usage: %s [--lat D] [--lon D] [--ground M] [--agl M] [--view KM] [--yaw DEG] [--pitch DEG]\n"
          "          [--albedo osm|photo] [--utc SECS] [--cloud C] [--cloudq Q] [--moonscale S] [--moon PATH]\n"
          "          [--base URL] [--seconds N] [--interval M] [--out DIR]\n"
          "  --cloudlab [--labx PARAM] [--laby PARAM]  render an N x M cloud-parameter grid to ONE PNG.\n"
          "    PARAM in {coverage,density,extinct,suni,detail}; fixed cloud-bank camera, no terrain.\n"
          "  --mission FILE [--timeout N] [--interval S]  ground-spawn a .fbm mission (doc/mission-format.md)\n"
          "    on its runway threshold and run headless (JSBSim + the module's FBPilot phase machine, NO renderer/\n"
          "    GPU device unless --interval > 0, in which case PNGs are written every --interval sim-\n"
          "    seconds -- this is the flying-frame oracle, the --fly replacement) until SUCCESS/CRASH/\n"
          "    TIMEOUT/FAIL; writes --out/telemetry.csv + --out/events.log, exit code 0/1/2/3. --timeout\n"
          "    overrides the mission file's own value.\n",
          argv0);
}

/* A lab parameter's sweep range. */
static void LabRange(const std::string &p, float &lo, float &hi) {
  if (p == "coverage") { lo = 0.30f; hi = 0.82f; }
  else if (p == "density") { lo = 2.0f; hi = 10.0f; }
  else if (p == "extinct") { lo = 0.03f; hi = 0.16f; }
  else if (p == "suni") { lo = 6.0f; hi = 40.0f; }
  else if (p == "detail") { lo = 0.4f; hi = 2.6f; }
  else { lo = 0.0f; hi = 1.0f; }
}

/* A cols x rows grid of parameter variants in ONE PNG: fixed camera, no terrain streaming. */
int RunCloudLab(double lat, double lon, time_t utc, double cloudQ, double ground, double aglM,
                const std::string &labx, const std::string &laby, const std::string &moonPath,
                const std::string &outDir, bool singleCell = false, float cov0 = 0.6f,
                float den0 = 5.0f, float det0 = 1.3f, double pitchOverrideDeg = -999.0,
                double camBelowM = -5000.0, double bankKm = 12.0, int frames = 24) {   /* default: view the deck from ABOVE (AC7 vantage) */
  const int W = 1280, H = 720;
  const double kPi2 = kPi;   /* was a 15-digit truncation of the same pi (core/FBUnits.h) */
  /* FRONTAL framing, below the deck base and aimed at a bank ~12 km ahead: the silhouette reads
   * face-on instead of grazing the deck edge-on. Deck-relative, so it is deterministic across runs. */
  const double deckBaseAGL = 1500.0, deckTopAGL = 4100.0, deckMidAGL = 0.5 * (deckBaseAGL + deckTopAGL);
  const double camAGL = deckBaseAGL - camBelowM, bankDistM = bankKm * 1000.0;
  (void)aglM;
  double altMSL = ground + camAGL;
  double eye[3];
  FBGeoToEcef(lat, lon, altMSL, eye);
  FlightBox::FBState hs{};
  hs.Platform.AltM = (float)altMSL; hs.Platform.GsMs = 220.f; hs.Platform.TasMs = 220.f; hs.Platform.Mode = FlightBox::FBMode::Manual;
  FlightBox::SunPos(lat, lon, utc, &hs.Env.SunElDeg, &hs.Env.SunAzDeg);
  FlightBox::MoonPos(lat, lon, utc, &hs.Env.MoonElDeg, &hs.Env.MoonAzDeg, &hs.Env.MoonPhase);
  double E3[3], N3[3], U3[3];
  FBEnuAxesEcef(lat, lon, E3, N3, U3);
  /* 42 deg OFF the sun azimuth: side-lit shows edge light and self-shadow instead of a blinding disc. */
  double yawDeg = hs.Env.SunAzDeg + 42.0, yaw = yawDeg * kPi2 / 180.0;
  double riseM = (pitchOverrideDeg > -900.0) ? bankDistM * std::tan(pitchOverrideDeg * kPi2 / 180.0)
                                             : deckMidAGL - camAGL;
  hs.Platform.YawDeg = (float)yawDeg; hs.Platform.PitchDeg = (float)(std::atan2(riseM, bankDistM) * 180.0 / kPi2);
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
  if (!R.Ready()) { FlightBox::FBLog::Error("cloudlab", "device_init_failed"); return 1; }

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
    if (!R.ReadPixels(img)) { FlightBox::FBLog::Error("cloudlab", "readback_failed"); return 1; }
    char cpath[512];
    snprintf(cpath, sizeof cpath, "%s/cloudcell_cov%.2f_den%.1f_det%.1f_p%.1f.png",
             outDir.c_str(), cov0, den0, det0, (double)hs.Platform.PitchDeg);
    stbi_write_png(cpath, W, H, 4, img.data(), W * 4);
    FlightBox::FBLog::Info("cloudlab", "wrote_cell", {{"path", cpath}, {"w", W}, {"h", H}, {"cov", (double)cov0},
        {"den", (double)den0}, {"det", (double)det0}, {"pitch", (double)hs.Platform.PitchDeg}, {"camBelow", camBelowM},
        {"bankKm", bankKm}, {"sunEl", (double)hs.Env.SunElDeg}});
    return 0;
  }
  const int cols = 4, rows = 3, cw = W / 4, ch = H / 4, gw = cols * cw, gh = rows * ch;
  std::vector<uint8_t> grid((size_t)gw * gh * 4, 25);
  float lox, hix, loy, hiy;
  LabRange(labx, lox, hix);
  LabRange(laby, loy, hiy);
  FlightBox::FBLog::Info("cloudlab", "grid", {{"cols", cols}, {"rows", rows}, {"labx", labx}, {"lox", (double)lox},
      {"hix", (double)hix}, {"laby", laby}, {"loy", (double)loy}, {"hiy", (double)hiy}, {"sunEl", (double)hs.Env.SunElDeg}});
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
      if (!R.ReadPixels(img)) { FlightBox::FBLog::Error("cloudlab", "readback_failed"); return 1; }
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
      FlightBox::FBLog::Debug("cloudlab", "cell", {{"col", c}, {"row", r}, {"labx", labx}, {"vx", (double)vx},
                                                   {"laby", laby}, {"vy", (double)vy}});
    }
  char path[512];
  snprintf(path, sizeof path, "%s/cloudlab_%s_x_%s.png", outDir.c_str(), labx.c_str(), laby.c_str());
  stbi_write_png(path, gw, gh, 4, grid.data(), gw * 4);
  FlightBox::FBLog::Info("cloudlab", "wrote_grid", {{"path", path}, {"w", gw}, {"h", gh},
      {"ref", "cauliflower silhouette, flat-ish base, self-shadow, sun-side edge light, no straight edges >20px"}});
  return 0;
}

/* The concrete FBMissionTickHook, implemented ONLY in this translation unit — which is what keeps
 * fb-gym's link GPU-free while both clients share one mission loop. */
class FBNativeMissionHook : public FlightBox::FBMissionTickHook {
public:
  FBNativeMissionHook(std::string base, std::string outDir, double intervalS, int width = 1280, int height = 720)
      : Base(std::move(base)), OutDir(std::move(outDir)), IntervalS(intervalS), Width(width), Height(height) {}

  void OnMissionStart(const FlightBox::FBSpawn &spawn, const FlightBox::FBActorList &actors,
                      const FlightBox::FBUnitRegistry &units) override {
    const FlightBox::FBSimUnit &primary = *actors.front();   /* the camera's actor (FBMissionRunner.h) */
    R = std::make_unique<FlightBox::FBRenderer>();
    R->SetDefaultMode(0);
    R->SetGroundMode(0);
    R->SetStreaming(512);
    time_t clk = time(nullptr);
    R->SetSkyClock((double)clk);
    { uint8_t *moon = 0; int mw = 0, mh = 0;
      if (fb_load_image_file("flightbox/web/moon.jpg", &moon, &mw, &mh)) { R->SetMoonTexture(moon, mw, mh); free(moon); } }
    W = std::make_unique<FlightBox::FBWorld>();
    if (!W->Open(R.get(), Base.c_str(), spawn.LatDeg, spawn.LonDeg, 32, 30000.0, 512)) {
      FlightBox::FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", "world open"}});
      R.reset(); W.reset();
      return;
    }
    R->SetHudDisplay(&primary.Displays());
    R->InitOffscreen(Width, Height);
    if (!R->Ready()) {
      FlightBox::FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", "gpu init"}});
      R.reset(); W.reset();
      return;
    }
    /* Borrowed: the renderer's VIEW of the cast, never a second list of its own. */
    W->SetUnits(&units);
    /* Warm the terrain cut before the first PNG; the jet is stationary, so an approximate cut does. */
    double altAsl0 = primary.GroundAslM() + (spawn.Ground ? 2.0 : (spawn.AltM - primary.GroundAslM()));
    double eye0[3], fwd0[3], right0[3], up0[3];
    FBGeoToEcef(spawn.LatDeg, spawn.LonDeg, altAsl0, eye0);
    FBCameraBasisEcef(spawn.HeadingDeg, -2.0, 0.0, spawn.LatDeg, spawn.LonDeg, fwd0, right0, up0);
    for (int i = 0; i < 60; i++) W->Update(spawn.LatDeg, spawn.LonDeg, eye0, fwd0, (double)i * 1000.0 / 15.0);
  }

  void OnTick(const FlightBox::FBActorList &actors, double simT) override {
    if (!R || !W) return;   /* OnMissionStart already logged the failure */
    Acc += 0.1;   /* dt = the runner's fixed 10 Hz decision tick, see FBMissionRunner.cpp */
    if (Acc < IntervalS) return;
    Acc = 0.0;
    const FlightBox::FBSimUnit &primary = *actors.front();
    FlightBox::FBUnitPose p = primary.GetPose();   /* the camera rides the unit, not a raw FDM POD */
    double eye[3], fwd[3], right[3], up[3];
    FBGeoToEcef(p.LatDeg, p.LonDeg, p.ElevM, eye);
    FBCameraBasisEcef(p.YawDeg, p.PitchDeg, p.RollDeg, p.LatDeg, p.LonDeg, fwd, right, up);
    R->SetCameraBasis(eye, fwd, right, up);
    FlightBox::FBState hs = primary.HudState();   /* module telemetry + this tick's live pose */
    hs.Platform.Mode = FlightBox::FBMode::Manual;
    FlightBox::SunPos(p.LatDeg, p.LonDeg, time(nullptr), &hs.Env.SunElDeg, &hs.Env.SunAzDeg);
    FlightBox::MoonPos(p.LatDeg, p.LonDeg, time(nullptr), &hs.Env.MoonElDeg, &hs.Env.MoonAzDeg, &hs.Env.MoonPhase);
    R->SetHud(hs, true);
    R->SetAgl((float)primary.AglM());
    W->Update(p.LatDeg, p.LonDeg, eye, fwd, simT * 1000.0);
    R->RenderFrame();
    std::vector<uint8_t> rgba;
    if (R->ReadPixels(rgba)) {
      char path[512];
      snprintf(path, sizeof path, "%s/mission_%04d.png", OutDir.c_str(), Shot++);
      if (stbi_write_png(path, Width, Height, 4, rgba.data(), Width * 4))
        FlightBox::FBLog::Debug("mission", "frame_written", {{"path", path}});
    }
  }

private:
  std::string Base, OutDir;
  double IntervalS;
  int Width, Height;
  std::unique_ptr<FlightBox::FBRenderer> R;
  std::unique_ptr<FlightBox::FBWorld> W;
  double Acc = 0.0;
  int Shot = 0;
};

/* No FBRenderer/FBWorld/GPU device at all unless `renderIntervalS > 0` — the renderer is a bolt-on
 * here, never a dependency of the physics or the termination logic. */
int RunMission(const std::string &missionPath, double timeoutOverride, double renderIntervalS,
              const std::string &base, const std::string &outDir) {
  FlightBox::FBTilesElevation elevation(base.c_str());
  if (renderIntervalS > 0.0) {
    FBNativeMissionHook hook(base, outDir, renderIntervalS);
    return FlightBox::FBRunMission(missionPath, timeoutOverride, outDir, FlightBox::FBNativeModelRoots(), elevation, &hook);
  }
  return FlightBox::FBRunMission(missionPath, timeoutOverride, outDir, FlightBox::FBNativeModelRoots(), elevation, nullptr);
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
  if (!FlightBox::FBEnsureDir(outDir)) { fprintf(stderr, "gpu_native: cannot create --out %s\n", outDir.c_str()); return 1; }

  /* FBRunMission installs its OWN sink; everything else here just wants console visibility. */
  static FlightBox::FBStdoutLogSink gStdoutSink;
  FlightBox::FBLog::SetSink(&gStdoutSink);
  FlightBox::FBLog::SetLevel(FlightBox::FBLogLevel::Debug);

  if (!missionPath.empty())
    return RunMission(missionPath, missionTimeout, intervalSet ? interval : 0.0, base, outDir);
  if (cloudLab)
    return RunCloudLab(lat, lon, utc ? utc : time(nullptr), cloudQ, ground, aglM, labx, laby, moonPath, outDir);
  if (cloudCell)
    return RunCloudLab(lat, lon, utc ? utc : time(nullptr), cloudQ, ground, aglM, labx, laby, moonPath,
                       outDir, true, cellCov, cellDen, cellDet, cellPitch, cellBelow, cellBank, cellFrames);

  const int width = 1280, height = 720, fps = 60;

  /* Built in the eye's ENU frame; --pitch (+ = up) lets a shot frame the sky. */
  double eye[3], target[3];
  FBGeoToEcef(lat, lon, ground + aglM, eye);
  double E3[3], N3[3], U3[3];
  FBEnuAxesEcef(lat, lon, E3, N3, U3);
  double look = yawDeg * kPi / 180.0, pitch = pitchDeg * kPi / 180.0;
  double cp = std::cos(pitch);
  double fwd[3];
  for (int a = 0; a < 3; a++)
    fwd[a] = N3[a] * cp * std::cos(look) + E3[a] * cp * std::sin(look) + U3[a] * std::sin(pitch);
  { double l = std::sqrt(fwd[0]*fwd[0] + fwd[1]*fwd[1] + fwd[2]*fwd[2]); fwd[0]/=l; fwd[1]/=l; fwd[2]/=l; }
  for (int a = 0; a < 3; a++) target[a] = eye[a] + fwd[a] * 80000.0;

  /* No live sim on this path: a plausible level-flight pose. */
  FlightBox::FBState hs{};
  hs.Platform.RollDeg = 0.f; hs.Platform.PitchDeg = (float)pitchDeg; hs.Platform.YawDeg = (float)yawDeg;
  hs.Platform.AltM = (float)(ground + aglM); hs.Platform.GsMs = 220.f; hs.Platform.TasMs = 220.f; hs.Platform.VsMs = 0.f;
  hs.Platform.HomeDistM = 8000.f; hs.Platform.HomeBearingDeg = 45.f;
  hs.Platform.Mode = FlightBox::FBMode::Manual;
  hs.Env.CloudCover = (float)cloudCover;
  /* EVS only; SVS renders a constant day regardless. */
  time_t clk = utc ? utc : time(nullptr);
  FlightBox::SunPos(lat, lon, clk, &hs.Env.SunElDeg, &hs.Env.SunAzDeg);
  FlightBox::MoonPos(lat, lon, clk, &hs.Env.MoonElDeg, &hs.Env.MoonAzDeg, &hs.Env.MoonPhase);
  FlightBox::FBLog::Info("gpu", "ephemeris", {{"utc", (int)clk}, {"sunEl", (double)hs.Env.SunElDeg},
      {"sunAz", (double)hs.Env.SunAzDeg}, {"moonEl", (double)hs.Env.MoonElDeg}, {"moonAz", (double)hs.Env.MoonAzDeg},
      {"moonPhase", (double)hs.Env.MoonPhase}});

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
      FlightBox::FBLog::Info("gpu", "moon_texture", {{"w", mw}, {"h", mh}, {"path", moonPath}});
    } else FlightBox::FBLog::Warn("gpu", "moon_texture_missing", {{"path", moonPath}});
    static uint8_t stars[262144];
    int sn = fb_fetch_stars(base.c_str(), stars, (int)sizeof stars);
    if (sn > 0) { R.SetStars(stars, sn, lat, lon); FlightBox::FBLog::Info("gpu", "star_catalogue", {{"bytes", sn}, {"stars", sn / 6}}); }
    else FlightBox::FBLog::Warn("gpu", "star_catalogue_unreachable", {{"base", base}});
  }
  R.SetCamera(eye, target);
  R.SetHud(hs, true);
  static FlightBox::FBDisplaySystem hudDisplay;   /* no live module here — the generic default HUD */
  R.SetHudDisplay(&hudDisplay);
  R.InitOffscreen(width, height);
  if (!R.Ready()) { FlightBox::FBLog::Error("gpu", "device_init_failed"); return 1; }

  FlightBox::FBWorld W;
  if (!W.Open(&R, base.c_str(), lat, lon, 32, viewKm * 1000.0, 512)) {
    FlightBox::FBLog::Error("gpu", "world_open_failed", {{"base", base}});
    return 1;
  }
  W.SetDefaultMode(groundPhoto);
  W.SetGroundMode(groundPhoto);
  W.SetNightLights(groundPhoto && hs.Env.SunElDeg < -3.0f);   /* EVS night -> stream /t/lights */
  FlightBox::FBLog::Info("gpu", "streaming_quadtree", {{"lat", lat}, {"lon", lon}, {"aglM", aglM},
      {"viewKm", viewKm}, {"albedo", groundPhoto ? "photo" : "osm"}, {"night", groundPhoto && hs.Env.SunElDeg < -3.0f}});

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
    if (!R.ReadPixels(rgba)) { FlightBox::FBLog::Error("gpu", "readback_failed", {{"frame", f}}); return 1; }
    char path[512];
    snprintf(path, sizeof path, "%s/frame_%04d.png", outDir.c_str(), shot++);
    if (!stbi_write_png(path, width, height, 4, rgba.data(), width * 4)) {
      FlightBox::FBLog::Error("gpu", "png_write_failed", {{"path", path}});
      return 1;
    }
    FlightBox::FBLog::Info("gpu", "frame_written", {{"path", path}});
  }
  return 0;
}
