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

namespace FlightBox::Render {

static void Cross3(const double a[3], const double b[3], double o[3]);   /* defined below */
static void Norm3(double v[3]);

/* Dawn callback messages are non-null-terminated views, not C strings. */
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
  /* Native Dawn drives Request{Adapter,Device} synchronously: there is no browser event loop here to
   * pump AllowSpontaneous callbacks. */
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
  /* adapterType==CPU means the WHOLE pipeline is software rasterization — then high CPU load is the
   * browser and the fix is browser-side (chrome://gpu, Firefox WebGPU/Vulkan flags). */
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
  /* rgba16float and not rg11b10ufloat: the cloud pass blends premultiplied alpha over the HDR target,
   * and rg11b10 has no alpha channel. Herleitung: doc/render/renderer.md §1.4. */
  bool rg11 = false;
  HdrFormat = wgpu::TextureFormat::RGBA16Float;
  wgpu::DeviceDescriptor dd{};
  std::vector<wgpu::FeatureName> feats;
  if (rg11) feats.push_back(wgpu::FeatureName::RG11B10UfloatRenderable);
  if (!feats.empty()) { dd.requiredFeatureCount = feats.size(); dd.requiredFeatures = feats.data(); }
  /* The multi-LOD albedo array outgrows the default 256-layer cap; ask for the adapter's real max. */
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
  { const char *e = getenv("FB_CLOUDS"); CloudsOn = !e || atoi(e) != 0; }   /* armed by default; the pass only exists when the weather has a deck */
  if (CloudsOn) CreateClouds();
  CreateTonemapPipeline();
  CreatePresent();          /* also Init()s Upscale (needs FrameTex, created here) */
  Hud->Init(gpu);
  DeviceReady = true;
  FBLog::Info("render", "device_ready", {{"width", Width}, {"height", Height},
                                         {"target", Mode == Target::Surface ? "surface" : "offscreen"},
                                         {"hdr", HdrFormat == wgpu::TextureFormat::RG11B10Ufloat
                                                     ? "rg11b10ufloat" : "rgba16float"}});
}

/* Creates the SCENE's shared targets and injects FBTilesStage's dependencies. Must run AFTER
 * CreateAtmosphere: Tiles' bind group pins the LUT views that method created. */
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


/* Lighting stays linear upstream; this is the only place display encoding happens. It knows nothing
 * about clouds any more: the cloud pass blends into HdrTex itself, so there is ONE pipeline again. */
void FBRenderer::CreateTonemapPipeline(void) {
  FBGpu gpu{Device, Queue, HdrFormat, SurfaceFormat, Width, Height, Instance};
  Tonemap->Configure(gpu, Samp, HdrTex.CreateView());
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

  /* Init-order CONTRACT: THIS order, because each later stage's bind group is built from an earlier
   * one's already-created texture view — a WebGPU bind group pins a view at creation, there is no
   * "rebind later". Same reason CreateAtmosphere runs before CreateTerrainPipeline. */
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

/* No bakes, no history, no textures: one pipeline over the atmosphere LUTs and the scene depth. Must
 * run after CreateTerrainPipeline (DepthTex) and CreateAtmosphere (the LUT views its bind group pins).
 * doc/render/clouds.md. */
void FBRenderer::CreateClouds(void) {
  FBGpu gpu{Device, Queue, HdrFormat, SurfaceFormat, Width, Height, Instance};
  Clouds->Configure(gpu, AtmoBuf, LutSamp, SkyLUT.CreateView(), TransLUT.CreateView(), DepthTex.CreateView());
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

/* Camera-relative: vertices arrive pre-translated by (origin-cam), so the eye is at the ORIGIN and the
 * view is pure rotation — no absolute ECEF coordinate ever reaches float.
 * Herleitung: doc/render/renderer.md §1.2. */
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
/* ONE daylight factor from sun elevation, shared by sky, ground and star fade: full day above ~+3°,
 * dark by nautical twilight (~-9°). */
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
/* Live canvas backing store = clientSize x devicePixelRatio, packed (w<<16 | h); 0 if it is gone. */
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

/* Scene + HUD stay fixed 720p; only the swapchain and upscale viewport follow the canvas. The 8 px
 * hysteresis keeps sub-pixel jitter from thrashing the reconfigure. */
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

/* RGBA8UnormSrgb so the GPU sRGB-encodes on store, exactly as the surface's sRGB view does:
 * CopyTextureToBuffer then hands back bytes a PNG writer uses directly, with no CPU encode step. */
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
  /* Scene + tonemap + HUD all land in the fixed-720p FrameTex; only the upscale pass touches finalView. */
  wgpu::TextureView frameView = FrameTex.CreateView();
#ifdef FB_GPU_BISECT
  FrameNo++;
  return;   /* bisect: acquire only — does the device still die? */
#endif
  if (DeviceLost) return;   /* device gone (headless SwiftShader): CPU streaming lives on elsewhere */

  /* Its own short frame path, 2 passes: the app freezes JSBSim until the target cut is resident, so
   * the first scene frame is already full resolution. doc/render/renderer.md §2.2. */
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

  /* Camera basis in ECEF (radial up ~ geodetic up to <1 deg): scripted eye/target if SetCamera was
   * used, otherwise the default orbit over Center. */
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
  /* Drives sun + atmosphere hemisphere, INDEPENDENT of camera roll — only the sky pass's view
   * reconstruction uses the rolled basis. */
  for (int a = 0; a < 3; a++) up[a] = eye[a];
  Norm3(up);
  Cross3(zax, up, east); Norm3(east); Cross3(up, east, north);

  float u[20];
  MvpCamRel(u, right, camUp, fwd, Width, Height);
  /* ONE sun for terrain diffuse and atmosphere, so sky and ground agree. SVS is a time-independent
   * database view -> a fixed 45° sun facing south; EVS is the real camera -> the live ephemeris sun. */
  double elDeg = 45.0, azDeg = 180.0;
  if (GroundPhoto) { elDeg = HudState.Env.SunElDeg; azDeg = HudState.Env.SunAzDeg; }
  const double el = elDeg * 3.14159265 / 180.0, az = azDeg * 3.14159265 / 180.0;
  const double ce = std::cos(el), se = std::sin(el), caz = std::cos(az), saz = std::sin(az);
  double sun[3];
  for (int a = 0; a < 3; a++) sun[a] = up[a] * se + (north[a] * caz + east[a] * saz) * ce;
  Norm3(sun);
  u[16] = (float)sun[0]; u[17] = (float)sun[1]; u[18] = (float)sun[2]; u[19] = 0;
  /* Moon direction + real-sky factors, EVS only: SVS pins day=1 and the sky pass gates the extras off. */
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
  /* cloud=0 kills the sky-dome noise sheet too, not just the volumetric march. */
  double cloud = (CloudsOn && GroundPhoto) ? std::max(0.0, std::min(1.0, (double)HudState.Env.CloudCover)) : 0.0;
  UpdateAtmosphere(eye, sun, right, camUp, fwd, moon, dayF, HudState.Env.MoonPhase, cloud);
  Stars->Update(SkyClock);

  /* The shared per-frame state every stage's Encode() reads; built before the cloud update so
   * FBCloudLayerStage::Update() can read it too. */
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

  if (CloudsOn) Clouds->Update(ctx);
  const bool cloudPass = CloudsOn && Clouds->Active();

  wgpu::CommandEncoder enc = Device.CreateCommandEncoder();

  /* PASS TOPOLOGY IS A CONTRACT: only this function opens and closes passes — a stage draws into the
   * borrowed encoder — and a stage split must never change this count. Hence the tally + the periodic
   * log below, so a before/after diff is readable straight from the telemetry.
   * Vollständige Encode-Reihenfolge: doc/render/renderer.md §2. */
  int passCount = 0;

  /* Once per frame — TODO cache while the sun is static. */
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

  Sun->Encode(ctx, scene);     /* additive (One/One), right after Sky */
  Moon->Encode(ctx, scene);

  Stars->Encode(ctx, scene);   /* additive, over the sky and under the terrain; self-gates */

  Tiles->Encode(ctx, scene);   /* terrain: RenderBundle (streaming) or direct per-tile draws (static) */

  /* NoOp today, but wired into the encode ORDER: units belong right after the terrain, effect
   * billboards right before the HUD — that placement is the contract, not the drawing. */
  Units->Encode(ctx, scene);

  TileLights->Encode(ctx, scene);   /* night lights, depth-tested so hills occlude far ones; self-gates */

  Sprites->Encode(ctx, scene);
  scene.End();

  /* A SEPARATE pass because it must SAMPLE the depth texture that was an attachment a moment ago —
   * and it blends premultiplied straight back into HdrTex, so nothing downstream knows about clouds.
   * It exists only when the weather actually has a deck: no weather, no pass, and the passcount log
   * below carries both numbers so a frame's topology is readable from the telemetry. */
  if (cloudPass) {
    wgpu::RenderPassColorAttachment cca{};
    cca.view = HdrTex.CreateView();
    cca.loadOp = wgpu::LoadOp::Load;     /* the scene stays; the cloud goes OVER it */
    cca.storeOp = wgpu::StoreOp::Store;
    wgpu::RenderPassDescriptor cp{};
    cp.colorAttachmentCount = 1;
    cp.colorAttachments = &cca;
    wgpu::RenderPassEncoder cl = enc.BeginRenderPass(&cp);
    passCount++;
    Clouds->Encode(ctx, cl);
    cl.End();
  }

  wgpu::RenderPassColorAttachment tca{};   /* tonemap -> FrameTex: ACES, sRGB-encode on store, no depth */
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

  /* Same target, loadOp Load to preserve the tonemapped scene. The `if` sits OUTSIDE the pass on
   * purpose: an empty pass would still be a pass, and the pass count is the invariant. */
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

  /* Present: 720p FrameTex -> finalView, bilinear (TODO bicubic/sharpen). */
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

  /* Logged on the first SCENE frame (FrameNo==1 is usually the loading screen, and a short
   * native-oracle run must still capture it), then every 300. Expected: 6 / 7 with a cloud deck /
   * 5 no HUD. */
  static bool loggedFirstPassCount = false;
  if (!loggedFirstPassCount || FrameNo % 300 == 0) {
    loggedFirstPassCount = true;
    FBLog::Debug("render", "passcount", {{"passes", passCount}, {"clouds", CloudsOn},
                                         {"cloudPass", cloudPass}, {"hud", HudEnabled}});
  }

  wgpu::CommandBuffer cmd = enc.Finish();
  Queue.Submit(1, &cmd);

  /* 2-phase-commit assertion (once/sec): no frame should ever have drawn an uncommitted layer. */
  if (Tiles->IsStreaming() && FrameNo % 60 == 0) {
    long notReady = Tiles->GetNotReadyDraws(), wrongMode = Tiles->GetWrongModeDraws(), black = Tiles->GetBlackDraws();
    bool violation = notReady || wrongMode || black;
    FBLog::Debug("render", "present", {{"notReadyDraws", (int)notReady}, {"wrongModeDraws", (int)wrongMode},
                                       {"blackDraws", (int)black}, {"violation", violation},
                                       {"bundleRecords", (int)Tiles->GetBundleRecords()}});
  }
}

/* CopyTextureToBuffer into a MapRead staging buffer (256-byte row pitch, the WebGPU rule), then
 * MapAsync blocking via Instance::WaitAny. Strips the row padding on the way out. */
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

} // namespace FlightBox::Render
