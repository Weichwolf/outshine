/* The native frame ORACLE: FBRenderer's offscreen mode through the same pipeline the browser runs,
 * dumping PNGs. This is the verification venue a headless-browser SwiftShader cannot give — native
 * Dawn actually renders. doc/render/renderer.md, Abschnitt 1.1. */
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
#include "FBCloudDensity.h"
#include "FBFixedWeather.h"
#include "stages/FBCloudDensityWGSL.h"   /* the shader half of the density function, for --cloudcheck */
#include <algorithm>
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
          "          [--base URL] [--seconds N] [--interval M] [--out DIR] [--wx BLOB]\n"
          "  --wx BLOB  the SCREENSHOT venue's weather (an FBWX blob). A mission carries its own in its\n"
          "    `wx` line (missions/FBWeatherBoot.h), so --wx with --mission is rejected, not ignored.\n"
          "  --cloudcheck  evaluate core/FBCloudDensity.h and its WGSL twin over a sample set on the GPU\n"
          "    and print the largest disagreement; exit 0 iff it is inside the stated tolerance.\n"
          "  --mission FILE [--timeout N] [--interval S]  ground-spawn a .fbm mission (doc/mission-format.md)\n"
          "    on its runway threshold and run headless (JSBSim + the module's FBPilot phase machine, NO renderer/\n"
          "    GPU device unless --interval > 0, in which case PNGs are written every --interval sim-\n"
          "    seconds -- this is the flying-frame oracle, the --fly replacement) until SUCCESS/CRASH/\n"
          "    TIMEOUT/FAIL; writes --out/telemetry.csv + --out/events.log, exit code 0/1/2/3. --timeout\n"
          "    overrides the mission file's own value.\n",
          argv0);
}

/* --cloudcheck: the acceptance test of the SHARED cloud density function. The same sample set goes
 * through core/FBCloudDensity.h on the CPU and through its WGSL transliteration on the GPU; the largest
 * disagreement is printed. Not a picture — the point of a shared function is that it is a NUMBER a
 * sensor could read, so the proof has to be numeric. */
static constexpr double kCloudCheckTolerance = 1.0e-4;   /* [SET] f32 round-off incl. possible FMA fusion */

int RunCloudDensityCheck(void) {
  static constexpr auto kTimedWaitAny = wgpu::InstanceFeatureName::TimedWaitAny;
  wgpu::InstanceDescriptor id{};
  id.requiredFeatureCount = 1;
  id.requiredFeatures = &kTimedWaitAny;
  wgpu::Instance instance = wgpu::CreateInstance(&id);
  wgpu::Adapter adapter;
  wgpu::RequestAdapterOptions ao{};
  instance.WaitAny(instance.RequestAdapter(&ao, wgpu::CallbackMode::WaitAnyOnly,
      [&adapter](wgpu::RequestAdapterStatus st, wgpu::Adapter a, wgpu::StringView) {
        if (st == wgpu::RequestAdapterStatus::Success) adapter = a;
      }), UINT64_MAX);
  if (!adapter) { FBLog::Error("cloudcheck", "no_adapter"); return 1; }
  wgpu::Device device;
  wgpu::DeviceDescriptor dd{};
  dd.SetUncapturedErrorCallback([](const wgpu::Device &, wgpu::ErrorType t, wgpu::StringView m) {
    FBLog::Error("cloudcheck", "gpu_error", {{"type", (int)t}, {"msg", std::string(m.data, m.length)}});
  });
  instance.WaitAny(adapter.RequestDevice(&dd, wgpu::CallbackMode::WaitAnyOnly,
      [&device](wgpu::RequestDeviceStatus st, wgpu::Device d, wgpu::StringView) {
        if (st == wgpu::RequestDeviceStatus::Success) device = d;
      }), UINT64_MAX);
  if (!device) { FBLog::Error("cloudcheck", "no_device"); return 1; }
  wgpu::Queue queue = device.GetQueue();

  /* Three decks that exercise every branch of the function: an isotropic low deck with erosion, a mid
   * deck with a moderate stretch, and the cirrus case (7:1 stretch, strong warp, thin sheet). */
  FBCloudDeckParams decks[3];
  for (int i = 0; i < 3; i++) {
    decks[i].BaseM = i == 0 ? 1200.0f : (i == 1 ? 4200.0f : 9000.0f);
    decks[i].TopM = decks[i].BaseM + (i == 0 ? 900.0f : (i == 1 ? 1400.0f : 500.0f));
    decks[i].Cover = i == 0 ? 0.75f : (i == 1 ? 0.40f : 0.95f);
    decks[i].DriftEastM = 1234.5f * (float)(i + 1);
    decks[i].DriftNorthM = -876.25f * (float)(i + 1);
    decks[i].WindDirE = i == 2 ? 0.8823f : 0.6f;
    decks[i].WindDirN = i == 2 ? 0.4707f : -0.8f;
    decks[i].Stretch = kCloudStretch[i];
    decks[i].FeatureM = kCloudFeatureM[i];
    decks[i].Warp = kCloudWarp[i];
    decks[i].Erosion = kCloudErosion[i];
    decks[i].SigmaPerM = kCloudSigma[i];
    FBCloudCalibrate(decks[i]);
  }
  /* The two calibration constants are claimed as MEASURED, so measure them here: the FBM's own mean and
   * sigma, and — the number that matters — whether "cover" really is the area fraction it says it is. */
  { double sum = 0.0, sum2 = 0.0;
    const int kM = 40000;
    std::vector<float> field((size_t)kM);
    uint32_t r2 = 0x2468aceu;
    auto nx = [&r2](void) { r2 ^= r2 << 13; r2 ^= r2 >> 17; r2 ^= r2 << 5; return (float)(r2 >> 8) / 16777216.0f; };
    int above[3] = {0, 0, 0};
    for (int i = 0; i < kM; i++) {
      const float a = (nx() - 0.5f) * 400.0f, b = (nx() - 0.5f) * 400.0f;
      field[(size_t)i] = FBCloudFbmShear(a, b, 3u);
      sum += field[(size_t)i];
      sum2 += (double)field[(size_t)i] * field[(size_t)i];
      for (int k = 0; k < 3; k++) if (field[(size_t)i] > decks[k].RemapEdge) above[k]++;
    }
    const double mean = sum / kM, sigma = std::sqrt(sum2 / kM - mean * mean);
    FBLog::Info("cloudcheck", "FBM_DISTRIBUTION", {{"samples", kM}, {"mean", mean}, {"sigma", sigma},
        {"constMean", (double)kCloudFbmMean}, {"constSigma", (double)kCloudFbmSigma},
        {"cover0", (double)decks[0].Cover}, {"area0", (double)above[0] / kM},
        {"cover1", (double)decks[1].Cover}, {"area1", (double)above[1] / kM},
        {"cover2", (double)decks[2].Cover}, {"area2", (double)above[2] / kM}});
  }
  /* A deterministic spread over +-300 km and the full height fraction, including h exactly 0 and 1. */
  const int kN = 12288;
  std::vector<float> samples((size_t)kN * 4);
  std::vector<float> cpu((size_t)kN);
  uint32_t rng = 0x13579bdfu;
  auto next = [&rng](void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (float)(rng >> 8) / 16777216.0f; };
  for (int i = 0; i < kN; i++) {
    const int deck = i % 3;
    const float east = (next() - 0.5f) * 600000.0f;
    const float north = (next() - 0.5f) * 600000.0f;
    float h = next();
    if (i % 97 == 0) h = 0.0f;
    if (i % 101 == 0) h = 1.0f;
    samples[(size_t)i * 4 + 0] = east;
    samples[(size_t)i * 4 + 1] = north;
    samples[(size_t)i * 4 + 2] = h;
    samples[(size_t)i * 4 + 3] = (float)deck;
    cpu[(size_t)i] = FBCloudDensity(decks[deck], east, north, h);
  }

  const char *kCheckWGSL = R"(
@group(0) @binding(0) var<storage, read> gDecks : array<CloudDeck>;
@group(0) @binding(1) var<storage, read> gSamples : array<vec4f>;
@group(0) @binding(2) var<storage, read_write> gOut : array<f32>;
@compute @workgroup_size(64) fn main(@builtin(global_invocation_id) gid : vec3u) {
  let i = gid.x;
  if (i >= arrayLength(&gOut)) { return; }
  let s = gSamples[i];
  gOut[i] = cloudDensity(gDecks[u32(s.w)], s.x, s.y, s.z);
}
)";
  const std::string src = FlightBox::Render::FBCloudDensityConstsWGSL() + FlightBox::Render::kCloudDensityWGSL + kCheckWGSL;
  wgpu::ShaderSourceWGSL wgsl{};
  wgsl.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wgsl;
  wgpu::ComputePipelineDescriptor cpd{};
  cpd.compute.module = device.CreateShaderModule(&smd);
  wgpu::ComputePipeline pipe = device.CreateComputePipeline(&cpd);

  auto mkbuf = [&](uint64_t size, wgpu::BufferUsage usage) {
    wgpu::BufferDescriptor bd{};
    bd.size = size;
    bd.usage = usage;
    return device.CreateBuffer(&bd);
  };
  wgpu::Buffer deckBuf = mkbuf(sizeof decks, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);
  wgpu::Buffer sampBuf = mkbuf(samples.size() * sizeof(float), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);
  wgpu::Buffer outBuf = mkbuf((uint64_t)kN * sizeof(float), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);
  wgpu::Buffer readBuf = mkbuf((uint64_t)kN * sizeof(float), wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead);
  queue.WriteBuffer(deckBuf, 0, decks, sizeof decks);
  queue.WriteBuffer(sampBuf, 0, samples.data(), samples.size() * sizeof(float));

  wgpu::BindGroupEntry be[3] = {};
  be[0].binding = 0; be[0].buffer = deckBuf; be[0].size = sizeof decks;
  be[1].binding = 1; be[1].buffer = sampBuf; be[1].size = samples.size() * sizeof(float);
  be[2].binding = 2; be[2].buffer = outBuf; be[2].size = (uint64_t)kN * sizeof(float);
  wgpu::BindGroupDescriptor bgd{};
  bgd.layout = pipe.GetBindGroupLayout(0);
  bgd.entryCount = 3;
  bgd.entries = be;
  wgpu::BindGroup bind = device.CreateBindGroup(&bgd);

  wgpu::CommandEncoder enc = device.CreateCommandEncoder();
  { wgpu::ComputePassEncoder cp = enc.BeginComputePass();
    cp.SetPipeline(pipe);
    cp.SetBindGroup(0, bind);
    cp.DispatchWorkgroups((kN + 63) / 64);
    cp.End(); }
  enc.CopyBufferToBuffer(outBuf, 0, readBuf, 0, (uint64_t)kN * sizeof(float));
  wgpu::CommandBuffer cmd = enc.Finish();
  queue.Submit(1, &cmd);

  bool mapped = false;
  instance.WaitAny(readBuf.MapAsync(wgpu::MapMode::Read, 0, (uint64_t)kN * sizeof(float),
      wgpu::CallbackMode::WaitAnyOnly,
      [&mapped](wgpu::MapAsyncStatus st, wgpu::StringView) { mapped = (st == wgpu::MapAsyncStatus::Success); }),
      UINT64_MAX);
  if (!mapped) { FBLog::Error("cloudcheck", "readback_failed"); return 1; }
  const float *gpu = static_cast<const float *>(readBuf.GetConstMappedRange(0, (uint64_t)kN * sizeof(float)));

  double maxAbs = 0.0, sumAbs = 0.0;
  int worst = 0, nonZero = 0;
  for (int i = 0; i < kN; i++) {
    const double diff = std::fabs((double)gpu[i] - (double)cpu[(size_t)i]);
    if (cpu[(size_t)i] > 0.0f) nonZero++;
    sumAbs += diff;
    if (diff > maxAbs) { maxAbs = diff; worst = i; }
  }
  FBLog::Info("cloudcheck", "RESULT", {{"samples", kN}, {"nonZero", nonZero}, {"maxAbsDiff", maxAbs},
      {"meanAbsDiff", sumAbs / kN}, {"tolerance", kCloudCheckTolerance},
      {"worstCpu", (double)cpu[(size_t)worst]}, {"worstGpu", (double)gpu[worst]},
      {"verdict", maxAbs <= kCloudCheckTolerance ? "AGREE" : "DISAGREE"}});
  readBuf.Unmap();
  return maxAbs <= kCloudCheckTolerance ? 0 : 1;
}

/* The concrete FBMissionTickHook, implemented ONLY in this translation unit — which is what keeps
 * fb-gym's link GPU-free while both clients share one mission loop. */
class FBNativeMissionHook : public FlightBox::Missions::FBMissionTickHook {
public:
  FBNativeMissionHook(std::string base, std::string outDir, double intervalS, int width = 1280, int height = 720)
      : Base(std::move(base)), OutDir(std::move(outDir)), IntervalS(intervalS), Width(width), Height(height) {}

  /* The run's atmosphere, borrowed before the world exists — handed on in OnMissionStart below, where
   * FBWorld is created. Only the DATA side: nothing draws weather yet. */
  void OnWeather(const FlightBox::FBWeatherProvider &weather) override { Wx = &weather; }

  void OnMissionStart(const FlightBox::FBSpawn &spawn, const FlightBox::Units::FBActorList &actors,
                      const FlightBox::Units::FBUnitRegistry &units) override {
    const FlightBox::Units::FBSimUnit &primary = *actors.front();   /* the camera's actor (FBMissionRunner.h) */
    R = std::make_unique<FlightBox::Render::FBRenderer>();
    R->SetDefaultMode(0);
    R->SetGroundMode(0);
    R->SetStreaming(512);
    time_t clk = time(nullptr);
    R->SetSkyClock((double)clk);
    { uint8_t *moon = 0; int mw = 0, mh = 0;
      if (fb_load_image_file("flightbox/web/moon.jpg", &moon, &mw, &mh)) { R->SetMoonTexture(moon, mw, mh); free(moon); } }
    W = std::make_unique<FlightBox::World::FBWorld>();
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
    W->SetWeather(Wx);
    /* Warm the terrain cut before the first PNG; the jet is stationary, so an approximate cut does. */
    double altAsl0 = primary.GroundAslM() + (spawn.Ground ? 2.0 : (spawn.AltM - primary.GroundAslM()));
    double eye0[3], fwd0[3], right0[3], up0[3];
    FBGeoToEcef(spawn.LatDeg, spawn.LonDeg, altAsl0, eye0);
    FBCameraBasisEcef(spawn.HeadingDeg, -2.0, 0.0, spawn.LatDeg, spawn.LonDeg, fwd0, right0, up0);
    for (int i = 0; i < 60; i++) W->Update(spawn.LatDeg, spawn.LonDeg, eye0, fwd0, (double)i * 1000.0 / 15.0);
  }

  void OnTick(const FlightBox::Units::FBActorList &actors, double simT) override {
    if (!R || !W) return;   /* OnMissionStart already logged the failure */
    Acc += 0.1;   /* dt = the runner's fixed 10 Hz decision tick, see FBMissionRunner.cpp */
    if (Acc < IntervalS) return;
    Acc = 0.0;
    const FlightBox::Units::FBSimUnit &primary = *actors.front();
    FlightBox::Units::FBUnitPose p = primary.GetPose();   /* the camera rides the unit, not a raw FDM POD */
    double eye[3], fwd[3], right[3], up[3];
    FBGeoToEcef(p.LatDeg, p.LonDeg, p.ElevM, eye);
    FBCameraBasisEcef(p.YawDeg, p.PitchDeg, p.RollDeg, p.LatDeg, p.LonDeg, fwd, right, up);
    R->SetCameraBasis(eye, fwd, right, up);
    FlightBox::FBState hs = primary.HudState();   /* module telemetry + this tick's live pose */
    hs.Platform.Mode = FlightBox::FBMode::Manual;
    FlightBox::Render::SunPos(p.LatDeg, p.LonDeg, time(nullptr), &hs.Env.SunElDeg, &hs.Env.SunAzDeg);
    FlightBox::Render::MoonPos(p.LatDeg, p.LonDeg, time(nullptr), &hs.Env.MoonElDeg, &hs.Env.MoonAzDeg, &hs.Env.MoonPhase);
    /* The renderer never asks the weather anything: the CLIENT samples it where the camera is and hands
     * over the resulting decks (core/FBCloudDensity.h) — the same call an IR sensor would make. */
    if (const FlightBox::FBWeatherProvider *wx = W->Weather()) {
      const FlightBox::FBCloudSky sky = FlightBox::FBCloudSkyFromWeather(*wx, p.LatDeg, p.LonDeg, simT);
      R->SetCloudSky(sky);
      hs.Env.CloudLow = sky.Deck[0].Cover;
      hs.Env.CloudMid = sky.Deck[1].Cover;
      hs.Env.CloudHigh = sky.Deck[2].Cover;
      hs.Env.CloudCover = std::max(sky.Deck[0].Cover, std::max(sky.Deck[1].Cover, sky.Deck[2].Cover));
      hs.Env.CloudBaseAglM = sky.Deck[0].Cover > 0.0f ? sky.Deck[0].BaseM : 0.0f;
    }
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
  const FlightBox::FBWeatherProvider *Wx = nullptr;   /* borrowed for the run (OnWeather) */
  std::unique_ptr<FlightBox::Render::FBRenderer> R;
  std::unique_ptr<FlightBox::World::FBWorld> W;
  double Acc = 0.0;
  int Shot = 0;
};

/* No FBRenderer/FBWorld/GPU device at all unless `renderIntervalS > 0` — the renderer is a bolt-on
 * here, never a dependency of the physics or the termination logic. */
int RunMission(const std::string &missionPath, double timeoutOverride, double renderIntervalS,
              const std::string &base, const std::string &outDir) {
  FlightBox::World::FBTilesElevation elevation(base.c_str());
  if (renderIntervalS > 0.0) {
    FBNativeMissionHook hook(base, outDir, renderIntervalS);
    return FlightBox::Missions::FBRunMission(missionPath, timeoutOverride, outDir, FlightBox::Missions::FBNativeModelRoots(), elevation, &hook);
  }
  return FlightBox::Missions::FBRunMission(missionPath, timeoutOverride, outDir, FlightBox::Missions::FBNativeModelRoots(), elevation, nullptr);
}

}  // namespace

int main(int argc, char **argv) {
  double lat = 47.18, lon = 7.41, seconds = 3.0, interval = 1.0;
  double ground = 430.0, aglM = 1500.0, viewKm = 240.0, yawDeg = 0.0, pitchDeg = -3.0, cloudCover = 0.0, moonScale = 1.0, cloudQ = 1.0;
  int groundPhoto = 0, cloudCheck = 0;
  time_t utc = 0;   /* 0 = real wall clock */
  std::string base = "http://localhost:8081", outDir = ".", moonPath = "flightbox/web/moon.jpg";
  std::string missionPath, wxPath;
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
    else if (a == "--cloudcheck") cloudCheck = 1;
    else if (a == "--wx" && i + 1 < argc) wxPath = argv[++i];   /* screenshot mode: an FBWX blob = real decks */
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
  if (!FlightBox::Missions::FBEnsureDir(outDir)) { fprintf(stderr, "gpu_native: cannot create --out %s\n", outDir.c_str()); return 1; }

  /* FBRunMission installs its OWN sink; everything else here just wants console visibility. */
  static FlightBox::Clients::FBStdoutLogSink gStdoutSink;
  FlightBox::FBLog::SetSink(&gStdoutSink);
  FlightBox::FBLog::SetLevel(FlightBox::FBLogLevel::Debug);

  if (cloudCheck) return RunCloudDensityCheck();
  if (!missionPath.empty()) {
    /* Two weather sources cannot both be the mission's. The .fbm owns its own (missions/
     * FBWeatherBoot.h, and its precedence rule); --wx belongs to the mission-less screenshot venue. */
    if (!wxPath.empty()) {
      fprintf(stderr, "gpu_native: --wx is the screenshot venue's weather; a mission declares its own "
                      "with a `wx` line (doc/mission-format.md)\n");
      return 1;
    }
    return RunMission(missionPath, missionTimeout, intervalSet ? interval : 0.0, base, outDir);
  }

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
  FlightBox::Render::SunPos(lat, lon, clk, &hs.Env.SunElDeg, &hs.Env.SunAzDeg);
  FlightBox::Render::MoonPos(lat, lon, clk, &hs.Env.MoonElDeg, &hs.Env.MoonAzDeg, &hs.Env.MoonPhase);
  FlightBox::FBLog::Info("gpu", "ephemeris", {{"utc", (int)clk}, {"sunEl", (double)hs.Env.SunElDeg},
      {"sunAz", (double)hs.Env.SunAzDeg}, {"moonEl", (double)hs.Env.MoonElDeg}, {"moonAz", (double)hs.Env.MoonAzDeg},
      {"moonPhase", (double)hs.Env.MoonPhase}});

  FlightBox::Render::FBRenderer R;
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
  static FlightBox::Systems::FBDisplaySystem hudDisplay;   /* no live module here — the generic default HUD */
  R.SetHudDisplay(&hudDisplay);
  R.InitOffscreen(width, height);
  if (!R.Ready()) { FlightBox::FBLog::Error("gpu", "device_init_failed"); return 1; }

  FlightBox::World::FBWorld W;
  if (!W.Open(&R, base.c_str(), lat, lon, 32, viewKm * 1000.0, 512)) {
    FlightBox::FBLog::Error("gpu", "world_open_failed", {{"base", base}});
    return 1;
  }
  W.SetDefaultMode(groundPhoto);
  W.SetGroundMode(groundPhoto);
  W.SetNightLights(groundPhoto && hs.Env.SunElDeg < -3.0f);   /* EVS night -> stream /t/lights */
  FlightBox::FBLog::Info("gpu", "streaming_quadtree", {{"lat", lat}, {"lon", lon}, {"aglM", aglM},
      {"viewKm", viewKm}, {"albedo", groundPhoto ? "photo" : "osm"}, {"night", groundPhoto && hs.Env.SunElDeg < -3.0f}});

  /* The screenshot venue's OWN weather: a committed FBWX blob, so a cloud frame is as deterministic as
   * the pinned --utc that lights it. The mission path gets its weather from the .fbm instead. */
  std::unique_ptr<FlightBox::FBFixedWeather> shotWx;
  if (!wxPath.empty()) {
    shotWx = std::make_unique<FlightBox::FBFixedWeather>(wxPath);
    if (!shotWx->Ok()) { fprintf(stderr, "gpu_native: cannot read --wx %s\n", wxPath.c_str()); return 1; }
    const FlightBox::FBCloudLayers cl = shotWx->CloudLayers(lat, lon);
    FlightBox::FBLog::Info("gpu", "weather", {{"path", wxPath}, {"lowPct", cl.LowPct}, {"midPct", cl.MidPct},
        {"highPct", cl.HighPct}, {"ceilingM", cl.HaveCeiling ? cl.CeilingM : -1.0},
        {"visM", shotWx->VisibilityM(lat, lon)}});
  }

  const int totalFrames = (int)(seconds * fps + 0.5);
  const int everyFrames = interval > 0.0 ? (int)(interval * fps + 0.5) : 0;
  int shot = 0;
  for (int f = 0; f < totalFrames; f++) {
    /* Advection time is the FRAME's own time, not the wall clock: a screenshot is a deterministic
     * venue, and the drift has to be a small number (core/FBCloudDensity.h, kCloudDriftWrapM). */
    if (shotWx) R.SetCloudSky(FlightBox::FBCloudSkyFromWeather(*shotWx, lat, lon, (double)f / fps));
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
