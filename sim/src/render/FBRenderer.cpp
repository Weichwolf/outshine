#include "FBRenderer.h"
#include "FBLog.h"
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

/* wgpu::StringView -> std::string for FBLog fields (Dawn callback messages are non-null-terminated
 * views, not C strings). */
static std::string SvToStr(wgpu::StringView v) { return std::string(v.data, v.length); }

FBRenderer::FBRenderer()
  : SurfaceFormat(wgpu::TextureFormat::Undefined), HdrFormat(wgpu::TextureFormat::RGBA16Float),
    SwapW(0), SwapH(0),
    MoonW(0), MoonH(0), MoonScale(1.0), SkyClock(0),
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
      FBLog::Error("render", "no_adapter", {{"msg", SvToStr(msg)}});
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
    FBLog::Info("render", "adapter", {{"vendor", SvToStr(info.vendor)}, {"arch", SvToStr(info.architecture)},
                                      {"device", SvToStr(info.device)}, {"desc", SvToStr(info.description)},
                                      {"type", at}, {"backend", (int)info.backendType}, {"software", soft},
                                      {"maxTexArrayLayers", (int)lim.maxTextureArrayLayers},
                                      {"maxBufferSizeMB", (double)(lim.maxBufferSize >> 20)},
                                      {"maxTexDim2D", (int)lim.maxTextureDimension2D}});
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
    FBLog::Error("render", "gpu_error", {{"type", (int)t}, {"msg", SvToStr(m)}});
  });
  dd.SetDeviceLostCallback(wgpu::CallbackMode::AllowSpontaneous,
      [this](const wgpu::Device &, wgpu::DeviceLostReason r, wgpu::StringView m) {
        DeviceLost = true;   /* guard GPU ops; the CPU streaming loop keeps running (counters live) */
        FBLog::Error("render", "device_lost", {{"reason", (int)r}, {"msg", SvToStr(m)}});
      });
  auto onDevice = [this](wgpu::RequestDeviceStatus st, wgpu::Device d, wgpu::StringView msg) {
    if (st != wgpu::RequestDeviceStatus::Success) {
      FBLog::Error("render", "no_device", {{"msg", SvToStr(msg)}});
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
  FBGpu gpu{Device, Queue, HdrFormat, SurfaceFormat, Width, Height, Instance};
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
  FBLog::Info("render", "device_ready", {{"width", Width}, {"height", Height},
                                         {"target", Mode == Target::Surface ? "surface" : "offscreen"},
                                         {"hdr", HdrFormat == wgpu::TextureFormat::RG11B10Ufloat
                                                     ? "rg11b10ufloat" : "rgba16float"}});
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

  FBGpu gpu{Device, Queue, HdrFormat, SurfaceFormat, Width, Height, Instance};
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

void FBRenderer::SetGroundMode(int photo) { GroundPhoto = photo != 0; }


/* Fullscreen ACES-approx tonemap (kTonemapWGSL/kTonemapPlainWGSL, FBTonemapStage): reads the HDR scene
 * target, encodes to the (sRGB) swapchain. Lighting stays linear upstream; this is the only place
 * display encoding happens. Depends on Resolve when CloudsOn (its CloudHist views), so this must run
 * AFTER CreateClouds(). */
void FBRenderer::CreateTonemapPipeline(void) {
  FBGpu gpu{Device, Queue, HdrFormat, SurfaceFormat, Width, Height, Instance};
  Tonemap->Configure(gpu, Samp, HdrTex.CreateView(), CloudsOn, CloudsOn ? Resolve.get() : nullptr);
}

void FBRenderer::CreatePresent(void) {
  wgpu::TextureDescriptor td{};   /* fixed 720p; scene + tonemap + HUD all land here */
  td.size = {(uint32_t)Width, (uint32_t)Height, 1};
  td.format = SurfaceFormat;      /* sRGB: the round-trip through the upscale is identity */
  td.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding |
             wgpu::TextureUsage::CopySrc;
  FrameTex = Device.CreateTexture(&td);

  FBGpu gpu{Device, Queue, HdrFormat, SurfaceFormat, Width, Height, Instance};
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

  /* Init-order CONTRACT: the atmosphere stages are Configure()d here, in THIS order, because each
   * later one's bind group is built from an EARLIER one's already-created texture view (WebGPU bind
   * groups pin a specific view at creation — there is no "rebind later"). Transmittance owns TransLUT;
   * SkyView reads TransLUT (injected) and writes SkyLUT; Sky reads SkyLUT (injected) + AtmoBuf; Sun
   * reads TransLUT (injected, solar colour) + AtmoBuf; Moon builds its own albedo texture from the
   * raw bytes SetMoonTexture staged + reads AtmoBuf. FBTilesStage (created after this method returns —
   * see OnDevice) likewise receives TransLUT/SkyLUT views injected at ITS Configure(), for the
   * terrain's aerial perspective — this is why CreateAtmosphere runs before CreateTerrainPipeline. */
  FBGpu gpu{Device, Queue, HdrFormat, SurfaceFormat, Width, Height, Instance};
  Transmittance->Configure(gpu, TransLUT.CreateView());
  SkyView->Configure(gpu, SkyLUT.CreateView(), TransLUT.CreateView(), LutSamp, AtmoBuf);
  Sky->Configure(gpu, SkyLUT.CreateView(), LutSamp, AtmoBuf);
  Sun->Configure(gpu, AtmoBuf, LutSamp, TransLUT.CreateView());
  Moon->Configure(gpu, AtmoBuf, LutSamp, MoonData.data(), MoonData.size(), MoonW, MoonH);
}

void FBRenderer::SetMoonTexture(const uint8_t *rgba, int w, int h) {
  if (!rgba || w <= 0 || h <= 0) return;
  MoonW = w; MoonH = h;
  MoonData.assign(rgba, rgba + (size_t)w * h * 4);
}

void FBRenderer::SetStars(const uint8_t *hyg, int nbytes, double originLat, double originLon) {
  Stars->SetCatalogue(hyg, nbytes, originLat, originLon);
}

/* Volumetric clouds (Nubis/MSFS-class): one class per shader (kein Big-Bang splitting further —
 * FBCloudBaseBakeStage/FBCloudDetailBakeStage/FBCloudCellBakeStage bake the 3 noise volumes ONCE via
 * FBCloudMipDownStage's shared box-downsample; FBCloudMarchStage raymarches the WGS84 spherical shell
 * into the quarter-res target; FBCloudResolveStage temporally upsamples it). This method only bakes
 * the noise + wires the per-frame stages together, in the Init-order their bind groups require:
 * bakes first (March's bind group pins their views), then March (Resolve's bind group pins March's
 * CloudLowTex view). EVS-only; skipped whole when CloudsOn is false (no boot/VRAM cost). */
void FBRenderer::CreateClouds(void) {
  FBGpu gpu{Device, Queue, HdrFormat, SurfaceFormat, Width, Height, Instance};
  CloudMipDown->Configure(gpu);
  BaseBake->Configure(gpu, *CloudMipDown);
  DetailBake->Configure(gpu, *CloudMipDown);
  CellBake->Configure(gpu);
  Cloud->Configure(gpu, AtmoBuf, LutSamp, SkyLUT.CreateView(), TransLUT.CreateView(), DepthTex.CreateView(),
                    BaseBake->GetView(), DetailBake->GetView(), CellBake->GetView(), HasTimestamp);
  Resolve->Configure(gpu, AtmoBuf, Samp, Cloud->GetLowView());
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
  FBLog::Info("render", "swapchain", {{"swapW", SwapW}, {"swapH", SwapH}, {"width", Width}, {"height", Height}});
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
      FBLog::Debug("render", "surface_texture", {{"frame", (int)FrameNo}, {"status", (int)st.status},
                                                  {"texture", st.texture ? "ok" : "NULL"}});
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
    if (FrameNo == 1) FBLog::Debug("render", "passcount", {{"passes", 2}, {"loading_screen", true}});
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
  if (GroundPhoto) { elDeg = HudState.Env.SunElDeg; azDeg = HudState.Env.SunAzDeg; }
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
    double mel = HudState.Env.MoonElDeg * 3.14159265 / 180.0, maz = HudState.Env.MoonAzDeg * 3.14159265 / 180.0;
    double cme = std::cos(mel);
    for (int a = 0; a < 3; a++)
      moon[a] = up[a] * std::sin(mel) + (north[a] * std::cos(maz) + east[a] * std::sin(maz)) * cme;
    Norm3(moon);
  }
  /* cloud=0 unless the path is armed: kills the sky-dome value-noise sheet (skyExtra.z) too, not just the
   * volumetric march — the whole cloud look is off by default. */
  double cloud = (CloudsOn && GroundPhoto) ? std::max(0.0, std::min(1.0, (double)HudState.Env.CloudCover)) : 0.0;
  UpdateAtmosphere(eye, sun, right, camUp, fwd, moon, dayF, HudState.Env.MoonPhase, cloud);
  Stars->Update(SkyClock);

  /* Shared per-frame state every draw stage's Encode() reads (FBRenderer still decides WHEN each is
   * called — the pass topology/order below is unchanged from before the stage split). Built here
   * (before the cloud update) so FBCloudMarchStage::Update() can read it too. */
  FBFrameContext ctx{};
  for (int a = 0; a < 3; a++) { ctx.Eye[a] = eye[a]; ctx.Fwd[a] = fwd[a]; ctx.Right[a] = right[a]; ctx.CamUp[a] = camUp[a]; ctx.Up[a] = up[a]; }
  for (int i = 0; i < 20; i++) ctx.Mvp20[i] = u[i];
  for (int a = 0; a < 3; a++) { ctx.SunDir[a] = sun[a]; ctx.MoonDir[a] = moon[a]; }
  ctx.DayFactor = dayF; ctx.MoonPhase = HudState.Env.MoonPhase; ctx.Cloud = cloud;
  ctx.GroundPhoto = GroundPhoto; ctx.SkyClock = SkyClock; ctx.DayFade = (float)dayF;
  ctx.CloudCover = HudState.Env.CloudCover;
  ctx.CloudLow = HudState.Env.CloudLow; ctx.CloudMid = HudState.Env.CloudMid; ctx.CloudHigh = HudState.Env.CloudHigh;
  ctx.CloudBaseAGL = HudState.Env.CloudBaseAglM; ctx.AltM = HudState.Platform.AltM;
  ctx.FrameNo = FrameNo; ctx.Width = Width; ctx.Height = Height;

  if (CloudsOn) Cloud->Update(ctx);

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

  /* Sun disc/glow + moon-as-lit-sphere: additive draws (One/One blend), same slot the original single
   * kSkyWGSL shader composited them into — encoded right after Sky so the result is pixel-equivalent. */
  Sun->Encode(ctx, scene);
  Moon->Encode(ctx, scene);

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
  if (CloudsOn) {
  {
    wgpu::RenderPassColorAttachment cca{};
    cca.view = Cloud->GetLowView();
    cca.loadOp = wgpu::LoadOp::Clear;
    cca.storeOp = wgpu::StoreOp::Store;
    cca.clearValue = {0, 0, 0, 0};
    wgpu::RenderPassDescriptor cp{};
    cp.colorAttachmentCount = 1;
    cp.colorAttachments = &cca;
    wgpu::PassTimestampWrites tw{};   /* BOTH indices on this one pass (the cloud march = the cost we budget); leaving
       one index at kQuerySetIndexUndefined trips this Dawn build's validation -> rejects every command buffer. */
    if (Cloud->WantsTimestamp()) { tw.querySet = Cloud->GetQuerySet(); tw.beginningOfPassWriteIndex = 0; tw.endOfPassWriteIndex = 1; cp.timestampWrites = &tw; }
    wgpu::RenderPassEncoder cl = enc.BeginRenderPass(&cp);
    passCount++;
    Cloud->Encode(ctx, cl);
    cl.End();
  }

  /* Temporal resolve: accumulate the fresh jittered march into the reprojected history (kills the
   * per-frame "static"). Writes into WriteIndex(), reads ReadIndex() as the previous. */
  {
    int w = Resolve->WriteIndex();
    wgpu::RenderPassColorAttachment rca[2]{};
    rca[0].view = Resolve->GetHistView(w);
    rca[0].loadOp = wgpu::LoadOp::Clear;
    rca[0].storeOp = wgpu::StoreOp::Store;
    rca[0].clearValue = {0, 0, 0, 0};
    rca[1].view = Resolve->GetWSumView(w);
    rca[1].loadOp = wgpu::LoadOp::Clear;
    rca[1].storeOp = wgpu::StoreOp::Store;
    rca[1].clearValue = {0, 0, 0, 0};
    wgpu::RenderPassDescriptor rp2{};
    rp2.colorAttachmentCount = 2;
    rp2.colorAttachments = rca;
    /* no timestampWrites on the resolve pass — both timestamps live on the cloud-march pass above */
    wgpu::RenderPassEncoder rz = enc.BeginRenderPass(&rp2);
    passCount++;
    Resolve->Encode(ctx, rz, Cloud->GetCloudMidR());
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
  Tonemap->Encode(ctx, tone);
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
    FBLog::Debug("render", "passcount", {{"passes", passCount}, {"clouds", CloudsOn}, {"hud", HudEnabled}});
  }

  if (CloudsOn) Cloud->ResolveTimestamps(enc);   /* resolve the 2 timestamps; copy to the readback buffer only when it's free */
  wgpu::CommandBuffer cmd = enc.Finish();
  Queue.Submit(1, &cmd);
  if (CloudsOn) Cloud->PollTimestamps();   /* async map -> accumulate GPU cloud-pass ms, log avg every 120 frames */

  /* Temporal state for next frame's reprojection: flips the ping-pong index + snapshots this frame's
   * view-proj/eye as "previous" (mirrors the original single-function ordering exactly). */
  if (CloudsOn) Resolve->Advance(ctx);

  /* 2-phase-commit assertion (once/sec): no frame should ever have drawn an uncommitted layer. */
  if (Tiles->IsStreaming() && FrameNo % 60 == 0) {
    long notReady = Tiles->GetNotReadyDraws(), wrongMode = Tiles->GetWrongModeDraws(), black = Tiles->GetBlackDraws();
    bool violation = notReady || wrongMode || black;
    FBLog::Debug("render", "present", {{"notReadyDraws", (int)notReady}, {"wrongModeDraws", (int)wrongMode},
                                       {"blackDraws", (int)black}, {"violation", violation},
                                       {"bundleRecords", (int)Tiles->GetBundleRecords()}});
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
            if (!ok) FBLog::Error("render", "buffer_map_failed", {{"msg", SvToStr(msg)}});
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

} // namespace FlightBox
