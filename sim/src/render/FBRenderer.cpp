#include "FBRenderer.h"
#include "stages/FBAtmoCommon.h"   /* kAtmoCommon: still needed here for the cloud shaders' splice */
#include "stages/FBAtmoSample.h"   /* kAtmoSample: same, clouds */
#include <cstdint>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace FlightBox {

static void Cross3(const double a[3], const double b[3], double o[3]);   /* defined below */
static void Norm3(double v[3]);

FBRenderer::FBRenderer()
  : SurfaceFormat(wgpu::TextureFormat::Undefined), HdrFormat(wgpu::TextureFormat::RGBA16Float),
    SwapW(0), SwapH(0),
    MoonW(0), MoonH(0), MoonScale(1.0), SkyClock(0),
    CloudW(0), CloudH(0), CloudQuality(1.0),
    HudState{}, HudEnabled(false), HudHave(false),
    Center{0, 0, 0}, GroundPhoto(false), HaveCamera(false), CameraFull(false), Eye{0, 0, 0}, LookTarget{0, 0, 0},
    Fwd{0, 0, 0}, Right{0, 0, 0}, Up{0, 0, 0}, Width(0), Height(0), DeviceReady(false),
    DeviceLost(false), Mode(Target::Surface), Blocking(false), Selector(nullptr), FrameNo(0) {}

void FBRenderer::SetHud(const FBState &s, bool have) {
  HudState = s;
  HudHave = have;
  HudEnabled = true;
}

void FBRenderer::SetAgl(float agl) { Hud->SetAgl(agl); }

void FBRenderer::SetAlbedoArray(const uint8_t *rgba, int ts, int layers) {
  Tiles->SetAlbedoArray(rgba, ts, layers);
}

void FBRenderer::SetCamera(const double eye[3], const double target[3]) {
  for (int a = 0; a < 3; a++) { Eye[a] = eye[a]; LookTarget[a] = target[a]; }
  HaveCamera = true;
}

void FBRenderer::SetCameraBasis(const double eye[3], const double fwd[3], const double right[3],
                                const double up[3]) {
  for (int a = 0; a < 3; a++) { Eye[a] = eye[a]; Fwd[a] = fwd[a]; Right[a] = right[a]; Up[a] = up[a]; }
  CameraFull = true;
}

void FBRenderer::SetTerrain(const float *verts, uint32_t nverts, int ntiles, const uint32_t *voff,
                            const uint32_t *vcnt, const double *origins, const double *center) {
  Tiles->SetStaticMesh(verts, nverts, ntiles, voff, vcnt, origins);
  for (int a = 0; a < 3; a++) Center[a] = center[a];
}

void FBRenderer::Init(const char *canvasSelector, int width, int height) {
  Mode = Target::Surface;
  Blocking = false;
  Selector = canvasSelector;
  Width = width;
  Height = height;
  wgpu::InstanceDescriptor id{};
  Instance = wgpu::CreateInstance(&id);
  StartAdapterRequest();
}

void FBRenderer::InitOffscreen(int width, int height) {
  Mode = Target::Offscreen;
  Blocking = true;
  Selector = nullptr;
  Width = width;
  Height = height;
  /* TimedWaitAny: native Dawn can drive Request{Adapter,Device} via Instance::WaitAny(future,
   * timeout) synchronously — no browser event loop here to pump AllowSpontaneous callbacks. */
  static constexpr auto kTimedWaitAny = wgpu::InstanceFeatureName::TimedWaitAny;
  wgpu::InstanceDescriptor id{};
  id.requiredFeatureCount = 1;
  id.requiredFeatures = &kTimedWaitAny;
  Instance = wgpu::CreateInstance(&id);
  StartAdapterRequest();
}

void FBRenderer::StartAdapterRequest(void) {
  wgpu::RequestAdapterOptions opts{};
  auto onAdapter = [this](wgpu::RequestAdapterStatus st, wgpu::Adapter a, wgpu::StringView msg) {
    if (st != wgpu::RequestAdapterStatus::Success) {
      printf("[FBRenderer] no WebGPU adapter (%.*s)\n", (int)msg.length, msg.data);
      return;
    }
    OnAdapter(a);
  };
  if (Blocking)
    Instance.WaitAny(Instance.RequestAdapter(&opts, wgpu::CallbackMode::WaitAnyOnly, onAdapter), UINT64_MAX);
  else
    Instance.RequestAdapter(&opts, wgpu::CallbackMode::AllowSpontaneous, onAdapter);
}

void FBRenderer::OnAdapter(wgpu::Adapter a) {
  Adapter = a;
  /* CPU-load diagnosis (branch performance): print WHAT WebGPU actually runs on. adapterType==CPU means
   * the ENTIRE pipeline is software rasterization (SwiftShader/lavapipe/WARP) — then the high CPU is the
   * browser, not our code, and the fix is browser-side (chrome://gpu, Firefox WebGPU/Vulkan flags). */
  { wgpu::AdapterInfo info{};
    a.GetInfo(&info);
    const bool soft = (info.adapterType == wgpu::AdapterType::CPU);
    const char *at = info.adapterType == wgpu::AdapterType::DiscreteGPU   ? "discrete-GPU"
                   : info.adapterType == wgpu::AdapterType::IntegratedGPU ? "integrated-GPU"
                   : info.adapterType == wgpu::AdapterType::CPU           ? "CPU-SOFTWARE"
                                                                          : "unknown";
    wgpu::Limits lim{};
    a.GetLimits(&lim);
    printf("[gpu] adapter: vendor='%.*s' arch='%.*s' device='%.*s' desc='%.*s'\n",
           (int)info.vendor.length, info.vendor.data, (int)info.architecture.length, info.architecture.data,
           (int)info.device.length, info.device.data, (int)info.description.length, info.description.data);
    printf("[gpu] adapter: type=%s backend=%d fallback=%d %s | limits: maxTexArrayLayers=%u maxBufferSize=%lluMB maxTexDim2D=%u\n",
           at, (int)info.backendType, soft ? 1 : 0,
           soft ? "<-- SOFTWARE RENDERING: 100% CPU, fix is browser-side (chrome://gpu / FF flags), not our code"
                : "(hardware)",
           (unsigned)lim.maxTextureArrayLayers, (unsigned long long)(lim.maxBufferSize >> 20), (unsigned)lim.maxTextureDimension2D);
    fflush(stdout);
  }
  /* HDR scene target = rgba16float: the volumetric cloud pass blends premultiplied-alpha over it, and
   * rg11b10ufloat has NO alpha channel + no guaranteed blend support (the earlier bandwidth pick broke
   * cloud compositing). 16F is the standard blendable HDR format; the extra 4 B/px at 720p is nothing. */
  bool rg11 = false;
  HdrFormat = wgpu::TextureFormat::RGBA16Float;
  wgpu::DeviceDescriptor dd{};
  std::vector<wgpu::FeatureName> feats;
  if (rg11) feats.push_back(wgpu::FeatureName::RG11B10UfloatRenderable);
  HasTimestamp = a.HasFeature(wgpu::FeatureName::TimestampQuery);   /* cloud-pass GPU timing (WASM iGPU number) */
  if (HasTimestamp) feats.push_back(wgpu::FeatureName::TimestampQuery);
  if (!feats.empty()) { dd.requiredFeatureCount = feats.size(); dd.requiredFeatures = feats.data(); }
  /* The multi-LOD albedo array grows past the default 256-layer cap; request the adapter's real max
   * (2048 on the target GPU) so EnsureAlbedoCap can grow that far. */
  wgpu::Limits adapterLimits{};
  a.GetLimits(&adapterLimits);
  MaxLayers = (int)adapterLimits.maxTextureArrayLayers;
  wgpu::Limits reqLimits{};
  reqLimits.maxTextureArrayLayers = adapterLimits.maxTextureArrayLayers;
  dd.requiredLimits = &reqLimits;
  dd.SetUncapturedErrorCallback([](const wgpu::Device &, wgpu::ErrorType t, wgpu::StringView m) {
    printf("[FBRenderer] GPU ERROR type=%d: %.*s\n", (int)t, (int)m.length, m.data);
  });
  dd.SetDeviceLostCallback(wgpu::CallbackMode::AllowSpontaneous,
      [this](const wgpu::Device &, wgpu::DeviceLostReason r, wgpu::StringView m) {
        DeviceLost = true;   /* guard GPU ops; the CPU streaming loop keeps running (counters live) */
        printf("[FBRenderer] DEVICE LOST reason=%d: %.*s\n", (int)r, (int)m.length, m.data);
      });
  auto onDevice = [this](wgpu::RequestDeviceStatus st, wgpu::Device d, wgpu::StringView msg) {
    if (st != wgpu::RequestDeviceStatus::Success) {
      printf("[FBRenderer] no WebGPU device (%.*s)\n", (int)msg.length, msg.data);
      return;
    }
    OnDevice(d);
  };
  if (Blocking)
    Instance.WaitAny(Adapter.RequestDevice(&dd, wgpu::CallbackMode::WaitAnyOnly, onDevice), UINT64_MAX);
  else
    Adapter.RequestDevice(&dd, wgpu::CallbackMode::AllowSpontaneous, onDevice);
}

void FBRenderer::OnDevice(wgpu::Device d) {
  Device = d;
  Queue = Device.GetQueue();
  if (Mode == Target::Surface) ConfigureSurface(); else CreateOffscreenTarget();
  CreateTileTexture();
  CreateAtmosphere();      /* before the terrain pipeline: terrain AP samples the transmittance LUT */
  FBGpu gpu{Device, Queue, HdrFormat, SurfaceFormat, Width, Height};
  Stars->Init(gpu);
  TileLights->Init(gpu);
  Units->Init(gpu);
  Sprites->Init(gpu);
  CreateTerrainPipeline();   /* creates DepthTex, which the cloud pass samples */
  { const char *e = getenv("FB_CLOUDS"); CloudsOn = e && atoi(e) != 0; }   /* default off — user judgment 2026-07-23 */
  if (CloudsOn) CreateClouds();   /* skip the noise-volume gen + cloud pipelines entirely when off (no boot/VRAM cost) */
  CreateTonemapPipeline();
  CreatePresent();          /* also Init()s Upscale (needs FrameTex, created here) */
  Hud->Init(gpu);
  DeviceReady = true;
  printf("[FBRenderer] WebGPU device ready, target %dx%d (%s), hdr=%s\n", Width, Height,
         Mode == Target::Surface ? "surface" : "offscreen",
         HdrFormat == wgpu::TextureFormat::RG11B10Ufloat ? "rg11b10ufloat" : "rgba16float");
}

/* Hillaire-2020 physically-based sky+atmosphere: three shader classes (FBTransmittanceStage,
 * FBSkyViewStage, FBSkyStage), one per shader — CreateAtmosphere below only creates the shared
 * resources (textures/sampler/uniform 3+ consumers read, incl. FBTilesStage terrain aerial
 * perspective) and wires the three stages via explicit dependency injection at Configure(). */
/* Terrain draw (kTerrainWGSL) + the albedo texture_2d_array + RenderBundle/DynTile streaming state
 * all live in FBTilesStage now (render/stages/FBTilesStage.*) — this method only creates the scene's
 * SHARED targets (DepthTex/HdrTex: FBSkyStage and FBTilesStage both draw into them, the cloud pass
 * samples DepthTex) and hands FBTilesStage its dependencies (the shared sampler, both atmosphere
 * LUTs, AtmoBuf) via explicit Configure() — same Init-order contract as CreateAtmosphere: this must
 * run AFTER it, since Tiles' bind group pins the LUT views CreateAtmosphere already created. */
void FBRenderer::CreateTerrainPipeline(void) {
  wgpu::TextureDescriptor td{};
  td.size = {(uint32_t)Width, (uint32_t)Height, 1};
  td.format = wgpu::TextureFormat::Depth32Float;
  td.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;   /* cloud pass reads it */
  DepthTex = Device.CreateTexture(&td);
  td.format = HdrFormat;
  td.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
  HdrTex = Device.CreateTexture(&td);

  FBGpu gpu{Device, Queue, HdrFormat, SurfaceFormat, Width, Height};
  Tiles->Configure(gpu, Samp, TransLUT.CreateView(), SkyLUT.CreateView(), AtmoBuf, MaxLayers);
}

void FBRenderer::CreateTileTexture(void) {
  wgpu::SamplerDescriptor sd{};
  sd.addressModeU = wgpu::AddressMode::ClampToEdge;   /* a bake is exactly one tile — no wrap/repeat */
  sd.addressModeV = wgpu::AddressMode::ClampToEdge;
  sd.magFilter = wgpu::FilterMode::Linear;
  sd.minFilter = wgpu::FilterMode::Linear;
  sd.mipmapFilter = wgpu::MipmapFilterMode::Linear;   /* trilinear across the new mip chain */
  sd.maxAnisotropy = 16;   /* target GPU allows it; the grazing-mip bias in the terrain fs handles the >16:1 tail */
  Samp = Device.CreateSampler(&sd);
}

int FBRenderer::UploadTile(const float *verts, uint32_t nverts, const double origin[3],
                           const uint8_t *albedo, int ts, int z) {
  if (!DeviceUsable()) return -1;
  return Tiles->UploadTile(verts, nverts, origin, albedo, ts, z);
}

int FBRenderer::UploadTilePhoto(int slot, const uint8_t *photo, int ts, int z) {
  if (!DeviceUsable()) return 0;
  return Tiles->UploadTilePhoto(slot, photo, ts, z, FrameNo);
}

void FBRenderer::SetGroundMode(int photo) {
  GroundPhoto = photo != 0;
  Hud->SetGroundMode(photo);   /* HUD SVS/EVS annunciator */
}


/* Fullscreen ACES-approx tonemap: reads the HDR scene target, encodes to the (sRGB) swapchain.
 * Lighting stays linear upstream; this is the only place display encoding happens. */
static const char *kTonemapWGSL = R"(
@group(0) @binding(0) var samp : sampler;
@group(0) @binding(1) var hdr : texture_2d<f32>;
@group(0) @binding(2) var cloud : texture_2d<f32>;   // quarter-res premultiplied cloud (upsampled)
struct VOut { @builtin(position) pos : vec4f, @location(0) uv : vec2f };
@vertex fn vs(@builtin(vertex_index) i : u32) -> VOut {
  var c = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0)); // fullscreen tri
  var o : VOut;
  let p = c[i];
  o.pos = vec4f(p, 0.0, 1.0);
  o.uv = vec2f((p.x + 1.0) * 0.5, (1.0 - p.y) * 0.5);
  return o;
}
fn aces(x : vec3f) -> vec3f {   // Narkowicz ACES fit
  let a = 2.51; let b = 0.03; let c = 2.43; let d = 0.59; let e = 0.14;
  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), vec3f(0.0), vec3f(1.0));
}
@fragment fn fs(in : VOut) -> @location(0) vec4f {
  var scene = textureSample(hdr, samp, in.uv).rgb;
  let cl = textureSample(cloud, samp, in.uv);          // bilinear upsample of the quarter-res cloud
  scene = scene * (1.0 - cl.a) + cl.rgb;               // premultiplied cloud over the HDR scene
  return vec4f(aces(scene), 1.0);   // linear [0,1]; the sRGB swapchain view encodes on write
}
)";

/* Cloud-off tonemap: the SAME ACES compress with no cloud composite (no cloud texture binding at all,
 * so the cloud path can be skipped whole — no stale-history sample). Used unless FB_CLOUDS=1. */
static const char *kTonemapPlainWGSL = R"(
@group(0) @binding(0) var samp : sampler;
@group(0) @binding(1) var hdr : texture_2d<f32>;
struct VOut { @builtin(position) pos : vec4f, @location(0) uv : vec2f };
@vertex fn vs(@builtin(vertex_index) i : u32) -> VOut {
  var c = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  var o : VOut;
  let p = c[i];
  o.pos = vec4f(p, 0.0, 1.0);
  o.uv = vec2f((p.x + 1.0) * 0.5, (1.0 - p.y) * 0.5);
  return o;
}
fn aces(x : vec3f) -> vec3f {
  let a = 2.51; let b = 0.03; let c = 2.43; let d = 0.59; let e = 0.14;
  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), vec3f(0.0), vec3f(1.0));
}
@fragment fn fs(in : VOut) -> @location(0) vec4f {
  let scene = textureSample(hdr, samp, in.uv).rgb;
  return vec4f(aces(scene), 1.0);
}
)";

void FBRenderer::CreateTonemapPipeline(void) {
  wgpu::ShaderSourceWGSL wgsl{};
  wgsl.code = kTonemapWGSL;
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wgsl;
  wgpu::ShaderModule sm = Device.CreateShaderModule(&smd);

  wgpu::ColorTargetState ct{};
  ct.format = SurfaceFormat;
  wgpu::RenderPipelineDescriptor rp{};
  rp.vertex.module = sm;   /* no vertex buffers — the triangle comes from vertex_index */
  wgpu::FragmentState fs{};
  fs.module = sm;
  fs.targetCount = 1;
  fs.targets = &ct;
  rp.fragment = &fs;       /* no depthStencil on the tonemap pass */

  /* Plain (cloud-off) tonemap always built — it is the default present path. */
  wgpu::ShaderSourceWGSL pwgsl{}; pwgsl.code = kTonemapPlainWGSL;
  wgpu::ShaderModuleDescriptor psmd{}; psmd.nextInChain = &pwgsl;
  wgpu::ShaderModule psm = Device.CreateShaderModule(&psmd);
  wgpu::RenderPipelineDescriptor prp{};
  prp.vertex.module = psm;
  wgpu::FragmentState pfs{}; pfs.module = psm; pfs.targetCount = 1; pfs.targets = &ct;
  prp.fragment = &pfs;
  TonemapPlainPipe = Device.CreateRenderPipeline(&prp);
  {
    wgpu::BindGroupEntry be[2] = {};
    be[0].binding = 0; be[0].sampler = Samp;
    be[1].binding = 1; be[1].textureView = HdrTex.CreateView();
    wgpu::BindGroupDescriptor bgd{};
    bgd.layout = TonemapPlainPipe.GetBindGroupLayout(0);
    bgd.entryCount = 2;
    bgd.entries = be;
    TonemapBindPlain = Device.CreateBindGroup(&bgd);
  }

  /* Cloud-compositing tonemap only when the cloud path is armed (CloudHist exists). */
  if (CloudsOn) {
    TonemapPipe = Device.CreateRenderPipeline(&rp);
    for (int k = 0; k < 2; k++) {   /* one per history: composites the TEMPORALLY-accumulated cloud */
      wgpu::BindGroupEntry be[3] = {};
      be[0].binding = 0; be[0].sampler = Samp;
      be[1].binding = 1; be[1].textureView = HdrTex.CreateView();
      be[2].binding = 2; be[2].textureView = CloudHist[k].CreateView();
      wgpu::BindGroupDescriptor bgd{};
      bgd.layout = TonemapPipe.GetBindGroupLayout(0);
      bgd.entryCount = 3;
      bgd.entries = be;
      TonemapBindH[k] = Device.CreateBindGroup(&bgd);
    }
  }
}

void FBRenderer::CreatePresent(void) {
  wgpu::TextureDescriptor td{};   /* fixed 720p; scene + tonemap + HUD all land here */
  td.size = {(uint32_t)Width, (uint32_t)Height, 1};
  td.format = SurfaceFormat;      /* sRGB: the round-trip through the upscale is identity */
  td.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding |
             wgpu::TextureUsage::CopySrc;
  FrameTex = Device.CreateTexture(&td);

  FBGpu gpu{Device, Queue, HdrFormat, SurfaceFormat, Width, Height};
  Upscale->Configure(gpu, FrameTex.CreateView());
}

void FBRenderer::CreateAtmosphere(void) {
  wgpu::SamplerDescriptor ss{};
  ss.addressModeU = wgpu::AddressMode::Repeat;        /* sky-view azimuth wraps */
  ss.addressModeV = wgpu::AddressMode::ClampToEdge;
  ss.addressModeW = wgpu::AddressMode::ClampToEdge;
  ss.magFilter = wgpu::FilterMode::Linear;
  ss.minFilter = wgpu::FilterMode::Linear;
  LutSamp = Device.CreateSampler(&ss);

  auto mklut = [&](uint32_t w, uint32_t h) {
    wgpu::TextureDescriptor td{};
    td.size = {w, h, 1};
    td.format = wgpu::TextureFormat::RGBA16Float;
    td.usage = wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::TextureBinding;
    return Device.CreateTexture(&td);
  };
  TransLUT = mklut(256, 64);
  SkyLUT = mklut(192, 108);

  wgpu::BufferDescriptor bd{};
  bd.size = 11 * 4 * sizeof(float);   /* 11 vec4 (camera/sun basis + moonDir + skyExtra) */
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  AtmoBuf = Device.CreateBuffer(&bd);

  {   /* NASA moon albedo (SetMoonTexture), or a 1x1 mid-grey fallback so the sky bind is always valid */
    int mw = MoonW > 0 ? MoonW : 1, mh = MoonH > 0 ? MoonH : 1;
    wgpu::TextureDescriptor td{};
    td.size = {(uint32_t)mw, (uint32_t)mh, 1};
    td.format = wgpu::TextureFormat::RGBA8UnormSrgb;
    td.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    MoonTex = Device.CreateTexture(&td);
    static const uint8_t grey[4] = {150, 150, 150, 255};
    const uint8_t *src = MoonData.size() >= (size_t)mw * mh * 4 ? MoonData.data() : grey;
    wgpu::TexelCopyTextureInfo dst{}; dst.texture = MoonTex;
    wgpu::TexelCopyBufferLayout lay{}; lay.bytesPerRow = (uint32_t)mw * 4; lay.rowsPerImage = (uint32_t)mh;
    wgpu::Extent3D ext{(uint32_t)mw, (uint32_t)mh, 1};
    Queue.WriteTexture(&dst, src, (size_t)mw * mh * 4, &lay, &ext);
  }

  /* Init-order CONTRACT: the three atmosphere stages are Configure()d here, in THIS order, because
   * each later one's bind group is built from an EARLIER one's already-created texture view (WebGPU
   * bind groups pin a specific view at creation — there is no "rebind later"). Transmittance owns
   * TransLUT; SkyView reads TransLUT (injected) and writes SkyLUT; Sky reads BOTH LUTs (injected) +
   * MoonTex + the shared AtmoBuf. FBTilesStage (created after this method returns — see OnDevice)
   * likewise receives TransLUT/SkyLUT views injected at ITS Configure(), for the terrain's aerial
   * perspective — this is the reason CreateAtmosphere runs before CreateTerrainPipeline. */
  FBGpu gpu{Device, Queue, HdrFormat, SurfaceFormat, Width, Height};
  Transmittance->Configure(gpu, TransLUT.CreateView());
  SkyView->Configure(gpu, SkyLUT.CreateView(), TransLUT.CreateView(), LutSamp, AtmoBuf);
  Sky->Configure(gpu, SkyLUT.CreateView(), LutSamp, TransLUT.CreateView(), AtmoBuf, MoonTex.CreateView());
}

void FBRenderer::SetMoonTexture(const uint8_t *rgba, int w, int h) {
  if (!rgba || w <= 0 || h <= 0) return;
  MoonW = w; MoonH = h;
  MoonData.assign(rgba, rgba + (size_t)w * h * 4);
}

void FBRenderer::SetStars(const uint8_t *hyg, int nbytes, double originLat, double originLon) {
  Stars->SetCatalogue(hyg, nbytes, originLat, originLon);
}

/* ============================================================================================
 * Volumetric clouds (Nubis / Guerrilla-class). A Perlin-Worley base + Worley detail 3D volume is
 * compute-generated ONCE at init; the sky pass then raymarches it through a WGS84 SPHERICAL SHELL
 * (base/top over the ground radius — so it follows the curvature at 240 km), depth-clipped by the
 * terrain, lit with dual-lobe Henyey-Greenstein + a Wrenninge multi-scatter approximation + Powder/
 * Beer + height-dependent sky ambient. EVS-only. ========================================== */
static const char *kCloudNoiseCommon = R"(
fn h33(p : vec3f) -> vec3f {
  let q = vec3f(dot(p, vec3f(127.1, 311.7, 74.7)), dot(p, vec3f(269.5, 183.3, 246.1)),
                dot(p, vec3f(113.5, 271.9, 124.6)));
  return fract(sin(q) * 43758.5453123);
}
fn worley2D(uv2 : vec2f, freq : f32) -> f32 {   /* tileable 2D cellular, 1 at cell centres (= 1 - F1) */
  let p = uv2 * freq;
  let id = floor(p);
  let fr = fract(p);
  var mind = 1.0;
  for (var j = -1; j <= 1; j++) { for (var i = -1; i <= 1; i++) {
    let o = vec2f(f32(i), f32(j));
    var c = id + o;
    c = c - floor(c / freq) * freq;   /* wrap -> seamless tiling */
    let fp = o + h33(vec3f(c, 0.0)).xy;
    let dv = fr - fp;
    mind = min(mind, dot(dv, dv));
  }}
  return 1.0 - sqrt(mind);
}
fn worley(uv : vec3f, freq : f32) -> f32 {   /* tileable cellular noise, 1 at cell centres */
  let p = uv * freq;
  let id = floor(p);
  let fr = fract(p);
  var mind = 1.0;
  for (var k = -1; k <= 1; k++) { for (var j = -1; j <= 1; j++) { for (var i = -1; i <= 1; i++) {
    let o = vec3f(f32(i), f32(j), f32(k));
    var c = id + o;
    c = c - floor(c / freq) * freq;   /* wrap -> seamless tiling */
    let fp = o + h33(c);
    let dv = fr - fp;
    mind = min(mind, dot(dv, dv));
  }}}
  return 1.0 - sqrt(mind);
}
fn worleyFbm(uv : vec3f, f : f32) -> f32 {
  return worley(uv, f) * 0.625 + worley(uv, f * 2.0) * 0.25 + worley(uv, f * 4.0) * 0.125;
}
fn gvec(c0 : vec3f, freq : f32) -> vec3f {
  let c = c0 - floor(c0 / freq) * freq;
  return normalize(h33(c) * 2.0 - 1.0);
}
fn perlin(uv : vec3f, freq : f32) -> f32 {
  let p = uv * freq;
  let id = floor(p);
  let fr = fract(p);
  let u = fr * fr * (3.0 - 2.0 * fr);
  var n = 0.0;
  for (var k = 0; k < 2; k++) { for (var j = 0; j < 2; j++) { for (var i = 0; i < 2; i++) {
    let o = vec3f(f32(i), f32(j), f32(k));
    let g = gvec(id + o, freq);
    let w = mix(1.0 - u.x, u.x, o.x) * mix(1.0 - u.y, u.y, o.y) * mix(1.0 - u.z, u.z, o.z);
    n += dot(g, fr - o) * w;
  }}}
  return n * 0.5 + 0.5;
}
fn perlinFbm(uv : vec3f, f : f32) -> f32 {
  return perlin(uv, f) * 0.55 + perlin(uv, f * 2.0) * 0.30 + perlin(uv, f * 4.0) * 0.15;
}
fn remap(v : f32, a : f32, b : f32, c : f32, d : f32) -> f32 { return c + (v - a) / (b - a) * (d - c); }
)";

static const char *kCloudBaseCS = R"(
@group(0) @binding(0) var outTex : texture_storage_3d<rgba8unorm, write>;
@compute @workgroup_size(4, 4, 4)
fn cs(@builtin(global_invocation_id) id : vec3u) {
  if (id.x >= 128u || id.y >= 128u || id.z >= 128u) { return; }
  let uv = (vec3f(id) + 0.5) / 128.0;
  /* Perlin-Worley DISPLACEMENT (Schneider 01 §3): inverted Worley forms the LOW END of a Perlin remap
   * -> keeps Perlin's connectedness (no planar facets) but gains Worley billow CONTRAST, so mid-coverage
   * forms solid cores with defined edges instead of a uniform thin sheet the detail shreds into slivers. */
  let pf = perlinFbm(uv, 3.0);
  let pw = 1.0 - worleyFbm(uv, 6.0);
  let raw = remap(pf, 0.0, 1.0, pw, 1.0);
  /* Contrast from a SMOOTH S-curve on the connected Perlin field (rounded billows, no angular Worley cell
   * ridges) — a light Worley touch adds billow without imprinting Voronoi edges. smoothstep gives the
   * dynamic range solid cores need while staying smooth (01 §3: keep Perlin's connectedness). */
  /* SOFT S-curve: a gentle contrast (not steep) so it doesn't band into corduroy/fold-lines. Opacity comes
   * from the extinction/density scale (optical depth), NOT from a sharp base transition -> decoupled. */
  let contrast = smoothstep(0.24, 0.64, pf);
  let base = clamp(mix(contrast, raw, 0.15), 0.0, 1.0);
  /* G = F1-ROUND 2D cell field (Candidate 12): (1-F1)^2 = round bumps at cell centres (NOT angular
   * F2-F1 Voronoi walls). Two octaves so cells vary in size (no uniform pyramids). Purely 2D -> columnar
   * (constant along the texture z) -> vertical towers. Drives horizontal discreteness + tower height. */
  let cellF = clamp(pow(worley2D(uv.xy, 9.0), 2.0) * 0.72 + pow(worley2D(uv.xy * 1.9 + 5.3, 5.0), 2.0) * 0.28, 0.0, 1.0);
  textureStore(outTex, vec3i(id), vec4f(base, cellF, 0.0, 0.0));
}
)";

static const char *kCloudDetailCS = R"(
@group(0) @binding(0) var outTex : texture_storage_3d<rgba8unorm, write>;
@compute @workgroup_size(4, 4, 4)
fn cs(@builtin(global_invocation_id) id : vec3u) {
  if (id.x >= 32u || id.y >= 32u || id.z >= 32u) { return; }
  let uv = (vec3f(id) + 0.5) / 32.0;
  /* 32^3 Nyquist is ~16 cells; worleyFbm internally reaches 4*f, so keep base freq <= 4 or the stored
   * detail is aliased HF garbage (radial streaks when it erodes the cloud). Fine scale comes from the
   * /0.28 km world-space sampling, not from over-cranking the generator frequency. */
  textureStore(outTex, vec3i(id), vec4f(worleyFbm(uv, 2.0), worleyFbm(uv, 3.0), worleyFbm(uv, 4.0), 1.0));
}
)";

/* 512² 2D cell field (B mode): tileable F1-round bumps pow(1-F1,2). ~57 texel/cell (vs ~14 in the old
 * 128³ G channel) -> ROUND puffs, not the angular 128³ tilted plates. Two integer-freq octaves so cells
 * vary in size (freq must be integer to wrap seamlessly). Sampled horizontally -> vertical columns. */
static const char *kCloudCellCS = R"(
@group(0) @binding(0) var outTex : texture_storage_2d<rgba8unorm, write>;
@compute @workgroup_size(8, 8)
fn cs(@builtin(global_invocation_id) id : vec3u) {
  if (id.x >= 512u || id.y >= 512u) { return; }
  let uv = (vec2f(f32(id.x), f32(id.y)) + 0.5) / 512.0;
  let cellF = clamp(pow(worley2D(uv, 7.0), 2.0) * 0.72 + pow(worley2D(uv * 1.9 + 5.3, 4.0), 2.0) * 0.28, 0.0, 1.0);
  textureStore(outTex, vec2i(i32(id.x), i32(id.y)), vec4f(cellF, 0.0, 0.0, 1.0));
}
)";

/* Raymarch. Prepend kAtmoCommon (Atmo struct, rayIntersectSphere, groundRadiusMM, PI) + kAtmoSample. */
static const char *kCloudWGSL = R"(
struct Cloud { p0 : vec4f, p1 : vec4f, p2 : vec4f, p3 : vec4f, p4 : vec4f, p5 : vec4f, p6 : vec4f, p7 : vec4f };   /* p0: rBaseMm,rTopMm(ABS),coverage,quality; p1: low,mid,high,time; p2: wind.xyz,densityScale; p3: depthScaleX,depthScaleY,groundR,jitterX; p4: extinction,sunIntensity,detailStrength,jitterY; p5: d2Freq,d2Weight,baseLODbias,coneR; p6: tangent1.xyz,cellSpanKm; p7: tangent2.xyz,- */
@group(0) @binding(0) var<uniform> A : Atmo;
@group(0) @binding(1) var lsamp : sampler;
@group(0) @binding(2) var svLUT : texture_2d<f32>;
@group(0) @binding(3) var tLUT : texture_2d<f32>;
@group(0) @binding(4) var nsamp : sampler;
@group(0) @binding(5) var baseTex : texture_3d<f32>;
@group(0) @binding(6) var detTex : texture_3d<f32>;
@group(0) @binding(7) var depthTex : texture_depth_2d;
@group(0) @binding(8) var<uniform> C : Cloud;
@group(0) @binding(9) var cellTex : texture_2d<f32>;   /* 512² tileable F1-round-cell bump field (B mode) */

/* Cloud-shape switch, baked at shader build from env FB_CLOUD_CELLS (CreateClouds): 0.0 = the deployed
 * Stand-A tower/threshold silhouette (default), 1.0 = the F1-round-cell (B) field. A module const so the
 * unused branch dead-strips — no per-pixel cost, no uniform slot (the Cloud struct is fully packed). */
const CELLS_ON : f32 = 0.0;
/* Moonlight strength, baked from env FB_MOONLIGHT (default 1.0). Scales the moon-as-second-source term
 * (see the march): moonlit clouds are faintly silver-grey and occlude stars; new moon (phase~0) stays
 * near-black. 0.0 disables it entirely (sun-only, the old behaviour). */
const MOONLIGHT : f32 = 1.0;

struct VOut { @builtin(position) pos : vec4f, @location(0) ndc : vec2f };
@vertex fn vs(@builtin(vertex_index) i : u32) -> VOut {
  var cc = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  var o : VOut;
  o.pos = vec4f(cc[i], 0.0, 1.0);
  o.ndc = cc[i];
  return o;
}
/* Tower height from a LOW-FREQ base sample (the large-scale mass sets how tall the tower is), decoupled
 * from the local/eroded density so the silhouette doesn't flutter at high frequency (team-lead guardrail /
 * Schneider dimensional-profile). low weather -> flat stratus ceiling, high -> taller cumulus towers. */
fn towerTop(bLow : f32, low : f32, high : f32) -> f32 {
  let topMax = mix(1.0 + 0.15 * high, 0.5, low);   /* cumulus tall (+high) vs stratus flat */
  return mix(0.35, topMax, pow(clamp(bLow, 0.0, 1.0), 1.5));
}
fn density(posMm : vec3f, h : f32, detLod : f32, footKm : f32) -> f32 {
  let posKm = posMm * 1000.0 + C.p2.xyz;   /* world-space km -> noise tiles fixed to the ground */
  /* Footprint-derived mip LOD: a grazing ray's pixel covers a large world span -> sample a COARSE mip so
   * the 128^3/32^3 field isn't read over Nyquist (kills the stable grazing moiré/crosshatch; 04 density-LOD). */
  let lodB0 = max(0.0, log2(max(footKm, 1.0e-5) * 128.0 / 9.0)) + C.p5.z;   /* p5.z = base LOD bias (corduroy test) */
  let lodB1 = max(0.0, log2(max(footKm, 1.0e-5) * 128.0 / 24.0)) + C.p5.z;
  let b0 = textureSampleLevel(baseTex, nsamp, posKm / 9.0, lodB0).r;    /* detail-scale mass */
  let b1 = textureSampleLevel(baseTex, nsamp, posKm / 24.0, lodB1).r;   /* mid-scale mass (intra-cell billow) */
  let b = b1 * 0.6 + b0 * 0.4;             /* horizontal base density [0,1] (contrast-stretched at gen) */
  let thresh = 1.0 - C.p0.z;               /* coverage threshold */
  var d : f32;
  var hNorm : f32;
  if (CELLS_ON > 0.5) {
    /* (B) F1-ROUND CELL FIELD (Candidate 12): discrete round convection cells with gaps = real
     * stratocumulus. ROUNDED DOME via HEIGHT SUBTRACTION (Schneider): cell strength drops with height so
     * the cloud narrows to the cell CENTRE going up. Coverage shifts the threshold: LOW/MID = discrete
     * cells with gaps, HIGH = cells merge into a closed smooth deck (undercast). */
    // Cell field from the dedicated 512² 2D texture, indexed by the HORIZONTAL (tangent-plane) world
    // position -> the cells extrude along LOCAL UP = vertical round columns. (The old 128³-G field was
    // worley2D constant along the noise z-axis, which maps to ECEF-Z / the polar axis -> ~43° tilt at
    // lat 47 = the tilted-PLATE artifact. 512² also gives ~57 texel/cell vs ~14 -> round, not angular.)
    let uvC = vec2f(dot(posKm, C.p6.xyz), dot(posKm, C.p7.xyz)) / C.p6.w;
    let cellF = textureSampleLevel(cellTex, nsamp, uvC, 0.0).r;   // [0,1] round hills, tileable
    let hDeck = clamp(h, 0.0, 1.0);
    // Per-column cloud-top height (deck fraction): taller where the cell field is stronger. Cross-section
    // shrinks with height (fewer cells reach up) AND the top ~45% tapers as a smooth DOMED cap (not a flat
    // linear cutoff = the boxy deckel). Wispy base at the deck floor. p7.w = height scale (FB_CELL_DOME).
    let colTop = (cellF - thresh) / max(C.p7.w, 0.01);
    if (colTop <= 0.0) { return 0.0; }     /* between cells -> clear sky */
    let cap = smoothstep(colTop, colTop * 0.55 - 0.05, hDeck);   /* rounded dome over the top ~45% */
    /* RAGGED base: a finer cell octave varies the base height per column, so the underside seen from
     * below is a bumpy surface, not a mirror-flat slab (the overhead-plate artifact). */
    let baseRough = textureSampleLevel(cellTex, nsamp, uvC * 3.3 + vec2f(1.7, 4.1), 0.0).r;
    let baseH = 0.04 + 0.20 * baseRough;
    d = cap * smoothstep(baseH, baseH + 0.10, hDeck) * mix(0.7, 1.0, b);
    if (d <= 0.001) { return 0.0; }
    hNorm = clamp((hDeck - baseH) / max(colTop - baseH, 0.05), 0.0, 1.0);
  } else {
    /* (A) DEPLOYED tower/threshold silhouette: coverage-remap the base, height-profile a per-column tower
     * (towerTop from the large-scale mass), wispy base + soft top auslauf. */
    if (b <= thresh) { return 0.0; }       /* below coverage -> clear sky */
    let cov = (b - thresh) / (1.0 - thresh);
    let topH = towerTop((b1 - thresh) / (1.0 - thresh), C.p1.x, C.p1.z);
    hNorm = h / max(topH, 0.05);
    if (hNorm >= 1.0) { return 0.0; }      /* above this column's tower top -> sky */
    let wispBase = smoothstep(0.0, 0.12, hNorm);   /* wispy bottom */
    let topFall = smoothstep(1.0, 0.72, hNorm);    /* soft auslauf in the upper quarter of the tower */
    d = cov * wispBase * topFall;
  }
  if (d <= 0.0) { return 0.0; }
  /* Top-biased detail erosion in hNorm (Nubis height dep): cauliflower heads high, smooth base low. */
  if (detLod > 0.01) {
    let d2f = C.p5.x; let d2w = C.p5.y;   /* fine-detail freq + weight (swept for cauliflower softness) */
    let lodD1 = max(0.0, log2(max(footKm, 1.0e-5) * 32.0 / 0.9));
    let lodD2 = max(0.0, log2(max(footKm, 1.0e-5) * 32.0 / d2f));
    let d1 = textureSampleLevel(detTex, nsamp, posKm / 0.9, lodD1);
    let d2 = textureSampleLevel(detTex, nsamp, posKm / d2f, lodD2);
    let dfbm = (d1.r * 0.55 + d1.g * 0.25 + d1.b * 0.2) * (1.0 - d2w) + (d2.r * 0.6 + d2.g * 0.4) * d2w;
    /* Cells (B): erode the BASE too (0.16..0.28) so the underside seen from below is ragged, not a hard
     * flat plate. Stand-A (A) keeps its clean top-biased base (0.02..0.26) unchanged. */
    let eBase = select(0.02, 0.16, CELLS_ON > 0.5);
    d = d - dfbm * mix(eBase, 0.28, hNorm) * max(C.p4.z, 0.01) * detLod;
  }
  return clamp(d, 0.0, 1.0);   /* dens [0,1] — powder/erosion see a real gradient */
}
fn hgPhase(c : f32, g : f32) -> f32 {
  let g2 = g * g;
  return (1.0 - g2) / (4.0 * PI * pow(1.0 + g2 - 2.0 * g * c, 1.5));
}
fn lightOD(posMm : vec3f, ldir : vec3f, jitter : f32) -> f32 {   /* optical depth toward a light: Schneider 6-cone (03 §6) */
  /* Two axes perpendicular to the light to spread the cone taps laterally — a straight 6-tap ray casts
   * hard banded self-shadows at grazing low light (finding 3); the cone spread smooths the banding. */
  var t0 = vec3f(0.0, 1.0, 0.0);
  if (abs(ldir.y) > 0.95) { t0 = vec3f(1.0, 0.0, 0.0); }
  let cx = normalize(cross(ldir, t0));
  let cy = cross(ldir, cx);
  var od = 0.0;
  var tt = 0.0;
  var stepMm = 0.00010;   /* 100 m first step, doubling */
  for (var i = 0; i < 5; i++) {   /* 5 near cone taps */
    let mid = tt + stepMm * 0.5;
    let ang = (f32(i) + jitter) * 2.3999632;   /* golden-angle rotation + per-pixel/-frame jitter -> temporal averages residual */
    let off = (cx * cos(ang) + cy * sin(ang)) * (mid * max(C.p5.w, 0.001));   /* cone half-angle ~17deg: radius grows with distance */
    let sp = posMm + ldir * mid + off;
    let h = clamp((length(sp) - C.p0.x) / (C.p0.y - C.p0.x), 0.0, 1.0);
    od += density(sp, h, 0.5, 0.03) * stepMm * 1.0e6;   /* light ray: short, keep fine mips */
    tt += stepMm;
    stepMm *= 2.0;
  }
  /* 1 distant shadow tap (Schneider): captures shadows cast by far clouds; base-only (cheap), half weight */
  let fp = posMm + ldir * (tt + 0.020);
  let fh = clamp((length(fp) - C.p0.x) / (C.p0.y - C.p0.x), 0.0, 1.0);
  od += density(fp, fh, 0.0, 0.1) * 0.020 * 1.0e6 * 0.5;
  return od;
}
@fragment fn fs(in : VOut) -> @location(0) vec4f {
  if (A.skyExtra.y < 0.5 || C.p0.z < 0.01) { return vec4f(0.0); }   /* SVS or clear -> nothing */
  let ndcJ = in.ndc + vec2f(C.p3.w, C.p4.w);   /* per-frame TAA screen jitter -> full-res reconstruction */
  let dir = normalize(A.camFwd.xyz + ndcJ.x * A.params.x * A.params.y * A.camRight.xyz
                                   + ndcJ.y * A.params.x * A.camUp.xyz);
  let cam = A.camPosMm.xyz;
  let rBase = C.p0.x;   /* absolute Mm radii — referenced to the REAL ground under the camera */
  let rTop = C.p0.y;
  let b = dot(cam, dir);
  let R2 = dot(cam, cam);
  let discTop = b * b - R2 + rTop * rTop;
  if (discTop <= 0.0) { return vec4f(0.0); }
  let sT = sqrt(discTop);
  var tStart = max(-b - sT, 0.0);
  var tEnd = -b + sT;
  if (tEnd <= 0.0) { return vec4f(0.0); }
  let discBase = b * b - R2 + rBase * rBase;
  if (discBase > 0.0) {
    let sB = sqrt(discBase);
    if (R2 < rBase * rBase) { tStart = max(tStart, -b + sB); }         /* below deck: start past the base */
    else if (R2 > rTop * rTop) { tEnd = min(tEnd, -b - sB); }          /* above deck (space): stop at base */
    else { if (-b - sB > 0.0) { tEnd = min(tEnd, -b - sB); } tStart = 0.0; }   /* inside the deck */
  }
  /* Terrain occlusion: reversed-Z infinite proj, depth = zn / (t * dot(dir,fwd)) -> t_hit. The march
   * is QUARTER-RES, so take the NEAREST full-res depth in this low-res pixel's 4x4 footprint
   * (conservative — a cloud never bleeds over a terrain silhouette). */
  let dpx = vec2i(i32(in.pos.x * C.p3.x), i32(in.pos.y * C.p3.y));
  var maxDepth = 0.0;
  for (var dy = 0; dy < 4; dy++) { for (var dx = 0; dx < 4; dx++) {
    maxDepth = max(maxDepth, textureLoad(depthTex, dpx + vec2i(dx, dy), 0));
  }}
  if (maxDepth > 1.0e-9) {   /* nearest terrain -> largest reversed-Z depth */
    let tTer = (0.05 / (maxDepth * max(dot(dir, A.camFwd.xyz), 1.0e-3))) / 1.0e6;
    tEnd = min(tEnd, tTer);
  }
  tEnd = min(tEnd, tStart + 0.24);   /* cap the march span at 240 km */
  if (tEnd <= tStart) { return vec4f(0.0); }

  let nSteps = max(24, i32(160.0 * clamp(C.p0.w, 0.05, 1.0)));
  let fine = min((tEnd - tStart) / f32(nSteps), 0.00012);   /* fine step, capped ~120 m: grazing/near-over-deck
                                                               rays step finely -> no big tangent slabs (Gap #3) */
  let coarse = fine * 3.0;                              /* empty-space skip step when out of cloud */
  let jit = fract(52.9829189 * fract(dot(in.pos.xy, vec2f(0.06711056, 0.00583715)) + C.p1.w));
  var t = tStart + fine * jit;

  let sun = A.sunDir.xyz;
  let up = normalize(cam);
  let cosT = dot(dir, sun);
  let phase = mix(hgPhase(cosT, 0.8), hgPhase(cosT, -0.5), 0.5);   /* dual-lobe HG: Hillaire 2016 measured fit */
  /* Sun colour reaching the deck: atmosphere transmittance toward the sun -> reddens near sunset. The
   * cloud march is in real WGS84 radii (~6.37 Mm); the Hillaire tLUT uses groundRadiusMM (6.360). REBASE
   * the camera to the LUT frame at its TRUE altitude, else a low sun lands the lookup below the horizon. */
  let camAltMm = length(cam) - C.p3.z;
  let camHill = up * (groundRadiusMM + max(camAltMm, 0.0));
  let sunCol = textureSampleLevel(tLUT, lsamp, tLUTuv(camHill, sun), 0.0).rgb;
  /* Ambient hemisphere: cool zenith sky above (brighter at cloud tops) + a warm bounce from the lit
   * horizon toward the sun (fills the shaded undersides with sunset colour). */
  let skyAmb = skyViewSample(svLUT, lsamp, A, up);
  let horizonCol = skyViewSample(svLUT, lsamp, A, normalize(sun - up * dot(sun, up)));

  var transm = 1.0;
  var scat = vec3f(0.0);
  var skipping = true;   /* two-tier: coarse-skip empty space, step BACK on entry, then fine-march the cloud */
  for (var i = 0; i < 384; i++) {   /* break on span end or transmittance saturation */
    if (t >= tEnd || transm < 0.02) { break; }
    let pos = cam + dir * t;
    let h = clamp((length(pos) - C.p0.x) / (C.p0.y - C.p0.x), 0.0, 1.0);
    let detLod = clamp(1.0 - (t * 1000.0 - 8.0) / 22.0, 0.0, 1.0);   /* fade detail 8->30 km (undersampled far) */
    let footKm = t * 1000.0 * (2.0 * A.params.x / 180.0);   /* low-res pixel world footprint (km) at this distance */
    let dens = density(pos, h, detLod, footKm);
    if (dens > 0.002) {
      if (skipping) { t = max(tStart, t - coarse); skipping = false; continue; }   /* back up to the boundary, re-march fine */
      let stepM = fine * 1.0e6;
      let sigK = C.p4.x * C.p2.w;   /* thickness lives in the extinction, NOT in dens */
      /* Sharpen the OPACITY on the POST-EROSION field: pow(d_eroded, gamma) puts the transmittance
       * transition on DETAIL wavelength (~100 m) with an IRREGULAR detail-shaped isosurface -> a crisp
       * optical surface (no along-ray smear) WITHOUT the smooth low-freq level-sets that caused corduroy. */
      /* Horizon distance-fade (B/high-cloud only — Stand-A keeps its full-range deck unchanged): clouds
       * beyond ~60 km dissolve into the haze by ~130 km (t = ray distance in Mm). Hides the quarter-res
       * mush of the far scattered field; matches reality (distant cumulus vanish in aerial perspective). */
      let distFade = select(1.0, 1.0 - smoothstep(0.060, 0.130, t), CELLS_ON > 0.5);
      let sigma = pow(clamp(dens, 0.0, 1.0), 2.5) * sigK * distFade;
      let od = lightOD(pos, sun, jit);
      /* Wrenninge multi-scatter octaves: attenuation a, phase-sharpness b, scatter c. */
      var ms = 0.0;
      var a = 1.0; var bo = 1.0; var co = 1.0;
      for (var o = 0; o < 3; o++) {
        let msPhase = mix(0.5 / (4.0 * PI), phase, bo);
        ms += co * msPhase * exp(-od * C.p4.x * C.p2.w * a);
        a *= 0.5; bo *= 0.5; co *= 0.55;
      }
      let powder = 1.0 - exp(-dens * 6.0);   /* dark-edge powder term */
      let sunLit = ms * sunCol * C.p4.y * (0.25 + 0.75 * powder);
      /* MOONLIGHT: the moon as a second source when it is UP — real moonlit clouds read faintly
       * silver-grey and occlude the stars. ~1/400 of the sun at full moon, phase-weighted (A.moonDir.w),
       * softened as the moon nears the horizon. Same cone-OD + multi-scatter chain, silver tint. */
      var moonLit = vec3f(0.0);
      let moonDir = A.moonDir.xyz;
      let moonUp = dot(moonDir, up);
      if (MOONLIGHT > 0.0 && moonUp > 0.02 && A.moonDir.w > 0.02) {
        let cosM = dot(dir, moonDir);
        let phaseM = mix(hgPhase(cosM, 0.8), hgPhase(cosM, -0.5), 0.5);
        let odM = lightOD(pos, moonDir, jit);
        var msM = 0.0; var am = 1.0; var bm = 1.0; var cm = 1.0;
        for (var o = 0; o < 3; o++) {
          let mp = mix(0.5 / (4.0 * PI), phaseM, bm);
          msM += cm * mp * exp(-odM * C.p4.x * C.p2.w * am);
          am *= 0.5; bm *= 0.5; cm *= 0.55;
        }
        let moonI = C.p4.y * A.moonDir.w * 0.10 * MOONLIGHT;   /* render-tuned: physically ~sun/400, but the
           night exposure is boosted (stars/lights visible), so ~sun/10 effective is where moonlit clouds
           actually READ as faint silver-grey. FB_MOONLIGHT tunes around 1.0; 0 = sun-only (near-black). */
        moonLit = msM * vec3f(0.72, 0.80, 1.0) * moonI * (0.25 + 0.75 * powder)
                  * smoothstep(0.0, 0.15, moonUp) * (1.0 - A.skyExtra.x);   /* night-only: fades out by day */
      }
      /* Nubis3 slide 32: ambient penetrates the surface, not the core -> fade by pow(1-profile, 0.5). But
       * with a FLOOR (0.30): a real day-cumulus base seen from below is mid-grey, not black — sky + ground
       * bounce reach even the dense underside. Without the floor the dense (sun-shadowed) base went near-
       * black = the "flatly dark from below" look. Slightly more sky fill at the base too (0.5 -> 0.62). */
      let ambGrad = 0.30 + 0.70 * pow(clamp(1.0 - dens, 0.0, 1.0), 0.5);
      let amb = (skyAmb * mix(0.62, 0.85, h) + horizonCol * 0.55 * (1.0 - h)) * 0.9 * ambGrad;
      let srcLum = sunLit + moonLit + amb;
      let tr = exp(-sigma * stepM);
      scat += transm * srcLum * (1.0 - tr);   /* energy-conserving in-scatter integration */
      transm *= tr;
      t += fine;
    } else {
      if (skipping) { t += coarse; } else { t += fine; }   /* coarse only while searching; fine once engaged (no re-overshoot) */
    }
  }
  return vec4f(max(scat, vec3f(0.0)), clamp(1.0 - transm, 0.0, 1.0));   /* premultiplied; guard NaN/neg */
}
)";

/* Temporal resolve: blend the fresh jittered quarter-res march into the reprojected history (camera
 * motion at the cloud mid-shell), with a neighbourhood clamp to suppress ghosting. Prepend kAtmoCommon
 * for the Atmo struct (camera basis + params for the ray). */
static const char *kCloudResolveWGSL = R"(
struct RU { prevVP : mat4x4f, camMove : vec4f, blend : vec4f };   /* camMove.xyz metres; blend: alpha, histValid, midR(Mm), - */
@group(0) @binding(0) var samp : sampler;
@group(0) @binding(1) var freshTex : texture_2d<f32>;
@group(0) @binding(2) var histTex : texture_2d<f32>;
@group(0) @binding(3) var<uniform> A : Atmo;
@group(0) @binding(4) var<uniform> RB : RU;
@group(0) @binding(5) var wsumTex : texture_2d<f32>;   /* accumulated splat weight (accum mode) */
struct ROut { @location(0) col : vec4f, @location(1) wsum : vec4f };
struct VOut { @builtin(position) pos : vec4f, @location(0) uv : vec2f, @location(1) ndc : vec2f };
@vertex fn vs(@builtin(vertex_index) i : u32) -> VOut {
  var cc = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  var o : VOut; let p = cc[i];
  o.pos = vec4f(p, 0.0, 1.0);
  o.uv = vec2f((p.x + 1.0) * 0.5, (1.0 - p.y) * 0.5);
  o.ndc = p;
  return o;
}
@fragment fn fs(in : VOut) -> ROut {
  let lowDim = vec2f(f32(textureDimensions(freshTex).x), f32(textureDimensions(freshTex).y));
  let fLow = in.uv * lowDim;
  var out : ROut;
  out.wsum = vec4f(1.0, 0.0, 0.0, 0.0);   /* live path: unused */

  /* ACCUM SPLAT proof (blend.y 2/3): place each cell's JITTERED sample with a tent kernel at its sub-cell
   * position and carry a running WEIGHTED average (weight sum in wsumTex). Over the 16 Halton phases each
   * full-res pixel accumulates the samples that landed on it -> SHARP full-res reconstruction, static. */
  if (RB.blend.y > 1.5) {
    let cell = floor(fLow) + 0.5;
    let pPos = cell + vec2f(RB.camMove.w, -RB.blend.w);         /* the cell's jittered sample position (low-res px) */
    let w = max(0.0, 1.0 - length(fLow - pPos) / 0.2);   /* narrow tent (~0.8 full-res px) for sharp placement */
    let sample = textureSampleLevel(freshTex, samp, cell / lowDim, 0.0);   /* nearest (no bilinear pre-blur) */
    var pw = 0.0; var pc = vec4f(0.0);
    if (RB.blend.y > 2.5) {   /* exact same pixel (static accum); textureLoad avoids the r32 filterable-sampler rule */
      let px = vec2i(i32(in.pos.x), i32(in.pos.y));
      pw = textureLoad(wsumTex, px, 0).r; pc = textureLoad(histTex, px, 0);
    }
    let nw = pw + w;
    out.col = select(pc, (pc * pw + sample * w) / max(nw, 1.0e-6), nw > 0.0);
    out.wsum = vec4f(nw, 0.0, 0.0, 0.0);
    return out;
  }

  /* LIVE path: same SPLAT placement as accum (nearest cell sample at its jittered sub-position, accepted by
   * confidence) but with an exponential blend instead of 1/N -> reconstructs full-res over frames AND adapts
   * to motion. Confidence modulates the blend alpha: heavy fresh where the jittered sample lands on F. */
  let cellL = floor(fLow) + 0.5;
  let pPosL = cellL + vec2f(RB.camMove.w, -RB.blend.w);
  let confL = max(0.0, 1.0 - length(fLow - pPosL) / 0.5);
  let cellUVL = cellL / lowDim;
  let fresh = textureSampleLevel(freshTex, samp, cellUVL, 0.0);   /* nearest (no bilinear pre-blur) */
  out.col = fresh;
  if (RB.blend.y < 0.5) { return out; }            /* first frame: no history */
  let dir = normalize(A.camFwd.xyz + in.ndc.x * A.params.x * A.params.y * A.camRight.xyz
                                   + in.ndc.y * A.params.x * A.camUp.xyz);
  let cam = A.camPosMm.xyz;
  let midR = RB.blend.z;
  let b = dot(cam, dir);
  let disc = b * b - (dot(cam, cam) - midR * midR);
  if (disc <= 0.0) { return out; }
  let midDist = -b + sqrt(disc);
  if (midDist <= 0.0) { return out; }
  let prevRelM = dir * midDist * 1.0e6 + RB.camMove.xyz;
  let clip = RB.prevVP * vec4f(prevRelM, 1.0);
  if (clip.w <= 0.0) { return out; }
  let puv = (clip.xy / clip.w) * vec2f(0.5, -0.5) + vec2f(0.5, 0.5);
  if (puv.x < 0.0 || puv.x > 1.0 || puv.y < 0.0 || puv.y > 1.0) { return out; }
  var hist = textureSampleLevel(histTex, samp, puv, 0.0);
  var mn = fresh; var mx = fresh;
  let texel = 1.0 / lowDim;
  for (var j = -1; j <= 1; j++) { for (var i = -1; i <= 1; i++) {
    let s = textureSampleLevel(freshTex, samp, cellUVL + vec2f(f32(i), f32(j)) * texel, 0.0);
    mn = min(mn, s); mx = max(mx, s);
  }}
  hist = clamp(hist, mn, mx);
  let motion = length(RB.camMove.xyz);
  let graze = 1.0 - abs(dot(normalize(cam), dir));
  /* confidence modulates the fresh weight: accept this cell's jittered sample mostly where it lands on F. */
  let a = clamp((RB.blend.x + motion * 0.02 + graze * 0.05) * (0.25 + 0.75 * confL), 0.0, 0.5);
  out.col = mix(hist, fresh, a);
  return out;
}
)";

/* F1-round-cell cloud shape (B) is the SHIPPED default (accepted 2026-07-23) — ON unless FB_CLOUD_CELLS
 * is explicitly 0 (which restores the old Stand-A tower silhouette). Read at init, drives both the
 * base-gen cell field and the CELLS_ON const in the march shader. */
static bool CloudCellsOn(void) { const char *e = getenv("FB_CLOUD_CELLS"); return !e || atoi(e) != 0; }

void FBRenderer::CreateClouds(void) {
  /* Storage-usage 3D textures don't sample trilinear on every backend (software Dawn falls back to
   * nearest -> flat facets). So GENERATE into a storage volume, then COPY into a sampled-only volume
   * that filters correctly. */
  auto mkStorage3d = [&](uint32_t n) {
    wgpu::TextureDescriptor td{};
    td.dimension = wgpu::TextureDimension::e3D;
    td.size = {n, n, n};
    td.format = wgpu::TextureFormat::RGBA8Unorm;
    td.usage = wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::CopySrc;
    return Device.CreateTexture(&td);
  };
  auto mipCount = [](uint32_t n) { uint32_t m = 1; while (n > 1) { n >>= 1; m++; } return m; };
  auto mkSampled3d = [&](uint32_t n) {
    wgpu::TextureDescriptor td{};
    td.dimension = wgpu::TextureDimension::e3D;
    td.size = {n, n, n};
    td.mipLevelCount = mipCount(n);   /* full mip chain: grazing rays sample coarse mips -> no over-Nyquist moiré */
    td.format = wgpu::TextureFormat::RGBA8Unorm;
    td.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    return Device.CreateTexture(&td);
  };
  CloudBaseTex = mkSampled3d(128);
  CloudDetailTex = mkSampled3d(32);

  auto mkmod = [&](const std::string &code) {
    wgpu::ShaderSourceWGSL w{};
    w.code = code.c_str();
    wgpu::ShaderModuleDescriptor smd{};
    smd.nextInChain = &w;
    return Device.CreateShaderModule(&smd);
  };

  static const char *kMipDownCS = R"(
@group(0) @binding(0) var src : texture_3d<f32>;
@group(0) @binding(1) var outTex : texture_storage_3d<rgba8unorm, write>;
@compute @workgroup_size(4, 4, 4)
fn cs(@builtin(global_invocation_id) id : vec3u) {
  let d = textureDimensions(outTex);
  if (id.x >= d.x || id.y >= d.y || id.z >= d.z) { return; }
  let c = vec3i(id) * 2;
  var s = vec4f(0.0);
  for (var k = 0; k < 2; k++) { for (var j = 0; j < 2; j++) { for (var i = 0; i < 2; i++) {
    s += textureLoad(src, c + vec3i(i, j, k), 0);   /* box-average 2x2x2 from the previous level */
  }}}
  textureStore(outTex, vec3i(id), s / 8.0);
}
)";
  wgpu::ComputePipeline downPipe;
  { wgpu::ComputePipelineDescriptor cp{}; cp.compute.module = mkmod(kMipDownCS); cp.compute.entryPoint = "cs";
    downPipe = Device.CreateComputePipeline(&cp); }

  /* Generate mip 0 (noise compute -> storage -> sampled mip 0) + box-downsample the whole mip chain. */
  auto genVolumeMips = [&](const char *cs, wgpu::Texture &dstSampled, uint32_t n) {
    wgpu::TextureViewDescriptor vAll{}; vAll.dimension = wgpu::TextureViewDimension::e3D;
    /* mip 0: noise compute -> storage -> copy to sampled mip 0 */
    { wgpu::Texture tex = mkStorage3d(n);
      wgpu::ComputePipelineDescriptor cp{}; cp.compute.module = mkmod(std::string(kCloudNoiseCommon) + cs); cp.compute.entryPoint = "cs";
      wgpu::ComputePipeline pipe = Device.CreateComputePipeline(&cp);
      wgpu::BindGroupEntry be{}; be.binding = 0; be.textureView = tex.CreateView(&vAll);
      wgpu::BindGroupDescriptor bg{}; bg.layout = pipe.GetBindGroupLayout(0); bg.entryCount = 1; bg.entries = &be;
      wgpu::CommandEncoder enc = Device.CreateCommandEncoder();
      wgpu::ComputePassEncoder pass = enc.BeginComputePass();
      pass.SetPipeline(pipe); pass.SetBindGroup(0, Device.CreateBindGroup(&bg)); pass.DispatchWorkgroups(n / 4, n / 4, n / 4); pass.End();
      wgpu::TexelCopyTextureInfo s{}, d{}; s.texture = tex; d.texture = dstSampled; wgpu::Extent3D ext{n, n, n};
      enc.CopyTextureToTexture(&s, &d, &ext);
      wgpu::CommandBuffer cmd = enc.Finish(); Queue.Submit(1, &cmd); }
    /* downsample each subsequent level from the sampled previous level */
    uint32_t lvl = 1, sz = n / 2;
    while (sz >= 1) {
      wgpu::Texture st = mkStorage3d(sz);
      wgpu::TextureViewDescriptor sv{}; sv.dimension = wgpu::TextureViewDimension::e3D; sv.baseMipLevel = lvl - 1; sv.mipLevelCount = 1;
      wgpu::BindGroupEntry be[2]{};
      be[0].binding = 0; be[0].textureView = dstSampled.CreateView(&sv);
      be[1].binding = 1; be[1].textureView = st.CreateView(&vAll);
      wgpu::BindGroupDescriptor bg{}; bg.layout = downPipe.GetBindGroupLayout(0); bg.entryCount = 2; bg.entries = be;
      wgpu::CommandEncoder enc = Device.CreateCommandEncoder();
      wgpu::ComputePassEncoder pass = enc.BeginComputePass();
      pass.SetPipeline(downPipe); pass.SetBindGroup(0, Device.CreateBindGroup(&bg));
      uint32_t g = (sz + 3) / 4; pass.DispatchWorkgroups(g, g, g); pass.End();
      wgpu::TexelCopyTextureInfo s{}, d{}; s.texture = st; d.texture = dstSampled; d.mipLevel = lvl; wgpu::Extent3D ext{sz, sz, sz};
      enc.CopyTextureToTexture(&s, &d, &ext);
      wgpu::CommandBuffer cmd = enc.Finish(); Queue.Submit(1, &cmd);
      lvl++; sz >>= 1;
    }
  };
  /* Base-construction sweep hooks (ridge diagnosis): toggle the Worley displacement / octaves via env. */
  std::string baseCS = kCloudBaseCS;
  auto rep = [&](const std::string &from, const std::string &to) {
    auto p = baseCS.find(from); if (p != std::string::npos) baseCS.replace(p, from.size(), to);
  };
  if (getenv("FB_BASE_PERLIN_ONLY")) rep("remap(pf, 0.0, 1.0, pw, 1.0)", "pf");
  if (const char *pf = getenv("FB_BASE_PFREQ")) rep("perlinFbm(uv, 3.0)", "perlinFbm(uv, " + std::string(pf) + ")");
  if (const char *ww = getenv("FB_BASE_WORLEY_W"))
    rep("1.0 - worleyFbm(uv, 6.0)", "mix(0.5, 1.0 - worleyFbm(uv, 6.0), " + std::string(ww) + ")");
  if (const char *wf = getenv("FB_BASE_WFREQ")) rep("worleyFbm(uv, 6.0)", "worleyFbm(uv, " + std::string(wf) + ")");
  if (const char *sh = getenv("FB_BASE_STRETCH_HI")) rep("(0.82 - 0.55)", "(" + std::string(sh) + " - 0.55)");
  if (const char *lo = getenv("FB_SS_LO")) rep("smoothstep(0.24,", "smoothstep(" + std::string(lo) + ",");
  if (const char *hi = getenv("FB_SS_HI")) rep(" 0.64, pf)", " " + std::string(hi) + ", pf)");
  /* F1-cell field (B) is OFF by default: skip its worley2D generation entirely (the Stand-A density path
   * reads only .r) — no boot cost. FB_CLOUD_CELLS=1 keeps it (paired with CELLS_ON=1.0 in the march). */
  if (!CloudCellsOn())
    rep("clamp(pow(worley2D(uv.xy, 9.0), 2.0) * 0.72 + pow(worley2D(uv.xy * 1.9 + 5.3, 5.0), 2.0) * 0.28, 0.0, 1.0)", "0.0");
  genVolumeMips(baseCS.c_str(), CloudBaseTex, 128);
  genVolumeMips(kCloudDetailCS, CloudDetailTex, 32);

  /* 512² 2D cell field (B mode), generated once: storage compute -> filterable sampled copy. */
  {
    const uint32_t CN = 512;
    wgpu::TextureDescriptor stD{};
    stD.size = {CN, CN, 1};
    stD.format = wgpu::TextureFormat::RGBA8Unorm;
    stD.usage = wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::CopySrc;
    wgpu::Texture stor = Device.CreateTexture(&stD);
    wgpu::TextureDescriptor smD{};
    smD.size = {CN, CN, 1};
    smD.format = wgpu::TextureFormat::RGBA8Unorm;
    smD.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    CloudCellTex = Device.CreateTexture(&smD);
    wgpu::ComputePipelineDescriptor cp{};
    cp.compute.module = mkmod(std::string(kCloudNoiseCommon) + kCloudCellCS);
    cp.compute.entryPoint = "cs";
    wgpu::ComputePipeline pipe = Device.CreateComputePipeline(&cp);
    wgpu::BindGroupEntry be{}; be.binding = 0; be.textureView = stor.CreateView();
    wgpu::BindGroupDescriptor bg{}; bg.layout = pipe.GetBindGroupLayout(0); bg.entryCount = 1; bg.entries = &be;
    wgpu::CommandEncoder enc = Device.CreateCommandEncoder();
    wgpu::ComputePassEncoder pass = enc.BeginComputePass();
    pass.SetPipeline(pipe); pass.SetBindGroup(0, Device.CreateBindGroup(&bg));
    pass.DispatchWorkgroups(CN / 8, CN / 8, 1); pass.End();
    wgpu::TexelCopyTextureInfo s{}, d{}; s.texture = stor; d.texture = CloudCellTex; wgpu::Extent3D ext{CN, CN, 1};
    enc.CopyTextureToTexture(&s, &d, &ext);
    wgpu::CommandBuffer cmd = enc.Finish(); Queue.Submit(1, &cmd);
  }

  /* Quarter-res march target: the whole cloud pass runs at Width/4 x Height/4 and is upsampled in the
   * tonemap composite. 16x fewer marched pixels — the core of the perf budget. */
  CloudW = Width / 4;
  CloudH = Height / 4;
  {
    wgpu::TextureDescriptor td{};
    td.size = {(uint32_t)CloudW, (uint32_t)CloudH, 1};
    td.format = wgpu::TextureFormat::RGBA16Float;
    td.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
    CloudLowTex = Device.CreateTexture(&td);
  }

  wgpu::SamplerDescriptor ns{};
  ns.addressModeU = wgpu::AddressMode::Repeat;
  ns.addressModeV = wgpu::AddressMode::Repeat;
  ns.addressModeW = wgpu::AddressMode::Repeat;
  ns.magFilter = wgpu::FilterMode::Linear;
  ns.minFilter = wgpu::FilterMode::Linear;
  CloudSamp = Device.CreateSampler(&ns);

  wgpu::BufferDescriptor bd{};
  bd.size = 8 * 4 * sizeof(float);   /* p0..p7 (added p6/p7 = the cell-field tangent basis) */
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  CloudUni = Device.CreateBuffer(&bd);

  /* Raymarch render pipeline: writes premultiplied cloud (rgb=scatter, a=1-transm) into the quarter-res
   * target; the tonemap composites it over the HDR scene. No blend (the target holds only the cloud).
   * kCloudNoiseCommon supplies remap() (the worley/perlin helpers go unused here — harmless). */
  std::string marchSrc = std::string(kAtmoCommon) + kAtmoSample + kCloudNoiseCommon + kCloudWGSL;
  if (CloudCellsOn()) {   /* flip the baked shape switch on (paired with the base-gen cellF above) */
    const std::string from = "const CELLS_ON : f32 = 0.0;";
    auto p = marchSrc.find(from);
    if (p != std::string::npos) marchSrc.replace(p, from.size(), "const CELLS_ON : f32 = 1.0;");
  }
  if (const char *ml = getenv("FB_MOONLIGHT")) {   /* moonlight strength (default 1.0) */
    const std::string from = "const MOONLIGHT : f32 = 1.0;";
    auto p = marchSrc.find(from);
    if (p != std::string::npos) marchSrc.replace(p, from.size(), "const MOONLIGHT : f32 = " + std::string(ml) + ";");
  }
  wgpu::ShaderModule m = mkmod(marchSrc);
  wgpu::ColorTargetState ct{};
  ct.format = wgpu::TextureFormat::RGBA16Float;
  wgpu::RenderPipelineDescriptor rp{};
  rp.vertex.module = m;
  wgpu::FragmentState fs{};
  fs.module = m;
  fs.targetCount = 1;
  fs.targets = &ct;
  rp.fragment = &fs;
  CloudPipe = Device.CreateRenderPipeline(&rp);

  wgpu::TextureViewDescriptor v3{};
  v3.dimension = wgpu::TextureViewDimension::e3D;
  wgpu::BindGroupEntry be[10] = {};
  be[0].binding = 0; be[0].buffer = AtmoBuf; be[0].size = 11 * 4 * sizeof(float);
  be[1].binding = 1; be[1].sampler = LutSamp;
  be[2].binding = 2; be[2].textureView = SkyLUT.CreateView();
  be[3].binding = 3; be[3].textureView = TransLUT.CreateView();
  be[4].binding = 4; be[4].sampler = CloudSamp;
  be[5].binding = 5; be[5].textureView = CloudBaseTex.CreateView(&v3);
  be[6].binding = 6; be[6].textureView = CloudDetailTex.CreateView(&v3);
  be[7].binding = 7; be[7].textureView = DepthTex.CreateView();
  be[8].binding = 8; be[8].buffer = CloudUni; be[8].size = 8 * 4 * sizeof(float);
  be[9].binding = 9; be[9].textureView = CloudCellTex.CreateView();
  wgpu::BindGroupDescriptor bg{};
  bg.layout = CloudPipe.GetBindGroupLayout(0);
  bg.entryCount = 10;
  bg.entries = be;
  CloudBind = Device.CreateBindGroup(&bg);

  /* --- Temporal UPSAMPLE: FULL-RES ping-pong history + resolve pass. The march stays quarter-res but
   * jitters its screen position per frame (Halton over the 4x4 phases); the resolve accumulates the
   * jittered low-res samples into the full-res history -> reconstructs full resolution over frames
   * (Nubis/Frostbite 05). Kills block facets, crosshatch, and low-sun speckle structurally. ----------- */
  for (int k = 0; k < 2; k++) {
    wgpu::TextureDescriptor td{};
    td.size = {(uint32_t)Width, (uint32_t)Height, 1};
    td.format = wgpu::TextureFormat::RGBA16Float;
    td.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
    CloudHist[k] = Device.CreateTexture(&td);
    td.format = wgpu::TextureFormat::R32Float;      /* accum-mode splat weight sum (MRT target 1) */
    CloudWSum[k] = Device.CreateTexture(&td);
  }
  { wgpu::BufferDescriptor rbd{};
    rbd.size = (16 + 4 + 4) * sizeof(float);   /* mat4 + camMove vec4 + blend vec4 */
    rbd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    ResolveUni = Device.CreateBuffer(&rbd); }
  {
    wgpu::ShaderModule rm = mkmod(std::string(kAtmoCommon) + kCloudResolveWGSL);
    wgpu::ColorTargetState rct[2]{};
    rct[0].format = wgpu::TextureFormat::RGBA16Float;   /* [0] = resolved cloud (normalized) */
    rct[1].format = wgpu::TextureFormat::R32Float;      /* [1] = accum splat weight sum */
    wgpu::RenderPipelineDescriptor rrp{};
    rrp.vertex.module = rm;
    wgpu::FragmentState rfs{};
    rfs.module = rm;
    rfs.targetCount = 2;
    rfs.targets = rct;
    rrp.fragment = &rfs;
    CloudResolvePipe = Device.CreateRenderPipeline(&rrp);
    for (int k = 0; k < 2; k++) {   /* [k] binds CloudHist[k] + CloudWSum[k] as the PREV history */
      wgpu::BindGroupEntry rbe[6] = {};
      rbe[0].binding = 0; rbe[0].sampler = Samp;
      rbe[1].binding = 1; rbe[1].textureView = CloudLowTex.CreateView();
      rbe[2].binding = 2; rbe[2].textureView = CloudHist[k].CreateView();
      rbe[3].binding = 3; rbe[3].buffer = AtmoBuf; rbe[3].size = 11 * 4 * sizeof(float);
      rbe[4].binding = 4; rbe[4].buffer = ResolveUni; rbe[4].size = (16 + 4 + 4) * sizeof(float);
      rbe[5].binding = 5; rbe[5].textureView = CloudWSum[k].CreateView();
      wgpu::BindGroupDescriptor rbg{};
      rbg.layout = CloudResolvePipe.GetBindGroupLayout(0);
      rbg.entryCount = 6;
      rbg.entries = rbe;
      CloudResolveBind[k] = Device.CreateBindGroup(&rbg);
    }
  }

  if (HasTimestamp) {   /* 2 timestamps bracket the cloud march + resolve; resolved into a readback buffer */
    wgpu::QuerySetDescriptor qd{};
    qd.type = wgpu::QueryType::Timestamp;
    qd.count = 2;
    TsQuery = Device.CreateQuerySet(&qd);
    wgpu::BufferDescriptor bd{};
    bd.size = 2 * sizeof(uint64_t);
    bd.usage = wgpu::BufferUsage::QueryResolve | wgpu::BufferUsage::CopySrc;
    TsResolveBuf = Device.CreateBuffer(&bd);
    bd.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
    TsReadBuf = Device.CreateBuffer(&bd);
  }
}

void FBRenderer::UpdateClouds(const double eye[3], const double sunDir[3], const double up[3], double nowSec) {
  (void)sunDir;
  /* Cloud deck from the weather (HudState): a main deck whose base/thickness follow cloud_base + the
   * low/mid/high mix; total coverage drives the fill. EVS-only (the pass self-gates on skyExtra.y). */
  float cover = HudState.cloud;
  if (HudState.cloud_low > cover) cover = HudState.cloud_low;
  if (HudState.cloud_mid > cover) cover = HudState.cloud_mid;
  if (HudState.cloud_high > cover) cover = HudState.cloud_high;
  if (cover <= 0.0f) cover = GroundPhoto ? 0.4f : 0.0f;   /* a pleasant default deck when no weather */
  /* DEFAULT WEATHER = a HIGH broken cell layer (base 8 km): the ~2 km loiter always sees it in the
   * mid/far field (accepted 2026-07-23). open-meteo cloud_base overrides once wired; FB_CLOUD_BASE_M
   * sweeps it. Paired with cells-on + FB_CELL_KM 40 / FB_CELL_DOME 0.5 + moonlight (the shipped look). */
  double baseAGL = HudState.cloud_base > 0.0f ? HudState.cloud_base : 8000.0;
  if (const char *cb = getenv("FB_CLOUD_BASE_M")) baseAGL = atof(cb);
  double thick = 2600.0 + 1400.0 * HudState.cloud_high;
  if (const char *ct = getenv("FB_CLOUD_THICK_M")) thick = atof(ct);
  double topAGL = baseAGL + thick;
  /* Tunable material params (default; the cloud lab / research numbers override via SetCloudLab). */
  float density = 18.0f, extinct = 0.06f, sunI = 18.0f, detail = 1.3f;
  if (CloudLab) { cover = LabCover; density = LabDensity; extinct = LabExtinct; sunI = LabSunI; detail = LabDetail; }
  /* Shell radii are ABSOLUTE, referenced to the REAL ground under the camera (WGS84, not the Hillaire
   * simplified 6360 km): ground radius = |eye| - camera MSL altitude; base/top add the AGL heights. */
  double eyeLen = std::sqrt(eye[0] * eye[0] + eye[1] * eye[1] + eye[2] * eye[2]);
  double groundR = eyeLen - (double)HudState.alt;
  float p[32];
  p[0] = (float)((groundR + baseAGL) / 1.0e6);   /* rBase, absolute Mm */
  p[1] = (float)((groundR + topAGL) / 1.0e6);     /* rTop */
  p[2] = cover;
  p[3] = (float)CloudQuality;
  p[4] = HudState.cloud_low; p[5] = HudState.cloud_mid; p[6] = HudState.cloud_high;
  p[7] = (float)std::fmod((double)FrameNo * 0.6180339887, 1.0);   /* per-frame along-ray dither (anti-banding + live AA) */
  double w = nowSec * 8.0;   /* slow wind drift (km) */
  p[8] = (float)(w * 0.001); p[9] = 0.0f; p[10] = (float)(w * 0.0006);
  p[11] = density;   /* density scale -> optically thick, solid cumulus */
  p[12] = CloudW > 0 ? (float)Width / (float)CloudW : 4.0f;    /* low-res -> full-res depth map */
  p[13] = CloudH > 0 ? (float)Height / (float)CloudH : 4.0f;
  p[14] = (float)(groundR / 1.0e6);   /* real ground radius Mm — rebase cloud pos into the Hillaire LUT frame */
  /* Per-frame screen jitter (NDC): EXACT 4x4 sub-grid so each full-res sub-pixel gets a direct sample
   * once per 16 frames -> the resolve splat reconstructs full resolution from the quarter-res march. */
  { uint32_t ph = FrameNo % 16u;
    float jx = ((float)(ph % 4u) + 0.5f) / 4.0f - 0.5f, jy = ((float)(ph / 4u) + 0.5f) / 4.0f - 0.5f;
    p[15] = CloudW > 0 ? jx * 2.0f / (float)CloudW : 0.0f;
    p[16] = extinct; p[17] = sunI; p[18] = detail;
    p[19] = CloudH > 0 ? jy * 2.0f / (float)CloudH : 0.0f; }
  /* p5: fine-detail freq + weight (default 0.28 km / 0.35; env-overridable for the cauliflower sweep). */
  { const char *ef = getenv("FB_D2_FREQ"), *ew = getenv("FB_D2_WEIGHT");
    p[20] = ef ? (float)atof(ef) : 0.45f; p[21] = ew ? (float)atof(ew) : 0.20f;
    const char *bb = getenv("FB_BASE_LOD_BIAS"); p[22] = bb ? (float)atof(bb) : 0.0f; const char *cr = getenv("FB_CONE_R"); p[23] = cr ? (float)atof(cr) : 0.30f; }   /* swept: soft cauliflower */
  /* p6/p7: horizontal tangent basis (from the camera up) + cell span (km/tile) — the 512² F1 cell field
   * is sampled by (dot(pos,t1), dot(pos,t2))/span so cells extrude along LOCAL UP = vertical round puffs. */
  double t1[3] = {up[1], -up[0], 0.0};
  double l1 = std::sqrt(t1[0] * t1[0] + t1[1] * t1[1] + t1[2] * t1[2]);
  if (l1 < 1e-6) { t1[0] = 1.0; t1[1] = 0.0; t1[2] = 0.0; l1 = 1.0; }
  t1[0] /= l1; t1[1] /= l1; t1[2] /= l1;
  double t2[3] = {up[1] * t1[2] - up[2] * t1[1], up[2] * t1[0] - up[0] * t1[2], up[0] * t1[1] - up[1] * t1[0]};
  double cellKm = 40.0;   /* accepted cell size (~4 km cells); FB_CELL_KM sweeps it */
  if (const char *ck = getenv("FB_CELL_KM")) cellKm = atof(ck);
  double domeSub = 0.5;   /* cell height-subtraction: lower = taller/more substantial puffs (FB_CELL_DOME) */
  if (const char *cd = getenv("FB_CELL_DOME")) domeSub = atof(cd);
  p[24] = (float)t1[0]; p[25] = (float)t1[1]; p[26] = (float)t1[2]; p[27] = (float)cellKm;
  p[28] = (float)t2[0]; p[29] = (float)t2[1]; p[30] = (float)t2[2]; p[31] = (float)domeSub;
  Queue.WriteBuffer(CloudUni, 0, p, sizeof p);
  CloudMidR = (double)(p[0] + p[1]) * 0.5;   /* mid-shell radius Mm (temporal reprojection depth) */
}

void FBRenderer::UpdateAtmosphere(const double eye[3], const double sunDir[3], const double right[3],
                                  const double camUp[3], const double fwd[3], const double moonDir[3],
                                  double dayF, double moonPh, double cloud) {
  double up[3] = {eye[0], eye[1], eye[2]};
  Norm3(up);
  double sd = sunDir[0] * up[0] + sunDir[1] * up[1] + sunDir[2] * up[2];
  double sunTan[3] = {sunDir[0] - up[0] * sd, sunDir[1] - up[1] * sd, sunDir[2] - up[2] * sd};
  Norm3(sunTan);
  double side[3];
  Cross3(up, sunTan, side);
  double camMm[3] = {eye[0] / 1e6, eye[1] / 1e6, eye[2] / 1e6};

  float a[44];
  auto put = [&](int i, const double v[3]) {
    a[i * 4] = (float)v[0]; a[i * 4 + 1] = (float)v[1]; a[i * 4 + 2] = (float)v[2]; a[i * 4 + 3] = 0.0f;
  };
  put(0, camMm); put(1, sunDir); put(2, up); put(3, sunTan); put(4, side);
  put(5, right); put(6, camUp); put(7, fwd);
  const float halfFov = 60.0f * 3.14159265f / 180.0f / 2.0f;
  a[32] = std::tan(halfFov);
  a[33] = (float)Width / (float)Height;
  a[34] = std::cos(0.5f * 3.14159265f / 180.0f);   /* sun angular radius */
  a[35] = 30.0f;                                    /* sun disc intensity */
  put(9, moonDir); a[39] = (float)moonPh;           /* moonDir.w = phase */
  a[40] = (float)dayF;                              /* skyExtra: day, EVS gate, cloud, moon radius */
  a[41] = GroundPhoto ? 1.0f : 0.0f;
  a[42] = (float)cloud;
  a[43] = 0.0045f * (float)MoonScale;               /* real angular radius x FB_MOON_SCALE (default 1) */
  Queue.WriteBuffer(AtmoBuf, 0, a, sizeof a);
}

/* Camera-RELATIVE [0,1] reversed-Z projection * view. Vertices arrive pre-translated by (origin-cam),
 * so the eye is at the ORIGIN and the view is pure rotation from the ECEF camera basis (right, up,
 * -fwd). This is the port's global-precision convention: no giant absolute ECEF coords reach float. */
static void MvpCamRel(float *m, const double R[3], const double Uc[3], const double F[3], int w, int h) {
  const float fov = 60.0f * 3.14159265f / 180.0f, asp = (float)w / (float)h;
  const float zn = 0.05f;
  const float f = 1.0f / std::tan(fov / 2.0f);
  float v[16] = {(float)R[0],  (float)Uc[0],  -(float)F[0],  0,
                 (float)R[1],  (float)Uc[1],  -(float)F[1],  0,
                 (float)R[2],  (float)Uc[2],  -(float)F[2],  0,
                 0,            0,             0,             1};
  /* infinite reversed-Z projection ([0,1]): z_clip = zn, w = -z_eye -> depth = zn / -z_eye */
  float p[16] = {f / asp, 0, 0, 0, 0, f, 0, 0, 0, 0, 0, -1, 0, 0, zn, 0};
  for (int c = 0; c < 4; c++)
    for (int r = 0; r < 4; r++) {
      m[c * 4 + r] = 0;
      for (int k = 0; k < 4; k++) m[c * 4 + r] += p[k * 4 + r] * v[c * 4 + k];
    }
}

/* Unit cross product a x b -> o. */
static void Cross3(const double a[3], const double b[3], double o[3]) {
  o[0] = a[1] * b[2] - a[2] * b[1];
  o[1] = a[2] * b[0] - a[0] * b[2];
  o[2] = a[0] * b[1] - a[1] * b[0];
}
static void Norm3(double v[3]) {
  double l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (l < 1e-9) l = 1.0;
  v[0] /= l; v[1] /= l; v[2] /= l;
}
/* ONE daylight factor from sun elevation (atmo.h w3_daylight, verbatim): full day above ~+3°, fading
 * through civil twilight, dark by nautical twilight (~-9°). Shared by sky, ground and star fade. */
static double DaylightFactor(double sunElDeg) {
  double t = (sunElDeg + 9.0) / 12.0;
  if (t < 0.0) t = 0.0;
  if (t > 1.0) t = 1.0;
  return t * t * (3.0 - 2.0 * t);
}

void FBRenderer::ConfigureSurface(void) {
  wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector canv{};
  canv.selector = Selector;
  wgpu::SurfaceDescriptor sd{};
  sd.nextInChain = &canv;
  Surface = Instance.CreateSurface(&sd);
  wgpu::SurfaceCapabilities caps{};
  Surface.GetCapabilities(Adapter, &caps);
  SurfaceFormat = caps.formatCount ? caps.formats[0] : wgpu::TextureFormat::BGRA8Unorm;
  for (uint32_t i = 0; i < caps.formatCount; i++)   /* prefer sRGB: the tonemap writes linear, the view encodes */
    if (caps.formats[i] == wgpu::TextureFormat::BGRA8UnormSrgb ||
        caps.formats[i] == wgpu::TextureFormat::RGBA8UnormSrgb) {
      SurfaceFormat = caps.formats[i];
      break;
    }
  SwapW = Width;
  SwapH = Height;
  wgpu::SurfaceConfiguration cfg{};
  cfg.device = Device;
  cfg.format = SurfaceFormat;
  cfg.usage = wgpu::TextureUsage::RenderAttachment;
  cfg.width = (uint32_t)SwapW;
  cfg.height = (uint32_t)SwapH;
  Surface.Configure(&cfg);
}

#ifdef __EMSCRIPTEN__
/* Live canvas backing-store size = clientSize x devicePixelRatio (the display resolution the upscale
 * pass should target). Returns packed (w<<16 | h), 0 if the canvas is gone. */
EM_JS(int, fb_canvas_px, (const char *sel), {
  var c = document.querySelector(UTF8ToString(sel));
  if (!c) return 0;
  var dpr = window.devicePixelRatio || 1;
  var w = Math.max(1, Math.round(c.clientWidth * dpr)) | 0;
  var h = Math.max(1, Math.round(c.clientHeight * dpr)) | 0;
  if (w > 4096) w = 4096;
  if (h > 4096) h = 4096;
  return (w << 16) | h;
})
#endif

/* Reconfigure the swapchain to the display size when the canvas changes (F-fullscreen, window resize).
 * Hysteresis (>= 8 px) avoids thrash from sub-pixel jitter (present.h pr_sync_size). The scene + HUD
 * stay fixed 720p (FrameTex); only the swapchain + upscale viewport follow. Surface mode only. */
void FBRenderer::SyncSwapSize(void) {
#ifdef __EMSCRIPTEN__
  if (Mode != Target::Surface || !Selector) return;
  int packed = fb_canvas_px(Selector);
  if (packed <= 0) return;
  int w = (packed >> 16) & 0xFFFF, h = packed & 0xFFFF;
  if (std::abs(w - SwapW) < 8 && std::abs(h - SwapH) < 8) return;
  SwapW = w;
  SwapH = h;
  wgpu::SurfaceConfiguration cfg{};
  cfg.device = Device;
  cfg.format = SurfaceFormat;
  cfg.usage = wgpu::TextureUsage::RenderAttachment;
  cfg.width = (uint32_t)SwapW;
  cfg.height = (uint32_t)SwapH;
  Surface.Configure(&cfg);
  printf("[FBRenderer] swapchain -> %dx%d (scene stays %dx%d)\n", SwapW, SwapH, Width, Height);
#endif
}

/* Offscreen final target: RGBA8UnormSrgb so the tonemap pass's linear-out write is sRGB-encoded by
 * the GPU on store, same as the surface's sRGB view — CopyTextureToBuffer then hands back bytes a
 * PNG writer can use directly, no CPU-side encode step. */
void FBRenderer::CreateOffscreenTarget(void) {
  SurfaceFormat = wgpu::TextureFormat::RGBA8UnormSrgb;
  wgpu::TextureDescriptor td{};
  td.size = {(uint32_t)Width, (uint32_t)Height, 1};
  td.format = SurfaceFormat;
  td.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
  OffscreenTex = Device.CreateTexture(&td);
}

void FBRenderer::RenderFrame(void) {
  if (!DeviceReady) return;
#ifdef FB_GPU_NOOP
  return;   /* bisect stage 2: totally inert frames — init-side death vs frame-side */
#endif
  wgpu::TextureView finalView;
  if (Mode == Target::Surface) {
    SyncSwapSize();   /* match the swapchain to the live display size before acquiring it */
    wgpu::SurfaceTexture st{};
    Surface.GetCurrentTexture(&st);
    if (FrameNo < 3 || !st.texture)
      printf("[FBRenderer] frame %u: surf status=%d tex=%s\n", FrameNo, (int)st.status,
             st.texture ? "ok" : "NULL");
    if (!st.texture) return;
    finalView = st.texture.CreateView();
  } else {
    finalView = OffscreenTex.CreateView();
  }
  /* Whole frame (scene + tonemap + HUD) renders into the fixed-720p FrameTex; the upscale pass at the
   * end resolves it onto finalView (swapchain at display size, or the 1:1 offscreen readback target). */
  wgpu::TextureView frameView = FrameTex.CreateView();
#ifdef FB_GPU_BISECT
  FrameNo++;
  return;   /* bisect: acquire only — does the device still die? */
#endif
  if (DeviceLost) return;   /* device gone (headless SwiftShader): CPU streaming lives on elsewhere */

  /* BOOT/TELEPORT LOADING SCREEN: black frame + "LOADING TERRAIN x%" (MAX7456 glyphs), no scene/sky.
   * The app keeps JSBSim frozen until the target cut is resident, then clears this — so the first scene
   * frame is already full-resolution (no low-res ladder). Reuses the HUD-text + upscale pipelines only. */
  if (LoadingScreen) {
    FrameNo++;
    FBFrameContext lctx{};   /* Upscale::Encode ignores ctx today; kept for interface uniformity */
    lctx.Width = Width; lctx.Height = Height;
    wgpu::CommandEncoder lenc = Device.CreateCommandEncoder();
    {
      wgpu::RenderPassColorAttachment ca{};
      ca.view = frameView; ca.loadOp = wgpu::LoadOp::Clear; ca.storeOp = wgpu::StoreOp::Store;
      ca.clearValue = {0, 0, 0, 1};
      wgpu::RenderPassDescriptor rp{}; rp.colorAttachmentCount = 1; rp.colorAttachments = &ca;
      wgpu::RenderPassEncoder pass = lenc.BeginRenderPass(&rp);
      Hud->EncodeLoadingText(pass, Width, Height, LoadPct, LoadReady, LoadTotal);
      pass.End();
    }
    {
      wgpu::RenderPassColorAttachment uca{};
      uca.view = finalView; uca.loadOp = wgpu::LoadOp::Clear; uca.storeOp = wgpu::StoreOp::Store;
      uca.clearValue = {0, 0, 0, 1};
      wgpu::RenderPassDescriptor upd{}; upd.colorAttachmentCount = 1; upd.colorAttachments = &uca;
      wgpu::RenderPassEncoder up = lenc.BeginRenderPass(&upd);
      Upscale->Encode(lctx, up);
      up.End();
    }
    wgpu::CommandBuffer lcmd = lenc.Finish(); Queue.Submit(1, &lcmd);
    if (FrameNo == 1) printf("[passcount] 2 passes/frame (loading screen: text + upscale)\n");
    return;
  }

  double t = FrameNo++ / 60.0;

  /* Camera basis in ECEF (radial up ~ geodetic up to <1deg). Scripted eye/target if SetCamera was
   * used (Stage 4 flight path); otherwise the default orbit over Center (static/native). */
  double eye[3], up[3], east[3], north[3], fwd[3], right[3], camUp[3];
  double zax[3] = {0, 0, 1};
  if (CameraFull) {   /* aircraft attitude: full rolled ECEF basis (horizon tilts at bank) */
    for (int a = 0; a < 3; a++) { eye[a] = Eye[a]; fwd[a] = Fwd[a]; right[a] = Right[a]; camUp[a] = Up[a]; }
  } else if (HaveCamera) {
    double g[3];
    for (int a = 0; a < 3; a++) { eye[a] = Eye[a]; g[a] = Eye[a]; }
    Norm3(g);
    for (int a = 0; a < 3; a++) fwd[a] = LookTarget[a] - eye[a];
    Norm3(fwd);
    Cross3(fwd, g, right); Norm3(right); Cross3(right, fwd, camUp);
  } else {
    double g[3] = {Center[0], Center[1], Center[2]}, e[3], nn[3];
    Norm3(g);
    Cross3(zax, g, e); Norm3(e); Cross3(g, e, nn);
    const double camH = 1500.0, camR = 6500.0, ang = t * 0.2;
    for (int a = 0; a < 3; a++)
      eye[a] = Center[a] + g[a] * camH + (e[a] * std::cos(ang) + nn[a] * std::sin(ang)) * camR;
    for (int a = 0; a < 3; a++) fwd[a] = Center[a] - eye[a];
    Norm3(fwd);
    Cross3(fwd, g, right); Norm3(right); Cross3(right, fwd, camUp);
  }
  /* Geographic frame at the eye (radial up) — drives the sun + atmosphere hemisphere, INDEPENDENT
   * of camera roll (only the view reconstruction in the sky pass uses the rolled camera basis). */
  for (int a = 0; a < 3; a++) up[a] = eye[a];
  Norm3(up);
  Cross3(zax, up, east); Norm3(east); Cross3(up, east, north);

  float u[20];
  MvpCamRel(u, right, camUp, fwd, Width, Height);
  /* Sun drives BOTH the terrain diffuse and the physically-based atmosphere, so sky and ground agree.
   * SVS (OSM) is a time-independent database view -> a constant 45° sun facing south. EVS (photo) is
   * the real camera -> the live ephemeris sun the app fed via SetHud (sun_el/sun_az, deg, az 0=N 90=E),
   * so dawn/dusk/night match the aerial imagery. az/el -> ECEF via the eye's radial ENU frame. */
  double elDeg = 45.0, azDeg = 180.0;
  if (GroundPhoto) { elDeg = HudState.sun_el; azDeg = HudState.sun_az; }
  const double el = elDeg * 3.14159265 / 180.0, az = azDeg * 3.14159265 / 180.0;
  const double ce = std::cos(el), se = std::sin(el), caz = std::cos(az), saz = std::sin(az);
  double sun[3];
  for (int a = 0; a < 3; a++) sun[a] = up[a] * se + (north[a] * caz + east[a] * saz) * ce;
  Norm3(sun);
  u[16] = (float)sun[0]; u[17] = (float)sun[1]; u[18] = (float)sun[2]; u[19] = 0;
  /* FBTilesStage::Encode() writes this into its own Uni buffer from ctx.Mvp20 (built below) — same
   * values, just written at draw time instead of here (both precede the single Queue.Submit). */

  /* Moon direction + real-sky factors (EVS only; SVS pins day=1 and the sky pass gates the extras
   * off). Daylight is w3_daylight(sun_el): full day above ~+3°, dark by ~-9° (nautical twilight). */
  double sunElDeg = std::asin(std::max(-1.0, std::min(1.0, sun[0] * up[0] + sun[1] * up[1] + sun[2] * up[2]))) * 180.0 / 3.14159265;
  double dayF = GroundPhoto ? DaylightFactor(sunElDeg) : 1.0;
  double moon[3];
  {
    double mel = HudState.moon_el * 3.14159265 / 180.0, maz = HudState.moon_az * 3.14159265 / 180.0;
    double cme = std::cos(mel);
    for (int a = 0; a < 3; a++)
      moon[a] = up[a] * std::sin(mel) + (north[a] * std::cos(maz) + east[a] * std::sin(maz)) * cme;
    Norm3(moon);
  }
  /* cloud=0 unless the path is armed: kills the sky-dome value-noise sheet (skyExtra.z) too, not just the
   * volumetric march — the whole cloud look is off by default. */
  double cloud = (CloudsOn && GroundPhoto) ? std::max(0.0, std::min(1.0, (double)HudState.cloud)) : 0.0;
  UpdateAtmosphere(eye, sun, right, camUp, fwd, moon, dayF, HudState.moon_phase, cloud);
  Stars->Update(SkyClock);
  if (CloudsOn) UpdateClouds(eye, sun, up, SkyClock > 0 ? SkyClock : (double)FrameNo / 60.0);

  /* Shared per-frame state every draw stage's Encode() reads (FBRenderer still decides WHEN each is
   * called — the pass topology/order below is unchanged from before the stage split). */
  FBFrameContext ctx{};
  for (int a = 0; a < 3; a++) { ctx.Eye[a] = eye[a]; ctx.Fwd[a] = fwd[a]; ctx.Right[a] = right[a]; ctx.CamUp[a] = camUp[a]; }
  for (int i = 0; i < 20; i++) ctx.Mvp20[i] = u[i];
  for (int a = 0; a < 3; a++) { ctx.SunDir[a] = sun[a]; ctx.MoonDir[a] = moon[a]; }
  ctx.DayFactor = dayF; ctx.MoonPhase = HudState.moon_phase; ctx.Cloud = cloud;
  ctx.GroundPhoto = GroundPhoto; ctx.SkyClock = SkyClock; ctx.DayFade = (float)dayF;
  ctx.FrameNo = FrameNo; ctx.Width = Width; ctx.Height = Height;

  wgpu::CommandEncoder enc = Device.CreateCommandEncoder();

  /* Pass-count proof (render/ stage-split acceptance criterion): the split into FBDrawStage classes
   * must not change the number of Begin*Pass calls/frame — count them here and log periodically so a
   * before/after diff is directly readable from the telemetry. */
  int passCount = 0;

  /* Atmosphere LUTs (compute, once per frame — TODO cache while the sun is static). */
  {
    wgpu::ComputePassEncoder cp = enc.BeginComputePass();
    passCount++;
    Transmittance->EncodeCompute(ctx, cp);
    cp.End();
  }
  {
    wgpu::ComputePassEncoder cp = enc.BeginComputePass();
    passCount++;
    SkyView->EncodeCompute(ctx, cp);
    cp.End();
  }

  /* Scene pass -> HDR offscreen: linear radiance, [0,1] reversed-Z depth. Sky fills the background
   * first (depth Always / no write); terrain draws over it. */
  wgpu::RenderPassColorAttachment ca{};
  ca.view = HdrTex.CreateView();
  ca.loadOp = wgpu::LoadOp::Clear;
  ca.storeOp = wgpu::StoreOp::Store;
  ca.clearValue = {0, 0, 0, 1};            /* irrelevant — the sky pass covers every pixel */
  wgpu::RenderPassDepthStencilAttachment da{};
  da.view = DepthTex.CreateView();
  da.depthLoadOp = wgpu::LoadOp::Clear;
  da.depthStoreOp = wgpu::StoreOp::Store;
  da.depthClearValue = 0.0f;               /* reversed-Z far */
  wgpu::RenderPassDescriptor sp{};
  sp.colorAttachmentCount = 1;
  sp.colorAttachments = &ca;
  sp.depthStencilAttachment = &da;
  wgpu::RenderPassEncoder scene = enc.BeginRenderPass(&sp);
  passCount++;
  Sky->Encode(ctx, scene);                 /* physically-based sky background, first in the pass */

  /* Real stars (EVS night): additive instanced quads at their true alt/az, over the sky, under the
   * terrain. FBStarsStage self-gates (SVS / daylight / none visible -> no draw). */
  Stars->Encode(ctx, scene);

  Tiles->Encode(ctx, scene);   /* terrain: RenderBundle (streaming) or direct per-tile draws (static) */

  Units->Encode(ctx, scene);   /* AI units draw slot (NoOp today) */

  /* Night lights (EVS night): additive sprites over the terrain, depth-tested so hills occlude far
   * ones. FBTileLightsStage self-gates (same fade window as the stars). */
  TileLights->Encode(ctx, scene);

  Sprites->Encode(ctx, scene);   /* future effect-billboard draw slot (NoOp today) */
  scene.End();

  /* Volumetric cloud pass -> the QUARTER-RES target (premultiplied cloud). SEPARATE pass so it can
   * SAMPLE the depth texture (now detached) for terrain occlusion; the tonemap upsamples + composites
   * it. EVS-only (the shader self-gates on skyExtra.y). Whole pass (march + resolve) skipped when the
   * cloud path is disarmed — the plain tonemap below presents the scene with no cloud composite. */
  int hcur = 0, hprev = 0;
  if (CloudsOn) {
  {
    wgpu::RenderPassColorAttachment cca{};
    cca.view = CloudLowTex.CreateView();
    cca.loadOp = wgpu::LoadOp::Clear;
    cca.storeOp = wgpu::StoreOp::Store;
    cca.clearValue = {0, 0, 0, 0};
    wgpu::RenderPassDescriptor cp{};
    cp.colorAttachmentCount = 1;
    cp.colorAttachments = &cca;
    wgpu::PassTimestampWrites tw{};   /* BOTH indices on this one pass (the cloud march = the cost we budget); leaving
       one index at kQuerySetIndexUndefined trips this Dawn build's validation -> rejects every command buffer. */
    if (HasTimestamp) { tw.querySet = TsQuery; tw.beginningOfPassWriteIndex = 0; tw.endOfPassWriteIndex = 1; cp.timestampWrites = &tw; }
    wgpu::RenderPassEncoder cl = enc.BeginRenderPass(&cp);
    passCount++;
    if (CloudQuality > 0.0) {
      cl.SetPipeline(CloudPipe);
      cl.SetBindGroup(0, CloudBind);
      cl.Draw(3);
    }
    cl.End();
  }

  /* Temporal resolve: accumulate the fresh jittered march into the reprojected history (kills the
   * per-frame "static"). Writes CloudHist[HistCur], reads CloudHist[1-HistCur] as the previous. */
  hcur = HistCur; hprev = 1 - HistCur;
  {
    float rb[24] = {0};
    for (int i = 0; i < 16; i++) rb[i] = PrevVP[i];
    rb[16] = (float)(eye[0] - PrevEye[0]);   /* camMove, metres */
    rb[17] = (float)(eye[1] - PrevEye[1]);
    rb[18] = (float)(eye[2] - PrevEye[2]);
    { uint32_t ph = FrameNo % 16u;           /* SAME 4x4-grid jitter the march used this frame (low-res px) */
      rb[19] = ((float)(ph % 4u) + 0.5f) / 4.0f - 0.5f; rb[23] = ((float)(ph / 4u) + 0.5f) / 4.0f - 0.5f; }
    if (AccumMode) { AccumN++; rb[20] = 1.0f; rb[21] = HistValid ? 3.0f : 2.0f; }   /* splat: 2=first accum frame, 3=has history */
    else { rb[20] = 0.12f; rb[21] = HistValid ? 1.0f : 0.0f; }
    rb[22] = (float)CloudMidR;
    Queue.WriteBuffer(ResolveUni, 0, rb, sizeof rb);
    wgpu::RenderPassColorAttachment rca[2]{};
    rca[0].view = CloudHist[hcur].CreateView();
    rca[0].loadOp = wgpu::LoadOp::Clear;
    rca[0].storeOp = wgpu::StoreOp::Store;
    rca[0].clearValue = {0, 0, 0, 0};
    rca[1].view = CloudWSum[hcur].CreateView();
    rca[1].loadOp = wgpu::LoadOp::Clear;
    rca[1].storeOp = wgpu::StoreOp::Store;
    rca[1].clearValue = {0, 0, 0, 0};
    wgpu::RenderPassDescriptor rp2{};
    rp2.colorAttachmentCount = 2;
    rp2.colorAttachments = rca;
    /* no timestampWrites on the resolve pass — both timestamps live on the cloud-march pass above */
    wgpu::RenderPassEncoder rz = enc.BeginRenderPass(&rp2);
    passCount++;
    rz.SetPipeline(CloudResolvePipe);
    rz.SetBindGroup(0, CloudResolveBind[hprev]);
    rz.Draw(3);
    rz.End();
  }
  }   /* end if (CloudsOn) — cloud march + resolve */

  /* Tonemap pass -> the fixed-720p FrameTex: ACES-compress HDR, sRGB-encode on store. No depth. */
  wgpu::RenderPassColorAttachment tca{};
  tca.view = frameView;
  tca.loadOp = wgpu::LoadOp::Clear;
  tca.storeOp = wgpu::StoreOp::Store;
  tca.clearValue = {0, 0, 0, 1};
  wgpu::RenderPassDescriptor tp{};
  tp.colorAttachmentCount = 1;
  tp.colorAttachments = &tca;
  wgpu::RenderPassEncoder tone = enc.BeginRenderPass(&tp);
  passCount++;
  if (CloudsOn) {
    tone.SetPipeline(TonemapPipe);
    tone.SetBindGroup(0, TonemapBindH[hcur]);   /* composite the temporally-accumulated cloud */
  } else {
    tone.SetPipeline(TonemapPlainPipe);         /* no cloud composite (default) */
    tone.SetBindGroup(0, TonemapBindPlain);
  }
  tone.Draw(3);
  tone.End();

  /* HUD overlay pass -> the SAME final target, loadOp Load (preserve the tonemapped scene). Geometry
   * is the reused symbology, rebuilt + uploaded each frame. Triangles (AA horizon) first, then the
   * line primitives, then the textured glyphs. */
  if (HudEnabled) {
    Hud->SetState(HudState, HudHave);
    wgpu::RenderPassColorAttachment hca{};
    hca.view = frameView;
    hca.loadOp = wgpu::LoadOp::Load;
    hca.storeOp = wgpu::StoreOp::Store;
    wgpu::RenderPassDescriptor hp{};
    hp.colorAttachmentCount = 1;
    hp.colorAttachments = &hca;
    wgpu::RenderPassEncoder hud = enc.BeginRenderPass(&hp);
    passCount++;
    Hud->Encode(ctx, hud);
    hud.End();
  }

  /* Upscale/present: sample the finished 720p FrameTex onto finalView (swapchain at display size, or
   * the offscreen readback target 1:1). The app owns the filter — bilinear here (TODO bicubic/sharpen).
   * SVS goes straight through; the EVS WebCodecs link (next stage) will swap the sampled source. */
  wgpu::RenderPassColorAttachment uca{};
  uca.view = finalView;
  uca.loadOp = wgpu::LoadOp::Clear;
  uca.storeOp = wgpu::StoreOp::Store;
  uca.clearValue = {0, 0, 0, 1};
  wgpu::RenderPassDescriptor upd{};
  upd.colorAttachmentCount = 1;
  upd.colorAttachments = &uca;
  wgpu::RenderPassEncoder upscale = enc.BeginRenderPass(&upd);
  passCount++;
  Upscale->Encode(ctx, upscale);
  upscale.End();

  /* Pass-count proof: log once on the first SCENE frame (not FrameNo==1, which is usually consumed by
   * the loading screen — a short native-oracle run must still capture this) and periodically after.
   * Expected today: 6 without clouds (2 compute + scene/tonemap/hud/upscale), 8 with FB_CLOUDS=1
   * (+ march + resolve); 5 if HudEnabled is off (cloud lab). */
  static bool loggedFirstPassCount = false;
  if (!loggedFirstPassCount || FrameNo % 300 == 0) {
    loggedFirstPassCount = true;
    printf("[passcount] %d passes/frame (clouds=%d hud=%d)\n", passCount, CloudsOn ? 1 : 0, HudEnabled ? 1 : 0);
  }

  if (HasTimestamp && CloudsOn) {   /* resolve the 2 timestamps; copy to the readback buffer only when it's free */
    enc.ResolveQuerySet(TsQuery, 0, 2, TsResolveBuf, 0);
    if (!TsMapPending) enc.CopyBufferToBuffer(TsResolveBuf, 0, TsReadBuf, 0, 2 * sizeof(uint64_t));
  }
  wgpu::CommandBuffer cmd = enc.Finish();
  Queue.Submit(1, &cmd);
  if (HasTimestamp && CloudsOn && !TsMapPending) {   /* async map -> accumulate GPU cloud-pass ms, log avg every 120 frames */
    TsMapPending = true;
    TsReadBuf.MapAsync(wgpu::MapMode::Read, 0, 2 * sizeof(uint64_t), wgpu::CallbackMode::AllowSpontaneous,
        [this](wgpu::MapAsyncStatus st, wgpu::StringView) {
          if (st == wgpu::MapAsyncStatus::Success) {
            const uint64_t *ts = static_cast<const uint64_t *>(TsReadBuf.GetConstMappedRange(0, 2 * sizeof(uint64_t)));
            if (ts && ts[1] > ts[0]) { TsAccumMs += (double)(ts[1] - ts[0]) / 1.0e6; TsCount++; }
            TsReadBuf.Unmap();
            if (TsCount >= 120) {
              printf("[cloud-perf] avg cloud march pass = %.3f ms/frame over %d frames (real GPU time)\n",
                     TsAccumMs / TsCount, TsCount);
              fflush(stdout); TsAccumMs = 0.0; TsCount = 0;
            }
          }
          TsMapPending = false;
        });
  }

  /* Temporal state for next frame's reprojection: this frame's view-proj + eye become "previous". */
  for (int i = 0; i < 16; i++) PrevVP[i] = u[i];
  for (int a = 0; a < 3; a++) PrevEye[a] = eye[a];
  HistCur = hprev;   /* the history we just wrote (hcur) becomes next frame's "previous" */
  HistValid = true;

  /* 2-phase-commit assertion (once/sec): no frame should ever have drawn an uncommitted layer. */
  if (Tiles->IsStreaming() && FrameNo % 60 == 0) {
    long notReady = Tiles->GetNotReadyDraws(), wrongMode = Tiles->GetWrongModeDraws(), black = Tiles->GetBlackDraws();
    printf("[present] notReadyDraws=%ld wrongModeDraws=%ld blackDraws=%ld (invariants: 0)%s | bundleRecords=%ld\n",
           notReady, wrongMode, black,
           (notReady || wrongMode || black) ? "  <-- VIOLATION" : "", Tiles->GetBundleRecords());
    fflush(stdout);
  }
}

/* Offscreen readback: CopyTextureToBuffer into a MapRead staging buffer (256-byte row-pitch
 * alignment, the WebGPU-wide rule) then MapAsync, blocking via Instance::WaitAny like the native
 * device bring-up above. Strips row padding on the way out. */
bool FBRenderer::ReadPixels(std::vector<uint8_t> &rgba) {
  const uint32_t bpp = 4;
  const uint32_t unpaddedRow = (uint32_t)Width * bpp;
  const uint32_t paddedRow = (unpaddedRow + 255u) & ~255u;
  const uint64_t bufSize = (uint64_t)paddedRow * (uint32_t)Height;

  wgpu::BufferDescriptor bd{};
  bd.size = bufSize;
  bd.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
  wgpu::Buffer staging = Device.CreateBuffer(&bd);

  wgpu::TexelCopyTextureInfo src{};
  src.texture = OffscreenTex;
  wgpu::TexelCopyBufferInfo dst{};
  dst.buffer = staging;
  dst.layout.bytesPerRow = paddedRow;
  dst.layout.rowsPerImage = (uint32_t)Height;
  wgpu::Extent3D ext{(uint32_t)Width, (uint32_t)Height, 1};

  wgpu::CommandEncoder enc = Device.CreateCommandEncoder();
  enc.CopyTextureToBuffer(&src, &dst, &ext);
  wgpu::CommandBuffer cmd = enc.Finish();
  Queue.Submit(1, &cmd);

  bool ok = false;
  Instance.WaitAny(
      staging.MapAsync(wgpu::MapMode::Read, 0, bufSize, wgpu::CallbackMode::WaitAnyOnly,
          [&ok](wgpu::MapAsyncStatus st, wgpu::StringView msg) {
            ok = (st == wgpu::MapAsyncStatus::Success);
            if (!ok) printf("[FBRenderer] buffer map failed: %.*s\n", (int)msg.length, msg.data);
          }),
      UINT64_MAX);
  if (!ok) return false;

  const uint8_t *mapped = static_cast<const uint8_t *>(staging.GetConstMappedRange(0, bufSize));
  rgba.resize((size_t)unpaddedRow * Height);
  for (int y = 0; y < Height; y++)
    memcpy(&rgba[(size_t)y * unpaddedRow], mapped + (size_t)y * paddedRow, unpaddedRow);
  staging.Unmap();
  return true;
}

/* Numeric shape/density histogram: evaluate the SAME base-shape math density() uses over a 3D grid in
 * the shell and read back the values, so base dynamic-range tuning is driven by numbers, not eyes.
 * Reports shape percentiles AND the post-coverage-remap density d = (shape-(1-cov))/cov percentiles —
 * the goal (per convergence guidance) is cloud CORES at d~1 with only edges in low d. */
void FBRenderer::ShapeStats(float cover, float low, float high) {
  const uint32_t GX = 128, GY = 128, GZ = 32, N = GX * GY * GZ;
  static const char *kHistCS = R"(
struct P { cov : f32, low : f32, high : f32, pad : f32 };
@group(0) @binding(0) var baseTex : texture_3d<f32>;
@group(0) @binding(1) var samp : sampler;
@group(0) @binding(2) var<storage, read_write> outv : array<f32>;
@group(0) @binding(3) var<uniform> C : P;
@compute @workgroup_size(4, 4, 4)
fn cs(@builtin(global_invocation_id) id : vec3u) {
  if (id.x >= 128u || id.y >= 128u || id.z >= 32u) { return; }
  let h = (f32(id.z) + 0.5) / 32.0;
  let posKm = vec3f(f32(id.x) * 0.31, f32(id.y) * 0.29, 2.0 + h * 2.6);   /* ~40x37 km patch, deck altitudes */
  let b0 = textureSampleLevel(baseTex, samp, posKm / 9.0, 0.0).r;
  let b1 = textureSampleLevel(baseTex, samp, posKm / 24.0, 0.0).r;
  let b = b1 * 0.6 + b0 * 0.4;
  let thresh = 1.0 - C.cov;
  var d = 0.0;
  if (b > thresh) {   /* mirror density(): low-freq tower height + dome envelope, no detail erosion */
    let cov = (b - thresh) / (1.0 - thresh);
    let topMax = mix(1.0 + 0.15 * C.high, 0.5, C.low);
    let topH = mix(0.35, topMax, pow(clamp((b1 - thresh) / (1.0 - thresh), 0.0, 1.0), 1.5));
    let hNorm = h / max(topH, 0.05);
    if (hNorm < 1.0) {
      let wispBase = smoothstep(0.0, 0.12, hNorm);
      let topFall = smoothstep(1.0, 0.72, hNorm);
      d = cov * wispBase * topFall;
    }
  }
  outv[id.x + id.y * 128u + id.z * 128u * 128u] = d;
}
)";
  wgpu::ShaderSourceWGSL w{};
  w.code = kHistCS;
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &w;
  wgpu::ShaderModule mod = Device.CreateShaderModule(&smd);
  wgpu::ComputePipelineDescriptor pd{};
  pd.compute.module = mod;
  pd.compute.entryPoint = "cs";
  wgpu::ComputePipeline pipe = Device.CreateComputePipeline(&pd);

  wgpu::BufferDescriptor sbd{};
  sbd.size = (uint64_t)N * sizeof(float);
  sbd.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc;
  wgpu::Buffer store = Device.CreateBuffer(&sbd);

  float uni[4] = {cover, low, high, 0.0f};
  wgpu::BufferDescriptor ubd{};
  ubd.size = sizeof(uni);
  ubd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  wgpu::Buffer ubuf = Device.CreateBuffer(&ubd);
  Queue.WriteBuffer(ubuf, 0, uni, sizeof(uni));

  wgpu::TextureViewDescriptor vd{};
  vd.dimension = wgpu::TextureViewDimension::e3D;
  wgpu::BindGroupEntry be[4]{};
  be[0].binding = 0; be[0].textureView = CloudBaseTex.CreateView(&vd);
  be[1].binding = 1; be[1].sampler = CloudSamp;
  be[2].binding = 2; be[2].buffer = store;
  be[3].binding = 3; be[3].buffer = ubuf;
  wgpu::BindGroupDescriptor bg{};
  bg.layout = pipe.GetBindGroupLayout(0);
  bg.entryCount = 4;
  bg.entries = be;
  wgpu::BindGroup bind = Device.CreateBindGroup(&bg);

  wgpu::BufferDescriptor rbd{};
  rbd.size = (uint64_t)N * sizeof(float);
  rbd.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
  wgpu::Buffer readb = Device.CreateBuffer(&rbd);

  wgpu::CommandEncoder enc = Device.CreateCommandEncoder();
  wgpu::ComputePassEncoder pass = enc.BeginComputePass();
  pass.SetPipeline(pipe);
  pass.SetBindGroup(0, bind);
  pass.DispatchWorkgroups(GX / 4, GY / 4, GZ / 4);
  pass.End();
  enc.CopyBufferToBuffer(store, 0, readb, 0, (uint64_t)N * sizeof(float));
  wgpu::CommandBuffer cmd = enc.Finish();
  Queue.Submit(1, &cmd);

  bool ok = false;
  Instance.WaitAny(readb.MapAsync(wgpu::MapMode::Read, 0, (uint64_t)N * sizeof(float),
      wgpu::CallbackMode::WaitAnyOnly, [&ok](wgpu::MapAsyncStatus st, wgpu::StringView) { ok = (st == wgpu::MapAsyncStatus::Success); }),
      UINT64_MAX);
  if (!ok) { printf("[shapehist] readback failed\n"); return; }
  const float *v = static_cast<const float *>(readb.GetConstMappedRange(0, (uint64_t)N * sizeof(float)));
  std::vector<float> s(v, v + N);
  readb.Unmap();
  std::sort(s.begin(), s.end());
  auto pct = [&](double p) { return s[(size_t)(p * (N - 1))]; };
  size_t cloud = 0, solid = 0;
  for (float x : s) { if (x > 0.001f) cloud++; if (x > 0.9f) solid++; }
  /* percentiles over CLOUD voxels (d>0) — the whole-grid median is ~0 since most of the shell is clear */
  size_t c0 = N - cloud;
  auto cpct = [&](double p) { return s[c0 + (size_t)(p * (cloud > 0 ? cloud - 1 : 0))]; };
  printf("[shapehist] cov=%.2f low=%.2f high=%.2f  N=%u (post-dome density d, no detail erosion)\n", cover, low, high, N);
  printf("[shapehist] d over CLOUD voxels: p10=%.3f median=%.3f p90=%.3f max=%.3f\n",
         cpct(0.10), cpct(0.50), cpct(0.90), s.back());
  printf("[shapehist] cloud voxels (d>0): %.1f%%  |  SOLID cores (d>0.9): %.2f%%\n",
         100.0 * cloud / N, 100.0 * solid / N);
  (void)pct;
}

} // namespace FlightBox
