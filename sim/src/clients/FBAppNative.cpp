/* The native frame ORACLE: FBRenderer's offscreen mode through the same pipeline the browser runs,
 * dumping PNGs. This is the verification venue a headless-browser SwiftShader cannot give — native
 * Dawn actually renders. doc/render/renderer.md, Abschnitt 1.1. */
#include "FBRenderer.h"
#include "FBMapCamera.h"
#include "FBTacticalMap.h"
#include "FBWorld.h"
#include "FBCamera.h"
#include "FBEphemeris.h"
#include "FBGeodesy.h"
#include "FBForcePicture.h"
#include "FBTacticalOrder.h"
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
#include "stages/FBAtmoCommon.h"
#include "stages/FBAtmoSample.h"
#include "stages/FBAtmoHaze.h"          /* ... and the shared air, checked the same way */
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
          "  --utc SECS the sky's instant (Unix seconds; 0/absent = the host wall clock). A mission\n"
          "    declares its own with a `time` line, so --utc together with one is rejected, not ranked.\n"
          "  --wx BLOB  the SCREENSHOT venue's weather (an FBWX blob). A mission carries its own in its\n"
          "    `wx` line (missions/FBWeatherBoot.h), so --wx with --mission is rejected, not ignored.\n"
          "  --vis KM / --cover FRAC  override the --wx sample's visibility / the LOW deck's cover for\n"
          "    ONE shot. Instruments of the screenshot venue only (a mission's weather is its own), and\n"
          "    the way \"the same camera under two atmospheres\" is measured at all.\n"
          "  --cloudcheck  evaluate core/FBCloudDensity.h and its WGSL twin over a sample set on the GPU\n"
          "    and print the largest disagreement; exit 0 iff it is inside the stated tolerance.\n"
          "  --mission FILE [--timeout N] [--interval S]  ground-spawn a .fbm mission (doc/missions/)\n"
          "  --map CALLSIGN [--map-span-km KM]  draw the TACTICAL MAP of that unit's fused picture\n"
          "    (the control node of its faction's net) instead of its cockpit -- an OSM raster sheet\n"
          "    off fb-tiles under APP-6 symbology. Only what that node has collected is drawn.\n"
          "  --map-zoom Z / --map-pan E_KM:N_KM / --map-home S  the zoom, the scroll and the re-centre a\n"
          "    browser gets from the wheel, the drag and the Home key. Z is an OSM zoom level (3..15)\n"
          "    and pins the span to one sheet texel per pixel; the pan is measured on the map's own\n"
          "    scale (so a long north leg lands slightly short in true ground metres -- Mercator) and\n"
          "    stops the map following the node; --map-home S puts it back on the node at second S.\n"
          "  --order AT:UNIT:KIND[:A[:B[:C]]]  hand a tactical order to a unit's AI at sim second AT.\n"
          "    KIND = waypoint|steer|attack (lat:lon[:altM]) | abort | emcon:0|1 | wcs:free|tight|hold\n"
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
    /* The band-limit is a BRANCH in the shared function, so the sample set has to cross it: full
     * detail, a partly filtered deck, and one collapsed onto the erosion's mean. */
    decks[i].ErodeFlat = i == 0 ? 0.0f : (i == 1 ? 0.55f : 1.0f);
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
    /* kCloudErodeMean is the value the band-limit fades the erosion TOWARD, so it has to be the
     * erosion FBM's own mean or the filter would darken/brighten what it filters. Measured, not
     * assumed, over the same sample count. */
    { double es = 0.0, es2 = 0.0;
      uint32_t r3 = 0x51ed270u;
      auto n3 = [&r3](void) { r3 ^= r3 << 13; r3 ^= r3 >> 17; r3 ^= r3 << 5; return (float)(r3 >> 8) / 16777216.0f; };
      for (int i = 0; i < kM; i++) {
        const float e = FBCloudErosionFbm((n3() - 0.5f) * 400.0f, (n3() - 0.5f) * 400.0f, n3() * kCloudErodeVert);
        es += e;
        es2 += (double)e * e;
      }
      const double em = es / kM;
      FBLog::Info("cloudcheck", "EROSION_DISTRIBUTION", {{"samples", kM}, {"mean", em},
          {"sigma", std::sqrt(es2 / kM - em * em)}, {"constMean", (double)kCloudErodeMean}});
    }
    /* THE CIRRUS AXIS, measured rather than eyeballed off a render: the spec asks the high deck's
     * fibres to lie along the real 250 hPa wind. Structure tensor of the coverage field over a plan
     * grid — the dominant orientation is the eigenvector of [[Jxx,Jxy],[Jxy,Jyy]], and the coherence
     * (l1-l2)/(l1+l2) says how much of an axis there is at all (1 = pure streaks, 0 = isotropic). */
    { const int kG = 256;
      const float kSpanM = 120000.0f, kStep = kSpanM / (float)kG;
      std::vector<float> f((size_t)kG * kG);
      /* The CONTROL: the same deck at stretch 1. "Coherence 0.94" only means something beside the
       * number the same field produces when nothing stretches it. */
      FBCloudDeckParams iso = decks[2];
      iso.Stretch = 1.0f;
      double isoCoh = 0.0;
      for (int pass = 0; pass < 2; pass++) {
      const FBCloudDeckParams &dk = pass == 0 ? decks[2] : iso;
      for (int y = 0; y < kG; y++)
        for (int x = 0; x < kG; x++)
          f[(size_t)y * kG + x] = FBCloudCoverage(dk, ((float)x - kG * 0.5f) * kStep,
                                                  ((float)y - kG * 0.5f) * kStep).Coverage;
      double jxx = 0, jyy = 0, jxy = 0;
      for (int y = 1; y < kG - 1; y++)
        for (int x = 1; x < kG - 1; x++) {
          const double gx = f[(size_t)y * kG + x + 1] - f[(size_t)y * kG + x - 1];
          const double gy = f[(size_t)(y + 1) * kG + x] - f[(size_t)(y - 1) * kG + x];
          jxx += gx * gx; jyy += gy * gy; jxy += gx * gy;
        }
      const double tr = jxx + jyy, det = jxx * jyy - jxy * jxy;
      const double disc = std::sqrt(std::max(tr * tr * 0.25 - det, 0.0));
      const double l1 = tr * 0.5 + disc, l2 = tr * 0.5 - disc;
      /* The gradient's dominant direction is ACROSS the streaks, so the streak axis is +90 deg. */
      const double gradDeg = std::atan2(2.0 * jxy, jxx - jyy) * 0.5 * 180.0 / kPi;
      double axisDeg = std::fmod(90.0 - gradDeg + 90.0 + 360.0, 180.0);   /* east/north -> compass, +90 */
      const double windDeg = std::fmod(std::atan2((double)decks[2].WindDirE, (double)decks[2].WindDirN)
                                       * 180.0 / kPi + 360.0, 180.0);
      double resid = std::fabs(axisDeg - windDeg);
      if (resid > 90.0) resid = 180.0 - resid;
      const double coh = tr > 0.0 ? (l1 - l2) / (l1 + l2) : 0.0;
      if (pass == 1) { isoCoh = coh; break; }
      FBLog::Info("cloudcheck", "CIRRUS_AXIS", {{"gridPts", kG * kG}, {"spanKm", kSpanM / 1000.0},
          {"stretch", (double)decks[2].Stretch}, {"fieldAxisDeg", axisDeg}, {"windAxisDeg", windDeg},
          {"residualDeg", resid}, {"coherence", coh}});
      }
      FBLog::Info("cloudcheck", "CIRRUS_AXIS_ISOTROPIC_CONTROL", {{"stretch", 1.0}, {"coherence", isoCoh}});
    }
    FBLog::Info("cloudcheck", "FBM_DISTRIBUTION", {{"samples", kM}, {"mean", mean}, {"sigma", sigma},
        {"constMean", (double)kCloudFbmMean}, {"constSigma", (double)kCloudFbmSigma},
        {"cover0", (double)decks[0].Cover}, {"area0", (double)above[0] / kM},
        {"cover1", (double)decks[1].Cover}, {"area1", (double)above[1] / kM},
        {"cover2", (double)decks[2].Cover}, {"area2", (double)above[2] / kM}});
  }
  /* A deterministic spread over +-300 km and the full height fraction, including h exactly 0 and 1.
   * The same sample index also carries an AIR sample: the shared haze/deck-light formula of
   * render/stages/FBAtmoHaze.h has two evaluators for exactly the reason the density does — the
   * terrain and the cloud deck must dissolve into one atmosphere — so it is measured the same way. */
  const int kN = 12288;
  std::vector<float> samples((size_t)kN * 4);
  std::vector<float> air((size_t)kN * 8);   /* per sample: (sigma0, zEff, dist, alt) + (base, top, tau, cover) */
  std::vector<float> cpu((size_t)kN), cpuAir((size_t)kN);
  uint32_t rng = 0x13579bdfu;
  auto next = [&rng](void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (float)(rng >> 8) / 16777216.0f; };
  /* Its OWN stream, so adding the air check did not move the density sample set by one draw and its
   * measured disagreement stays comparable to every number this file has recorded. */
  uint32_t rngAir = 0x7a1c9e05u;
  auto nextAir = [&rngAir](void) { rngAir ^= rngAir << 13; rngAir ^= rngAir >> 17; rngAir ^= rngAir << 5; return (float)(rngAir >> 8) / 16777216.0f; };
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

    /* 0.2 km .. 200 km visibility, altitudes that cross every deck (and go below sea level, where the
     * max(z,0) clamp lives), sight lines out to the streaming radius, and the sun anywhere above the
     * horizon incl. below the kMinSunUp floor. */
    const float sigma0 = FlightBox::Render::FBHazeSigma0(200.0f + nextAir() * 200000.0f);
    const float zEff = (nextAir() - 0.1f) * 14000.0f;
    const float dist = nextAir() * 250000.0f;
    const float alt = (nextAir() - 0.05f) * 12000.0f;
    const float tau = FlightBox::Render::FBDeckSunOpticalDepth(decks[deck], nextAir() * 1.2f - 0.1f);
    air[(size_t)i * 8 + 0] = sigma0;
    air[(size_t)i * 8 + 1] = zEff;
    air[(size_t)i * 8 + 2] = dist;
    air[(size_t)i * 8 + 3] = alt;
    air[(size_t)i * 8 + 4] = decks[deck].BaseM;
    air[(size_t)i * 8 + 5] = decks[deck].TopM;
    air[(size_t)i * 8 + 6] = tau;
    air[(size_t)i * 8 + 7] = decks[deck].Cover;
    cpuAir[(size_t)i] = FlightBox::Render::FBHazeTransmittance(sigma0, zEff, dist)
                      * FlightBox::Render::FBDeckSunTransmittance(tau, decks[deck].Cover,
                                                                  FlightBox::Render::FBDeckFrac(decks[deck], alt));
  }

  const char *kCheckWGSL = R"(
@group(0) @binding(0) var<storage, read> gDecks : array<CloudDeck>;
@group(0) @binding(1) var<storage, read> gSamples : array<vec4f>;
@group(0) @binding(2) var<storage, read_write> gOut : array<f32>;
@group(0) @binding(3) var<storage, read> gAir : array<vec4f>;
@compute @workgroup_size(64) fn main(@builtin(global_invocation_id) gid : vec3u) {
  let i = gid.x;
  if (i * 2u >= arrayLength(&gOut)) { return; }
  let s = gSamples[i];
  gOut[i * 2u] = cloudDensity(gDecks[u32(s.w)], s.x, s.y, s.z);
  let a = gAir[i * 2u];
  gOut[i * 2u + 1u] = hazeTransmittance(a.x, a.y, a.z) * deckSunThru(gAir[i * 2u + 1u], a.w);
}
)";
  const std::string src = std::string(FlightBox::Render::kAtmoCommon) + FlightBox::Render::kAtmoSample
                        + FlightBox::Render::FBHazeConstsWGSL() + FlightBox::Render::kHazeWGSL
                        + FlightBox::Render::FBCloudDensityConstsWGSL() + FlightBox::Render::kCloudDensityWGSL + kCheckWGSL;
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
  const uint64_t outBytes = (uint64_t)kN * 2 * sizeof(float);
  wgpu::Buffer deckBuf = mkbuf(sizeof decks, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);
  wgpu::Buffer sampBuf = mkbuf(samples.size() * sizeof(float), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);
  wgpu::Buffer airBuf = mkbuf(air.size() * sizeof(float), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);
  wgpu::Buffer outBuf = mkbuf(outBytes, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);
  wgpu::Buffer readBuf = mkbuf(outBytes, wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead);
  queue.WriteBuffer(deckBuf, 0, decks, sizeof decks);
  queue.WriteBuffer(sampBuf, 0, samples.data(), samples.size() * sizeof(float));
  queue.WriteBuffer(airBuf, 0, air.data(), air.size() * sizeof(float));

  wgpu::BindGroupEntry be[4] = {};
  be[0].binding = 0; be[0].buffer = deckBuf; be[0].size = sizeof decks;
  be[1].binding = 1; be[1].buffer = sampBuf; be[1].size = samples.size() * sizeof(float);
  be[2].binding = 2; be[2].buffer = outBuf; be[2].size = outBytes;
  be[3].binding = 3; be[3].buffer = airBuf; be[3].size = air.size() * sizeof(float);
  wgpu::BindGroupDescriptor bgd{};
  bgd.layout = pipe.GetBindGroupLayout(0);
  bgd.entryCount = 4;
  bgd.entries = be;
  wgpu::BindGroup bind = device.CreateBindGroup(&bgd);

  wgpu::CommandEncoder enc = device.CreateCommandEncoder();
  { wgpu::ComputePassEncoder cp = enc.BeginComputePass();
    cp.SetPipeline(pipe);
    cp.SetBindGroup(0, bind);
    cp.DispatchWorkgroups((kN + 63) / 64);
    cp.End(); }
  enc.CopyBufferToBuffer(outBuf, 0, readBuf, 0, outBytes);
  wgpu::CommandBuffer cmd = enc.Finish();
  queue.Submit(1, &cmd);

  bool mapped = false;
  instance.WaitAny(readBuf.MapAsync(wgpu::MapMode::Read, 0, outBytes, wgpu::CallbackMode::WaitAnyOnly,
      [&mapped](wgpu::MapAsyncStatus st, wgpu::StringView) { mapped = (st == wgpu::MapAsyncStatus::Success); }),
      UINT64_MAX);
  if (!mapped) { FBLog::Error("cloudcheck", "readback_failed"); return 1; }
  const float *gpu = static_cast<const float *>(readBuf.GetConstMappedRange(0, outBytes));

  double maxAbs = 0.0, sumAbs = 0.0, maxAir = 0.0, sumAir = 0.0;
  int worst = 0, worstAir = 0, nonZero = 0;
  for (int i = 0; i < kN; i++) {
    const double diff = std::fabs((double)gpu[i * 2] - (double)cpu[(size_t)i]);
    if (cpu[(size_t)i] > 0.0f) nonZero++;
    sumAbs += diff;
    if (diff > maxAbs) { maxAbs = diff; worst = i; }
    const double dAir = std::fabs((double)gpu[i * 2 + 1] - (double)cpuAir[(size_t)i]);
    sumAir += dAir;
    if (dAir > maxAir) { maxAir = dAir; worstAir = i; }
  }
  FBLog::Info("cloudcheck", "RESULT", {{"samples", kN}, {"nonZero", nonZero}, {"maxAbsDiff", maxAbs},
      {"meanAbsDiff", sumAbs / kN}, {"tolerance", kCloudCheckTolerance},
      {"worstCpu", (double)cpu[(size_t)worst]}, {"worstGpu", (double)gpu[worst * 2]},
      {"verdict", maxAbs <= kCloudCheckTolerance ? "AGREE" : "DISAGREE"}});
  FBLog::Info("cloudcheck", "AIR_RESULT", {{"samples", kN}, {"maxAbsDiff", maxAir},
      {"meanAbsDiff", sumAir / kN}, {"tolerance", kCloudCheckTolerance},
      {"worstCpu", (double)cpuAir[(size_t)worstAir]}, {"worstGpu", (double)gpu[worstAir * 2 + 1]},
      {"verdict", maxAir <= kCloudCheckTolerance ? "AGREE" : "DISAGREE"}});
  readBuf.Unmap();
  return (maxAbs <= kCloudCheckTolerance && maxAir <= kCloudCheckTolerance) ? 0 : 1;
}

/* THE MAP SHEET'S BYTES, and they come off the tile path that already exists: mode 0 is /bake/osm,
 * the same page the terrain streamer uploads as albedo, so the map's pages are cache hits rather than
 * a second client. render/ never sees this — the renderer is handed the hook. */
int MapTileFetch(int z, int x, int y, int ts, unsigned char *dst) {
  return fb_stream_pyramid(z, (uint32_t)x, (uint32_t)y, 0, ts, dst);
}

/* The concrete FBMissionTickHook, implemented ONLY in this translation unit — which is what keeps
 * fb-gym's link GPU-free while both clients share one mission loop. */
class FBNativeMissionHook : public FlightBox::Missions::FBMissionTickHook {
public:
  FBNativeMissionHook(std::string base, std::string outDir, double intervalS, bool groundPhoto = false,
                      int width = 1280, int height = 720)
      : Base(std::move(base)), OutDir(std::move(outDir)), IntervalS(intervalS), Width(width), Height(height),
        GroundPhoto(groundPhoto) {}

  /* THE TACTICAL MAP as a frame venue: `--map <callsign>` draws the picture of THAT unit's fusion --
   * the control node of the faction's net -- instead of its cockpit. The ground is an OSM raster sheet
   * on the Web-Mercator grid the symbols are projected onto. doc/player-layer.md §9. */
  void SetMapNode(const std::string &callsign, double spanM) {
    MapNode = callsign;
    if (spanM > 0.0) Cam.SetSpanM(spanM);
  }
  /* gpu_native has no window and therefore no keys: the ZOOM and PAN a browser gets from the wheel and
   * the drag arrive here as declared values, through the same FBMapCamera the keys drive. */
  void SetMapZoom(int zoom) { MapZoom = zoom; }
  void SetMapPan(double eastKm, double northKm) { PanEastKm = eastKm; PanNorthKm = northKm; }
  void SetMapHome(double atS) { HomeAtS = atS; }
  /* A SCRIPTED COMMANDER, so the order path has a reproducible venue: one order, at one sim second,
   * to one unit's AI. It writes no state -- it hands pilot/FBPilot an intention, exactly as a click on
   * the map will. */
  void AddOrder(double atS, const std::string &unit, const FBTacticalOrder &o) {
    Scripted.push_back({atS, unit, o, false});
  }

  /* A mission-declared clock replaces this client's own wall clock for the whole run; without one the
   * frames below keep the host clock they always had. */
  void OnClock(const FlightBox::Missions::FBMissionClock &clock) override { Clock = clock; }

  /* The run's atmosphere, borrowed before the world exists — handed on in OnMissionStart below, where
   * FBWorld is created. Only the DATA side: nothing draws weather yet. */
  void OnWeather(const FlightBox::FBWeatherProvider &weather) override { Wx = &weather; }

  void OnMissionStart(const FlightBox::FBSpawn &spawn, const FlightBox::Units::FBActorList &actors,
                      const FlightBox::Units::FBUnitRegistry &units) override {
    const FlightBox::Units::FBSimUnit &primary = *actors.front();   /* the camera's actor (FBMissionRunner.h) */
    R = std::make_unique<FlightBox::Render::FBRenderer>();
    /* --albedo photo is the EVS view, and the ONLY one with a real ephemeris: the OSM database view
     * pins daylight (render/FBFrameContext GroundPhoto), so no night frame exists without it. */
    R->SetDefaultMode(GroundPhoto);
    R->SetGroundMode(GroundPhoto);
    R->SetStreaming(512);
    R->SetSkyClock(UtcAt(0.0));
    { uint8_t *moon = 0; int mw = 0, mh = 0;
      if (fb_load_image_file("flightbox/web/moon.jpg", &moon, &mw, &mh)) { R->SetMoonTexture(moon, mw, mh); free(moon); } }
    W = std::make_unique<FlightBox::World::FBWorld>();
    if (!W->Open(R.get(), Base.c_str(), spawn.LatDeg, spawn.LonDeg, 32, 30000.0, 512)) {
      FlightBox::FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", "world open"}});
      R.reset(); W.reset();
      return;
    }
    R->SetHudDisplay(&primary.Displays());
    /* The airframe meshes, from the model root this client already runs against (sim/). */
    if (!R->AddUnitModel("f16", "assets/models"))
      FlightBox::FBLog::Warn("mission", "unit_model_missing", {{"type", "f16"}, {"dir", "assets/models"}});
    R->InitOffscreen(Width, Height);
    if (!R->Ready()) {
      FlightBox::FBLog::Error("mission", "RESULT", {{"result", "FAIL"}, {"reason", "gpu init"}});
      R.reset(); W.reset();
      return;
    }
    R->SetMapSheetSource(&MapTileFetch);
    /* Borrowed: the renderer's VIEW of the cast, never a second list of its own. */
    W->SetUnits(&units);
    W->SetEyeUnitId(primary.GetId());   /* the camera rides this one: drawing it would draw its inside */
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
    RunScriptedOrders(actors, simT);
    /* THE MAP'S SHEET FILLS EVERY TICK, not once per screenshot: the fetch budget is four pages per
     * pump, so a cut only touched on shot frames never converges and the map draws over bare paper.
     * The 3D terrain is NOT streamed here any more — the sheet is the ground now, and streaming the
     * quadtree as well only fought the same tile worker for the same /bake/osm pages. */
    bool mapping = !MapNode.empty();
    if (mapping) PumpMapSheet(actors, simT);
    Acc += 0.1;   /* dt = the runner's fixed 10 Hz decision tick, see FBMissionRunner.cpp */
    if (Acc < IntervalS) return;
    Acc = 0.0;
    if (mapping) { DrawMap(actors, simT); return; }
    const FlightBox::Units::FBSimUnit &primary = *actors.front();
    FlightBox::Units::FBUnitPose p = primary.GetPose();   /* the camera rides the unit, not a raw FDM POD */
    double eye[3], fwd[3], right[3], up[3];
    FBGeoToEcef(p.LatDeg, p.LonDeg, p.ElevM, eye);
    FBCameraBasisEcef(p.YawDeg, p.PitchDeg, p.RollDeg, p.LatDeg, p.LonDeg, fwd, right, up);
    R->SetCameraBasis(eye, fwd, right, up);
    FlightBox::FBState hs = primary.HudState();   /* module telemetry + this tick's live pose */
    hs.Platform.Mode = FlightBox::FBMode::Manual;
    const double utcNow = UtcAt(simT);
    FlightBox::FBSunPos(p.LatDeg, p.LonDeg, utcNow, &hs.Env.SunElDeg, &hs.Env.SunAzDeg);
    FlightBox::FBMoonPos(p.LatDeg, p.LonDeg, utcNow, &hs.Env.MoonElDeg, &hs.Env.MoonAzDeg, &hs.Env.MoonPhase);
    if (Clock.Have) R->SetSkyClock(utcNow);   /* a declared clock ADVANCES; the host clock does that itself */
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
  double UtcAt(double simT) const { return Clock.Have ? Clock.At(simT) : (double)time(nullptr); }

  struct ScriptedOrder {
    double AtS;
    std::string Unit;
    FlightBox::FBTacticalOrder Order;
    bool Fired;
  };

  void RunScriptedOrders(const FlightBox::Units::FBActorList &actors, double simT) {
    for (ScriptedOrder &so : Scripted) {
      if (so.Fired || simT < so.AtS) continue;
      so.Fired = true;
      for (const auto &u : actors) {
        if (!u || u->GetName() != so.Unit) continue;
        FlightBox::FBLogUnitScope us(u->GetName());
        so.Order.IssuedS = simT;
        u->Module().PilotSystem().ReceiveOrder(so.Order);
        break;
      }
    }
  }

  const FlightBox::Units::FBSimUnit *FindNode(const FlightBox::Units::FBActorList &actors) const {
    for (const auto &u : actors) if (u && u->GetName() == MapNode) return u.get();
    return nullptr;
  }

  /* THE VIEW, and it is the CAMERA's, not the node's: the node only says where "home" is. */
  FlightBox::Render::FBMapView MapView(const FlightBox::Units::FBUnitPose &np, double simT) {
    /* The boresight is at ViewH/2, not Height/2 — the scene is shifted up by the MFD bank's third. */
    Cam.SetFrame(Width, Height, 0.0f, R ? (float)R->ViewH() * 0.5f : (float)Height * 0.5f);
    Cam.Track(np.LatDeg, np.LonDeg);
    if (MapZoom > 0) { Cam.SetZoom(MapZoom); MapZoom = 0; }
    if (HomeAtS >= 0.0 && simT >= HomeAtS) { Cam.Recentre(); HomeAtS = -1.0; }
    if (PanEastKm != 0.0 || PanNorthKm != 0.0) {
      FlightBox::Render::FBMapView v = Cam.View();
      double pxPerM = (double)Width / v.SpanM;
      Cam.PanPixels(PanEastKm * 1000.0 * pxPerM, -PanNorthKm * 1000.0 * pxPerM);
      PanEastKm = PanNorthKm = 0.0;
    }
    return Cam.View();
  }

  void PumpMapSheet(const FlightBox::Units::FBActorList &actors, double simT) {
    const FlightBox::Units::FBSimUnit *node = FindNode(actors);
    if (!node || !R || !R->Ready()) return;
    FlightBox::Units::FBUnitPose np = node->GetPose();
    R->SetMapSheet(MapView(np, simT), true);
    R->PumpMapSheet();
  }

  /* ONE FRAME OF THE MAP. Everything drawn comes out of the node's own published blocks: the picture
   * is built by core/FBForcePicture from the node's FBState and its pose, and NOTHING else is read. */
  void DrawMap(const FlightBox::Units::FBActorList &actors, double simT) {
    const FlightBox::Units::FBSimUnit *node = FindNode(actors);
    if (!node) return;
    FlightBox::Units::FBUnitPose np = node->GetPose();
    Pic.Begin(simT, node->GetTeam());
    Pic.Ingest(node->HudState(), node->GetName().c_str(), np.LatDeg, np.LonDeg, np.ElevM,
               np.HeadingDeg, np.SpeedMs);

    /* THE HUD BEFORE THE VIEW: SetHud is what turns the HUD pass on, and the pass being on is what
     * shifts the scene up by the bank's third — so the boresight pixel is only known after it. */
    R->SetHud(node->HudState(), true);
    FlightBox::Render::FBMapView v = MapView(np, simT);
    R->SetMapSheet(v, true);
    R->PumpMapSheet();
    /* The scene pass still runs and the sheet covers all of it; the camera is set anyway so the sky
     * LUTs are evaluated somewhere real rather than at whatever basis the last cockpit frame left. */
    double eye[3], fwd[3], right[3], up[3];
    FBGeoToEcef(v.CentreLatDeg, v.CentreLonDeg, 12000.0, eye);
    FBCameraBasisEcef(0.0, kMapPitchDeg, 0.0, v.CentreLatDeg, v.CentreLonDeg, fwd, right, up);
    R->SetCameraBasis(eye, fwd, right, up);
    if (Clock.Have) R->SetSkyClock(UtcAt(simT));

    MapGeo.Reset();
    Map.SetGroundNote(FlightBox::Render::FBMapSheetNote(R->MapSheetState()));
    Map.Build(Pic, v, MapGeo);
    R->SetMapOverlay(&MapGeo);
    R->RenderFrame();
    R->SetMapOverlay(nullptr);
    R->SetMapSheet(v, false);
    std::vector<uint8_t> rgba;
    if (R->ReadPixels(rgba)) {
      char path[512];
      snprintf(path, sizeof path, "%s/map_%04d.png", OutDir.c_str(), Shot++);
      if (stbi_write_png(path, Width, Height, 4, rgba.data(), Width * 4))
        FlightBox::FBLog::Info("map", "frame",
            {{"path", path}, {"t", simT}, {"own", Pic.OwnCount()}, {"contacts", Pic.ContactCount()},
             {"bearings", Pic.BearingCount()}, {"node", MapNode}, {"spanKm", v.SpanM / 1000.0},
             {"zoom", R->MapSheetZoom()}, {"sheet", R->MapSheetResident()},
             {"sheetWanted", R->MapSheetWanted()}, {"centreLat", v.CentreLatDeg},
             {"centreLon", v.CentreLonDeg}, {"follow", Cam.Following()}});
      /* EVERY UNKNOWN SYMBOL, in text, so the picture the map drew is checkable without reading pixels
       * -- and so a commander's click has a reproducible coordinate on the scripted path. */
      for (int i = 0; i < Pic.Count(); i++) {
        const FlightBox::FBForceSymbol &sy = Pic.At(i);
        if (sy.Aff != FlightBox::FBAffiliation::Unknown || !sy.HavePoint) continue;
        FlightBox::FBLog::Info("map", "contact",
            {{"t", simT}, {"src", FlightBox::FBForceSourceStr(sy.Src)},
             {"aff", FlightBox::FBAffiliationStr(sy.Aff)}, {"lat", sy.LatDeg}, {"lon", sy.LonDeg},
             {"altM", (double)sy.AltM}, {"ageS", (double)sy.AgeS}});
      }
    }
  }

  std::string Base, OutDir;
  FlightBox::Missions::FBMissionClock Clock;
  double IntervalS;
  int Width, Height;
  bool GroundPhoto = false;
  const FlightBox::FBWeatherProvider *Wx = nullptr;   /* borrowed for the run (OnWeather) */
  std::unique_ptr<FlightBox::Render::FBRenderer> R;
  std::unique_ptr<FlightBox::World::FBWorld> W;
  double Acc = 0.0;
  int Shot = 0;
  std::string MapNode;
  FlightBox::Render::FBMapCamera Cam{FlightBox::Render::FBMapSheetStage::kTs};
  int MapZoom = 0;                          /* --map-zoom, applied on the first map view */
  double PanEastKm = 0.0, PanNorthKm = 0.0; /* --map-pan, likewise */
  double HomeAtS = -1.0;                    /* --map-home: the Home key, at a declared sim second */
  /* NOT -90. FBCameraBasisEcef builds its right vector from fwd x world-up, and at exactly straight
   * down those two are parallel: the cross product is zero, the basis collapses and the frame comes
   * back empty (measured: 22 leaves DRAWN and not one pixel of ground). A tenth of a degree off nadir
   * displaces the centre by H*tan(0.1 deg) = 21 m at H = 12 km, i.e. 0.6 px of a 1280 px frame. */
  static constexpr double kMapPitchDeg = -89.9;
  FlightBox::FBForcePicture Pic;
  FlightBox::Render::FBTacticalMap Map;
  FlightBox::Systems::FBHudGeometry MapGeo;
  std::vector<ScriptedOrder> Scripted;
};

/* `--order <atS>:<unit>:<kind>[:a[:b[:c]]]`. It exists so the ORDER PATH has a reproducible venue: a
 * commander who presses at a declared second is the same input a click on the map will be, and both
 * end in pilot/FBPilot's inbox. It writes no simulation state -- there is no path from here to one. */
bool ParseOrderSpec(const std::string &spec, double &atS, std::string &unit,
                    FlightBox::FBTacticalOrder &o) {
  std::vector<std::string> f;
  size_t p = 0;
  while (p <= spec.size()) {
    size_t c = spec.find(':', p);
    if (c == std::string::npos) { f.push_back(spec.substr(p)); break; }
    f.push_back(spec.substr(p, c - p));
    p = c + 1;
  }
  if (f.size() < 3) return false;
  atS = atof(f[0].c_str());
  unit = f[1];
  const std::string &k = f[2];
  if (k == "waypoint") o.Kind = FlightBox::FBOrderKind::Waypoint;
  else if (k == "steer") o.Kind = FlightBox::FBOrderKind::Steer;
  else if (k == "attack") o.Kind = FlightBox::FBOrderKind::Attack;
  else if (k == "abort") o.Kind = FlightBox::FBOrderKind::Abort;
  else if (k == "emcon") o.Kind = FlightBox::FBOrderKind::Emcon;
  else if (k == "wcs") o.Kind = FlightBox::FBOrderKind::WeaponsControl;
  else return false;
  if (o.Kind == FlightBox::FBOrderKind::Emcon) {
    o.Value = f.size() > 3 ? atof(f[3].c_str()) : 1.0;
  } else if (o.Kind == FlightBox::FBOrderKind::WeaponsControl) {
    FlightBox::FBWeaponsControl w = FlightBox::FBWeaponsControl::Free;
    if (f.size() < 4 || !FlightBox::FBWeaponsControlFromString(f[3].c_str(), w)) return false;
    o.Value = (double)(int)w;
  } else if (o.Kind != FlightBox::FBOrderKind::Abort) {
    if (f.size() < 5) return false;
    o.LatDeg = atof(f[3].c_str());
    o.LonDeg = atof(f[4].c_str());
    o.AltM = f.size() > 5 ? atof(f[5].c_str()) : 0.0;
  }
  return true;
}

/* No FBRenderer/FBWorld/GPU device at all unless `renderIntervalS > 0` — the renderer is a bolt-on
 * here, never a dependency of the physics or the termination logic. */
int RunMission(const std::string &missionPath, double timeoutOverride, double renderIntervalS,
              const std::string &base, const std::string &outDir, bool utcSet,
              const std::string &mapNode, double mapSpanM, int mapZoom, double mapPanE,
              double mapPanN, double mapHomeS, bool groundPhoto, const std::vector<std::string> &orderSpecs) {
  FlightBox::World::FBTilesElevation elevation(base.c_str());
  if (renderIntervalS > 0.0) {
    FBNativeMissionHook hook(base, outDir, renderIntervalS, groundPhoto);
    if (!mapNode.empty()) {
      hook.SetMapNode(mapNode, mapSpanM);
      hook.SetMapZoom(mapZoom);
      hook.SetMapPan(mapPanE, mapPanN);
      hook.SetMapHome(mapHomeS);
    }
    for (const std::string &spec : orderSpecs) {
      double atS = 0.0;
      std::string unit;
      FlightBox::FBTacticalOrder o;
      if (!ParseOrderSpec(spec, atS, unit, o)) {
        fprintf(stderr, "bad --order '%s'\n", spec.c_str());
        return 1;
      }
      static uint32_t seq = 0;
      o.Seq = ++seq;
      hook.AddOrder(atS, unit, o);
    }
    return FlightBox::Missions::FBRunMission(missionPath, timeoutOverride, outDir, FlightBox::Missions::FBNativeModelRoots(), elevation, &hook, 1, utcSet);
  }
  return FlightBox::Missions::FBRunMission(missionPath, timeoutOverride, outDir, FlightBox::Missions::FBNativeModelRoots(), elevation, nullptr, 1, utcSet);
}

}  // namespace

int main(int argc, char **argv) {
  double lat = 47.18, lon = 7.41, seconds = 3.0, interval = 1.0;
  double ground = 430.0, aglM = 1500.0, viewKm = 240.0, yawDeg = 0.0, pitchDeg = -3.0, cloudCover = 0.0, moonScale = 1.0, cloudQ = 1.0;
  double visOverrideKm = 0.0, coverOverride = -1.0;   /* <=0 / <0 = take the --wx sample as it is */
  int groundPhoto = 0, cloudCheck = 0;
  time_t utc = 0;   /* 0 = real wall clock */
  std::string base = "http://localhost:8081", outDir = ".", moonPath = "flightbox/web/moon.jpg";
  std::string missionPath, wxPath, mapNode;
  double missionTimeout = 0.0, mapSpanM = 0.0, mapPanE = 0.0, mapPanN = 0.0, mapHomeS = -1.0;
  int mapZoom = 0;
  std::vector<std::string> orderSpecs;
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
    else if (a == "--vis" && i + 1 < argc) visOverrideKm = atof(argv[++i]);
    else if (a == "--cover" && i + 1 < argc) coverOverride = atof(argv[++i]);
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
    else if (a == "--map" && i + 1 < argc) mapNode = argv[++i];          /* the net's control node */
    else if (a == "--map-span-km" && i + 1 < argc) mapSpanM = atof(argv[++i]) * 1000.0;
    else if (a == "--map-zoom" && i + 1 < argc) mapZoom = atoi(argv[++i]);
    else if (a == "--map-home" && i + 1 < argc) mapHomeS = atof(argv[++i]);
    else if (a == "--map-pan" && i + 1 < argc) {
      std::string s = argv[++i];
      size_t c = s.find(':');
      if (c == std::string::npos) { Usage(argv[0]); return 1; }
      mapPanE = atof(s.substr(0, c).c_str());
      mapPanN = atof(s.substr(c + 1).c_str());
    }
    else if (a == "--order" && i + 1 < argc) orderSpecs.push_back(argv[++i]);
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
                      "with a `wx` line (doc/missions/syntax.md)\n");
      return 1;
    }
    return RunMission(missionPath, missionTimeout, intervalSet ? interval : 0.0, base, outDir, utc != 0,
                      mapNode, mapSpanM, mapZoom, mapPanE, mapPanN, mapHomeS, groundPhoto, orderSpecs);
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
  FlightBox::FBSunPos(lat, lon, (double)clk, &hs.Env.SunElDeg, &hs.Env.SunAzDeg);
  FlightBox::FBMoonPos(lat, lon, (double)clk, &hs.Env.MoonElDeg, &hs.Env.MoonAzDeg, &hs.Env.MoonPhase);
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
    if (shotWx) {
      FlightBox::FBCloudSky sky = FlightBox::FBCloudSkyFromWeather(*shotWx, lat, lon, (double)f / fps);
      /* The two instruments: they leave the deck GEOMETRY (base, thickness, wind, character) exactly
       * where the fixture put it and move only the one number under test. */
      if (visOverrideKm > 0.0) sky.VisibilityM = (float)(visOverrideKm * 1000.0);
      if (coverOverride >= 0.0) {
        sky.Deck[0].Cover = (float)(coverOverride > 1.0 ? 1.0 : coverOverride);
        sky.Deck[1].Cover = 0.0f;
        sky.Deck[2].Cover = 0.0f;
        for (int d = 0; d < 3; d++) FlightBox::FBCloudCalibrate(sky.Deck[d]);
      }
      R.SetCloudSky(sky);
    }
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
