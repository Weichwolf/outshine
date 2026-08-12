#include "Renderer.h"
#include "Camera.h"
#include "Ephemeris.h"
#include "Geodesy.h"
#include "Log.h"
#include "stages/AtmoHaze.h"
#include "stages/SceneTargets.h"
#include <cstdint>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

namespace outshine::Render {

static void Cross3(const double a[3], const double b[3], double o[3]);   /* defined below */
static void Norm3(double v[3]);

/* The ambient floor a lit surface gets when the sun is down. Radiometrically night is far below what
 * a fixed exposure can show, and there is no auto-exposure yet; stages/SurfaceLight.h derives the
 * number and this is where the daylight factor gates it. */
static float NightAmbient(const FrameContext &ctx) {
  return (float)(0.00914 * (1.0 - ctx.DayFactor));
}

/* Dawn callback messages are non-null-terminated views, not C strings. */
static std::string SvToStr(wgpu::StringView v) { return std::string(v.data, v.length); }

Renderer::Renderer()
  : SurfaceFormat(wgpu::TextureFormat::Undefined), HdrFormat(wgpu::TextureFormat::RGBA16Float),
    MoonW(0), MoonH(0), MoonScale(1.0), SkyClock(0), SceneState{},
    Center{0, 0, 0}, HaveCamera(false), CameraFull(false), Eye{0, 0, 0}, LookTarget{0, 0, 0},
    Fwd{0, 0, 0}, Right{0, 0, 0}, Up{0, 0, 0}, Width(0), Height(0), DeviceReady(false),
    DeviceLost(false), FrameNo(0) {}

void Renderer::SetVegetationTable(const void *rows, size_t rowBytes, int bareRockRow,
                                  float slopeBandDeg) {
  VegRows.assign((const uint8_t *)rows, (const uint8_t *)rows + rowBytes);
  Geometry->Terrain().SetBareRock(bareRockRow, slopeBandDeg);
}

void Renderer::SetCamera(const double eye[3], const double target[3]) {
  for (int a = 0; a < 3; a++) { Eye[a] = eye[a]; LookTarget[a] = target[a]; }
  HaveCamera = true;
}

void Renderer::SetCameraBasis(const double eye[3], const double fwd[3], const double right[3],
                                const double up[3]) {
  for (int a = 0; a < 3; a++) { Eye[a] = eye[a]; Fwd[a] = fwd[a]; Right[a] = right[a]; Up[a] = up[a]; }
  CameraFull = true;
}

/* `TimedWaitAny` is what makes `WaitAny(UINT64_MAX)` below legal — without it it returns Error
 * before it looks at the future. */
wgpu::Instance Renderer::MakeInstance(void) const {
  wgpu::InstanceDescriptor id{};
  static constexpr auto kTimedWaitAny = wgpu::InstanceFeatureName::TimedWaitAny;
  id.requiredFeatureCount = 1;
  id.requiredFeatures = &kTimedWaitAny;
  wgpu::Instance made = wgpu::CreateInstance(&id);
  if (!made) Log::Error("render", "no_instance", {});
  return made;
}

void Renderer::Init(int width, int height, std::shared_ptr<const RenderPlan> plan) {
  Plan_ = std::move(plan);
  Width = width;
  Height = height;
  /* Dawn drives Request{Adapter,Device} synchronously here: there is no event loop to pump
   * AllowSpontaneous callbacks on. */
  Instance = MakeInstance();
  StartAdapterRequest();
}

void Renderer::StartAdapterRequest(void) {
  wgpu::RequestAdapterOptions opts{};
  auto onAdapter = [this](wgpu::RequestAdapterStatus st, wgpu::Adapter a, wgpu::StringView msg) {
    if (st != wgpu::RequestAdapterStatus::Success) {
      Log::Error("render", "no_adapter", {{"msg", SvToStr(msg)}});
      return;
    }
    OnAdapter(a);
  };
  Instance.WaitAny(Instance.RequestAdapter(&opts, wgpu::CallbackMode::WaitAnyOnly, onAdapter), UINT64_MAX);
}

void Renderer::OnAdapter(wgpu::Adapter a) {
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
    Log::Info("render", "adapter", {{"vendor", SvToStr(info.vendor)}, {"arch", SvToStr(info.architecture)},
                                      {"device", SvToStr(info.device)}, {"desc", SvToStr(info.description)},
                                      {"type", at}, {"backend", (int)info.backendType}, {"software", soft},
                                      {"maxTexArrayLayers", (int)lim.maxTextureArrayLayers},
                                      {"maxBufferSizeMB", (double)(lim.maxBufferSize >> 20)},
                                      {"maxTexDim2D", (int)lim.maxTextureDimension2D}});
  }
  /* rgba16float and not rg11b10ufloat: the cloud pass blends premultiplied alpha over the HDR target,
   * and rg11b10 has no alpha channel. The plan may widen it to rgba32float, and then the two
   * features below are what make that format behave like the narrow one instead of being refused at
   * every bind and every blend. */
  const bool wideScene =
      Plan_ && Plan_->Format(Resource::SceneHdr) == TexelFormat::Rgba32Float;
  HdrFormat = wideScene ? wgpu::TextureFormat::RGBA32Float : wgpu::TextureFormat::RGBA16Float;
  wgpu::DeviceDescriptor dd{};
  std::vector<wgpu::FeatureName> feats;
  if (wideScene) {
    for (wgpu::FeatureName wanted :
         {wgpu::FeatureName::Float32Filterable, wgpu::FeatureName::Float32Blendable}) {
      if (a.HasFeature(wanted)) { feats.push_back(wanted); }
    }
  }
  /* The per-pass clock is TELEMETRY and not a mode, so it is asked for whenever the
   * adapter has it. An adapter without it is not refused a device it could have had — the row then
   * says the stage times are absent. In Chrome the feature needs
   * --enable-dawn-features=allow_unsafe_apis; without it HasFeature is false and this is the branch
   * that reports it. */
  GpuTimeGranted = a.HasFeature(wgpu::FeatureName::TimestampQuery);
  if (GpuTimeGranted) feats.push_back(wgpu::FeatureName::TimestampQuery);
  if (!feats.empty()) { dd.requiredFeatureCount = feats.size(); dd.requiredFeatures = feats.data(); }
  /* The multi-LOD albedo array outgrows the default 256-layer cap; ask for the adapter's real max. */
  wgpu::Limits adapterLimits{};
  a.GetLimits(&adapterLimits);
  MaxLayers = (int)adapterLimits.maxTextureArrayLayers;
  wgpu::Limits reqLimits{};
  reqLimits.maxTextureArrayLayers = adapterLimits.maxTextureArrayLayers;
  dd.requiredLimits = &reqLimits;
  dd.SetUncapturedErrorCallback([](const wgpu::Device &, wgpu::ErrorType t, wgpu::StringView m) {
    Log::Error("render", "gpu_error", {{"type", (int)t}, {"msg", SvToStr(m)}});
  });
  dd.SetDeviceLostCallback(wgpu::CallbackMode::AllowSpontaneous,
      [this](const wgpu::Device &, wgpu::DeviceLostReason r, wgpu::StringView m) {
        DeviceLost = true;   /* guard GPU ops; the CPU streaming loop keeps running (counters live) */
        Log::Error("render", "device_lost", {{"reason", (int)r}, {"msg", SvToStr(m)}});
      });
  auto onDevice = [this](wgpu::RequestDeviceStatus st, wgpu::Device d, wgpu::StringView msg) {
    if (st != wgpu::RequestDeviceStatus::Success) {
      Log::Error("render", "no_device", {{"msg", SvToStr(msg)}});
      return;
    }
    OnDevice(d);
  };
  Instance.WaitAny(Adapter.RequestDevice(&dd, wgpu::CallbackMode::WaitAnyOnly, onDevice), UINT64_MAX);
}

void Renderer::OnDevice(wgpu::Device d) {
  Device = d;
  Queue = Device.GetQueue();
  /* The format the plan names for `frameTex` and `surface`: sRGB, so the GPU encodes on store and the
   * presented bytes need no CPU encode step. */
  SurfaceFormat = wgpu::TextureFormat::RGBA8UnormSrgb;

  /* WHAT THE PLAN HOLDS AND NOTHING ELSE. Resources first, then stages in the plan's derived order --
   * which is a topological order of the read/write graph, so a stage's bind group can never pin a
   * view of something that has not been created. That used to be two comments. */
  for (size_t r = 0; r < kResourceCount; ++r) {
    const Resource id = static_cast<Resource>(r);
    if (Plan_->Holds(id)) Create(id);
  }
  for (Stage stage : Plan_->Order()) Configure(stage);

  GpuTime.Configure(Device, GpuTimeGranted, Plan_->PassCount());
  DeviceReady = true;
  Log::Info("render", "device_ready", {{"width", Width}, {"height", Height},
                                       {"plan", Plan_->Digest()},
                                       {"passes", Plan_->PassCount()},
                                       {"stages", (int)Plan_->Order().size()}});
  for (const std::string &merge : Plan_->Merges())
    Log::Info("render", "plan_merge", {{"merge", merge}});
  for (const std::string &alias : Plan_->Aliases())
    Log::Info("render", "plan_alias", {{"alias", alias}});
}

/* WHAT A RESOURCE IS ON THIS DEVICE. Exhaustive over the catalogue and with no `default:`, so a new
 * row does not compile until this answers for it. The three resources a stage owns -- the shadow
 * atlas, the occlusion buffer and the linear resolve's history pair -- are created by that stage's
 * Configure, and the plan's order is what guarantees they exist before a reader binds them. */
void Renderer::Create(Resource resource) {
  auto lut = [&](uint32_t w, uint32_t h) {
    wgpu::TextureDescriptor td{};
    td.size = {w, h, 1};
    td.format = wgpu::TextureFormat::RGBA16Float;
    td.usage = wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::TextureBinding;
    return Device.CreateTexture(&td);
  };
  auto buffer = [&](uint64_t bytes, wgpu::BufferUsage usage) {
    wgpu::BufferDescriptor bd{};
    bd.size = bytes;
    bd.usage = usage;
    return Device.CreateBuffer(&bd);
  };
  auto target = [&](wgpu::TextureFormat format, wgpu::TextureUsage usage) {
    wgpu::TextureDescriptor td{};
    td.size = {(uint32_t)Width, (uint32_t)Height, 1};
    td.format = format;
    td.usage = usage;
    return Device.CreateTexture(&td);
  };
  const wgpu::TextureUsage kAttachment =
      wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;

  switch (resource) {
    case Resource::LinearSampler: {
      wgpu::SamplerDescriptor sd{};
      sd.addressModeU = wgpu::AddressMode::ClampToEdge;
      sd.addressModeV = wgpu::AddressMode::ClampToEdge;
      sd.magFilter = wgpu::FilterMode::Linear;
      sd.minFilter = wgpu::FilterMode::Linear;
      sd.mipmapFilter = wgpu::MipmapFilterMode::Linear;   /* trilinear across the mip chain */
      sd.maxAnisotropy = 16;   /* the grazing-mip bias in the terrain fs handles the >16:1 tail */
      Samp = Device.CreateSampler(&sd);
      return;
    }
    case Resource::LutSampler: {
      wgpu::SamplerDescriptor ss{};
      ss.addressModeU = wgpu::AddressMode::Repeat;        /* sky-view azimuth wraps */
      ss.addressModeV = wgpu::AddressMode::ClampToEdge;
      ss.addressModeW = wgpu::AddressMode::ClampToEdge;
      ss.magFilter = wgpu::FilterMode::Linear;
      ss.minFilter = wgpu::FilterMode::Linear;
      LutSamp = Device.CreateSampler(&ss);
      return;
    }
    case Resource::AtmosphereUniform:
      /* 12 vec4: camera and sun basis, moon direction, sky extra, view. */
      AtmoBuf = buffer(12 * 4 * sizeof(float),
                       wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst);
      return;
    case Resource::CascadeUniform:
      CsmBuf = buffer(kShadowUniFloats * sizeof(float),
                      wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst);
      return;
    case Resource::VegetationTable:
      if (VegRows.empty()) return;
      VegBuf = buffer(VegRows.size(), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);
      Queue.WriteBuffer(VegBuf, 0, VegRows.data(), VegRows.size());
      return;
    case Resource::TransmittanceLut: TransLUT = lut(256, 64); return;
    case Resource::MultiScatterLut:
      MsLUT = lut((uint32_t)MultiScatterStage::kSide, (uint32_t)MultiScatterStage::kSide);
      return;
    case Resource::SkyViewLut: SkyLUT = lut(192, 108); return;
    case Resource::IrradianceBuffer:
      IrrBuf = buffer(IrradianceStage::kBufferBytes, wgpu::BufferUsage::Storage |
                                                        wgpu::BufferUsage::CopyDst |
                                                        wgpu::BufferUsage::CopySrc);
      return;
    case Resource::Meter:
      MeterBuf = buffer(ExposureStage::kMeterBytes, wgpu::BufferUsage::Storage |
                                                       wgpu::BufferUsage::CopyDst |
                                                       wgpu::BufferUsage::CopySrc);
      return;
    case Resource::SceneHdr:
      /* CopySrc because the scene-referred linear tap reads it wherever the plan's alias put
       * the resolve's place here. */
      HdrTex = target(HdrFormat, kAttachment | wgpu::TextureUsage::CopySrc);
      return;
    case Resource::SceneVelocity: VelTex = target(kVelocityFormat, kAttachment); return;
    case Resource::SceneDepth:
      DepthTex = target(wgpu::TextureFormat::Depth32Float, kAttachment | wgpu::TextureUsage::CopySrc);
      return;
    case Resource::FrameTex:
      FrameTex = target(SurfaceFormat, kAttachment | wgpu::TextureUsage::CopySrc);
      return;
    case Resource::Surface: {
      wgpu::TextureDescriptor td{};
      td.size = {(uint32_t)Width, (uint32_t)Height, 1};
      td.format = SurfaceFormat;
      td.usage = wgpu::TextureUsage::RenderAttachment;
      OffscreenTex = Device.CreateTexture(&td);
      return;
    }
    /* Owned by the stage that writes them, and reachable through View(). */
    case Resource::ShadowAtlas:
    case Resource::AoBuffer:
    case Resource::SceneLinear:
    case Resource::kCount:
      return;
  }
}

/* WHERE A READER BINDS. Buffers are named directly by the Configure that needs them; this answers
 * only for the texture views, including the three a stage owns. */
wgpu::TextureView Renderer::View(Resource resource) const {
  switch (resource) {
    case Resource::TransmittanceLut: return TransLUT ? TransLUT.CreateView() : wgpu::TextureView();
    case Resource::MultiScatterLut: return MsLUT ? MsLUT.CreateView() : wgpu::TextureView();
    case Resource::SkyViewLut: return SkyLUT ? SkyLUT.CreateView() : wgpu::TextureView();
    case Resource::ShadowAtlas: return Shadow->AtlasView();
    case Resource::SceneHdr: return HdrTex ? HdrTex.CreateView() : wgpu::TextureView();
    case Resource::SceneVelocity: return VelTex ? VelTex.CreateView() : wgpu::TextureView();
    case Resource::SceneDepth: return DepthTex ? DepthTex.CreateView() : wgpu::TextureView();
    case Resource::AoBuffer: return Ao->OutputView();
    case Resource::SceneLinear: return Taa->Output(FrameNo);
    case Resource::FrameTex: return FrameTex ? FrameTex.CreateView() : wgpu::TextureView();
    case Resource::Surface: return OffscreenTex ? OffscreenTex.CreateView() : wgpu::TextureView();
    case Resource::LinearSampler:
    case Resource::LutSampler:
    case Resource::AtmosphereUniform:
    case Resource::CascadeUniform:
    case Resource::VegetationTable:
    case Resource::IrradianceBuffer:
    case Resource::Meter:
    case Resource::kCount:
      return wgpu::TextureView();
  }
  return wgpu::TextureView();
}

/* THE DISPLAY TRANSFER THE PLAN DECLARED, as the two stages that emit it read it. */
DisplayOptions Renderer::Display(void) const {
  DisplayOptions options;
  options.HasOcclusion = Plan_->Holds(Resource::AoBuffer);
  options.HasMeter = Plan_->Holds(Resource::Meter);
  options.Exposure = Plan_->Exposure();
  options.Curve = Plan_->Display();
  return options;
}

void Renderer::Configure(Stage stage) {
  Gpu gpu{Device, Queue, HdrFormat, SurfaceFormat, Width, Height};
  switch (stage) {
    case Stage::Transmittance: Transmittance->Configure(gpu, View(Resource::TransmittanceLut)); return;
    case Stage::MultiScatter:
      MultiScatter->Configure(gpu, View(Resource::MultiScatterLut),
                              View(Resource::TransmittanceLut), LutSamp);
      return;
    case Stage::SkyView:
      SkyView->Configure(gpu, View(Resource::SkyViewLut), View(Resource::TransmittanceLut), LutSamp,
                         AtmoBuf, View(Resource::MultiScatterLut));
      return;
    case Stage::Irradiance:
      Irradiance->Configure(gpu, IrrBuf, View(Resource::SkyViewLut),
                            View(Resource::TransmittanceLut), LutSamp, AtmoBuf);
      return;
    case Stage::AutoExposure: Exposure->Configure(gpu, MeterBuf, IrrBuf); return;
    case Stage::ShadowMap: Shadow->Init(gpu); return;
    case Stage::Sky: Sky->Configure(gpu, View(Resource::SkyViewLut), LutSamp, AtmoBuf); return;
    case Stage::Sun:
      Sun->Configure(gpu, AtmoBuf, LutSamp, View(Resource::TransmittanceLut));
      return;
    case Stage::Moon:
      Moon->Configure(gpu, AtmoBuf, LutSamp, MoonData.data(), MoonData.size(), MoonW, MoonH);
      return;
    case Stage::Stars: Stars->Init(gpu); return;
    case Stage::BenchGround: BenchGround->Configure(gpu, Light()); return;
    case Stage::Terrain:
      Geometry->Terrain().Configure(gpu, LutSamp, View(Resource::SkyViewLut), AtmoBuf, MaxLayers,
                                    VegBuf, Light());
      return;
    case Stage::Buildings: Geometry->Buildings().Configure(gpu, Light()); return;
    case Stage::Water: Geometry->Water().Configure(gpu, Light()); return;
    case Stage::Models: Geometry->Models().Configure(gpu, Light()); return;
    case Stage::Subjects: Geometry->Subjects().Configure(gpu); return;
    case Stage::Occlusion:
      Ao->Configure(gpu, View(Resource::SceneDepth), AtmoBuf, Width, Height);
      return;
    case Stage::TemporalResolve:
      Taa->Configure(gpu, Samp, View(Resource::SceneHdr), View(Resource::SceneVelocity),
                     View(Resource::SceneDepth), AtmoBuf, View(Resource::AoBuffer), MeterBuf,
                     Display(), Width, Height);
      return;
    case Stage::Tonemap:
      /* R2 put the display transfer in the resolve's own fragment; there is no second pipeline. */
      if (Plan_->Fused(Stage::Tonemap)) return;
      Tonemap->Configure(gpu, Bound(Resource::SceneLinear), View(Resource::SceneDepth),
                         View(Resource::AoBuffer), MeterBuf, Display());
      return;
    case Stage::Present: Present->Configure(gpu, View(Resource::FrameTex)); return;
    case Stage::kCount: return;
  }
}

void Renderer::EncodeStage(Stage stage, const FrameContext &ctx, wgpu::ComputePassEncoder &pass) {
  switch (stage) {
    case Stage::Transmittance: Transmittance->EncodeCompute(ctx, pass); return;
    case Stage::MultiScatter: MultiScatter->EncodeCompute(ctx, pass); return;
    case Stage::SkyView: SkyView->EncodeCompute(ctx, pass); return;
    case Stage::Irradiance: Irradiance->EncodeCompute(ctx, pass); return;
    case Stage::AutoExposure: Exposure->EncodeCompute(ctx, pass); return;
    /* Every raster stage of the catalogue: the compiler never puts one in a compute pass, and
     * listing them is what makes a new stage a compile error here rather than a silent no-op. */
    case Stage::ShadowMap:
    case Stage::Sky:
    case Stage::Sun:
    case Stage::Moon:
    case Stage::Stars:
    case Stage::BenchGround:
    case Stage::Terrain:
    case Stage::Buildings:
    case Stage::Water:
    case Stage::Models:
    case Stage::Subjects:
    case Stage::Occlusion:
    case Stage::TemporalResolve:
    case Stage::Tonemap:
    case Stage::Present:
    case Stage::kCount:
      return;
  }
}

void Renderer::EncodeStage(Stage stage, const FrameContext &ctx, wgpu::RenderPassEncoder &pass) {
  switch (stage) {
    case Stage::ShadowMap: Shadow->Encode(ctx, pass); return;
    case Stage::Sky: Sky->Encode(ctx, pass); return;
    case Stage::Sun: Sun->Encode(ctx, pass); return;
    case Stage::Moon: Moon->Encode(ctx, pass); return;
    case Stage::Stars: Stars->Encode(ctx, pass); return;
    case Stage::BenchGround:
      BenchGround->SetSun(ctx.SunDir, NightAmbient(ctx));
      BenchGround->Encode(ctx, pass);
      return;
    case Stage::Terrain:
    case Stage::Buildings:
    case Stage::Water:
    case Stage::Models:
    case Stage::Subjects:
      Geometry->EncodeUnit(stage, ctx, pass);
      return;
    case Stage::Occlusion: Ao->Encode(ctx, pass); return;
    case Stage::TemporalResolve: Taa->Encode(ctx, pass); return;
    case Stage::Tonemap:
      if (Plan_->Fused(Stage::Tonemap)) return;   /* the resolve's fragment already wrote it */
      Tonemap->Encode(ctx, pass);
      return;
    case Stage::Present: Present->Encode(ctx, pass); return;
    case Stage::Transmittance:
    case Stage::MultiScatter:
    case Stage::SkyView:
    case Stage::Irradiance:
    case Stage::AutoExposure:
    case Stage::kCount:
      return;
  }
}

SceneLight Renderer::Light(void) const {
  SceneLight l{};
  l.Irradiance = IrrBuf;
  l.Cascades = CsmBuf;
  l.ShadowAtlas = Shadow->AtlasView();
  l.ShadowCompare = Shadow->CompareSampler();
  return l;
}

void Renderer::SetMoonTexture(const uint8_t *rgba, int w, int h) {
  if (!rgba || w <= 0 || h <= 0) return;
  MoonW = w; MoonH = h;
  MoonData.assign(rgba, rgba + (size_t)w * h * 4);
}

void Renderer::SetStars(const uint8_t *hyg, int nbytes, double originLat, double originLon) {
  Stars->SetCatalogue(hyg, nbytes, originLat, originLon);
}

/* No bakes, no history, no textures: one pipeline over the atmosphere LUTs and the scene depth. Must
 * run after CreateTerrainPipeline (DepthTex) and CreateAtmosphere (the LUT views its bind group
 * pins). */
void Renderer::UpdateAtmosphere(const double eye[3], const double sunDir[3], const double right[3],
                                  const double camUp[3], const double fwd[3], const double moonDir[3],
                                  double dayF, double moonPh) {
  double up[3] = {eye[0], eye[1], eye[2]};
  Norm3(up);
  double sd = sunDir[0] * up[0] + sunDir[1] * up[1] + sunDir[2] * up[2];
  double sunTan[3] = {sunDir[0] - up[0] * sd, sunDir[1] - up[1] * sd, sunDir[2] - up[2] * sd};
  Norm3(sunTan);
  double side[3];
  Cross3(up, sunTan, side);
  double camMm[3] = {eye[0] / 1e6, eye[1] / 1e6, eye[2] / 1e6};

  float a[48];
  auto put = [&](int i, const double v[3]) {
    a[i * 4] = (float)v[0]; a[i * 4 + 1] = (float)v[1]; a[i * 4 + 2] = (float)v[2]; a[i * 4 + 3] = 0.0f;
  };
  put(0, camMm); put(1, sunDir); put(2, up); put(3, sunTan); put(4, side);
  /* camPosMm.w: the real WGS84 ground radius under the eye, so every atmosphere shader can rebase
   * onto the model's 6360 km sphere (AtmoCommon.h atmoPos). */
  a[3] = (float)((std::sqrt(eye[0] * eye[0] + eye[1] * eye[1] + eye[2] * eye[2])
                  - (double)SceneState.Platform.AltM) / 1.0e6);
  put(5, right); put(6, camUp); put(7, fwd);
  /* The same projection MvpCamRel builds, in the form the sky/cloud ray reconstruction uses. Sky and
   * terrain must agree on the ray or the horizon parts company with the ground. */
  const float halfFov = FovDeg * 3.14159265f / 180.0f / 2.0f;
  a[32] = std::tan(halfFov);
  a[33] = (float)Width / (float)Height;
  a[34] = std::cos(0.5f * 3.14159265f / 180.0f);   /* sun angular radius */
  /* Radiance of the DRAWN disc per unit top-of-atmosphere irradiance, which is what every atmosphere
   * shader is normalised to: L = E / omega with omega = 2 pi (1 - cos theta) of the very cosine above,
   * so widening the disc cannot change the flux it carries. 0.5 deg gives 1/2.392e-4 = 4180. */
  a[35] = 1.0f / (2.0f * 3.14159265f * (1.0f - a[34]));
  put(9, moonDir); a[39] = (float)moonPh;           /* moonDir.w = phase */
  a[40] = (float)dayF;                              /* skyExtra: day, spare, spare, moon radius */
  a[41] = 0.0f;
  a[42] = 0.0f;
  a[43] = 0.0045f * (float)MoonScale;               /* real angular radius x FB_MOON_SCALE (default 1) */
  a[44] = 0.0f;   /* view.xy — the vec4 stride's spare: nothing rides here */
  a[45] = 0.0f;
  /* view.zw — the frame's sub-pixel sample offset in NDC. camRay() subtracts it, because the ray
   * that belongs to a pixel is the one the JITTERED projection would have put there; without it the
   * sun disc's own edge and the sky behind a terrain silhouette would sample half a pixel apart. */
  a[46] = 2.0f * Jitter.PixelX() / (float)Width;
  a[47] = 2.0f * Jitter.PixelY() / (float)Height;
  Queue.WriteBuffer(AtmoBuf, 0, a, sizeof a);
}

/* Camera-relative: vertices arrive pre-translated by (origin-cam), so the eye is at the ORIGIN and the
 * view is pure rotation — no absolute ECEF coordinate ever reaches float. */
/* The SUB-PIXEL SAMPLE POSITION is a constant NDC offset on the z column, i.e. a shear of the frustum
 * and not a translation of the world. That is what makes it a camera property — every world-fixed
 * thing keeps its place, and only the grid the rasteriser asks its coverage question on moves
 * (render/TemporalJitter.h). */
static void MvpCamRel(float *m, const double R[3], const double Uc[3], const double F[3], int w,
                      int h, float fovDeg, float jitNdcX, float jitNdcY, float orthoM) {
  const float fov = fovDeg * 3.14159265f / 180.0f, asp = (float)w / (float)h;
  const float zn = Renderer::kNearM;
  const float f = 1.0f / std::tan(fov / 2.0f);
  float v[16] = {(float)R[0],  (float)Uc[0],  -(float)F[0],  0,
                 (float)R[1],  (float)Uc[1],  -(float)F[1],  0,
                 (float)R[2],  (float)Uc[2],  -(float)F[2],  0,
                 0,            0,             0,             1};
  /* infinite reversed-Z projection ([0,1]): z_clip = zn, w = -z_eye -> depth = zn / -z_eye.
   * The z column carries the jitter on the x and y rows. */
  float p[16] = {f / asp, 0, 0, 0, 0, f, 0, 0, -jitNdcX, -jitNdcY, 0, -1, 0, 0, zn, 0};
  if (orthoM > 0.0f) {
    /* A comparison against a map is a comparison against a PARALLEL projection; under perspective the
     * same field is two different shapes at the centre and at the corner. */
    const float hw = 0.5f * orthoM * asp, hh = 0.5f * orthoM;
    const float zf = 60000.0f, rz = 1.0f / (zf - zn);
    const float q[16] = {1.0f / hw, 0, 0, 0,
                         0, 1.0f / hh, 0, 0,
                         0, 0, rz, 0,
                         -jitNdcX, -jitNdcY, zf * rz, 1};
    for (int i = 0; i < 16; i++) p[i] = q[i];
  }
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

/* RGBA8UnormSrgb so the GPU sRGB-encodes on store, exactly as the surface's sRGB view does — and so
 * the presented bytes need no CPU encode step. It is a present target only; the readback takes
 * FrameTex, which both translations have. */
void Renderer::BakePrototypeImpostor(void) {
  if (!Device || !Geometry->Models().WantsBake()) return;
  Geometry->Models().CreateImpostor();
  if (!Geometry->Models().ImpostorDepthTarget()) return;

  wgpu::RenderPassColorAttachment ca[2] = {};
  ca[0].view = Geometry->Models().ImpostorAlbedoTarget();
  ca[0].loadOp = wgpu::LoadOp::Clear;
  ca[0].storeOp = wgpu::StoreOp::Store;
  ca[0].clearValue = {0.0, 0.0, 0.0, 0.0};
  ca[1] = ca[0];
  ca[1].view = Geometry->Models().ImpostorNormalTarget();
  ca[1].clearValue = {0.5, 0.5, 0.5, 0.0};
  wgpu::RenderPassDepthStencilAttachment da{};
  da.view = Geometry->Models().ImpostorDepthTarget();
  da.depthLoadOp = wgpu::LoadOp::Clear;
  da.depthStoreOp = wgpu::StoreOp::Discard;
  da.depthClearValue = 0.0f;   /* reversed-Z, as every scene surface */
  wgpu::RenderPassDescriptor rp{};
  rp.colorAttachmentCount = 2;
  rp.colorAttachments = ca;
  rp.depthStencilAttachment = &da;

  wgpu::CommandEncoder enc = Device.CreateCommandEncoder();
  wgpu::RenderPassEncoder pass = enc.BeginRenderPass(&rp);
  Geometry->Models().EncodeBake(pass);
  pass.End();
  wgpu::CommandBuffer cb = enc.Finish();
  Queue.Submit(1, &cb);
  Geometry->Models().FinishBake();
  Log::Info("render", "impostor_baked",
            {{"cells", (double)(ModelDraw::kCells * ModelDraw::kCells)},
             {"cellPx", (double)ModelDraw::kCellSize},
             {"atlasMb", 2.0 * (double)(ModelDraw::kCells * ModelDraw::kCellSize) *
                             (double)(ModelDraw::kCells * ModelDraw::kCellSize) * 4.0 / 1048576.0}});
}

/* WHAT AN ATTACHMENT IS CLEARED TO, and each of these is a statement rather than a habit: the scene
 * target's clear is what a pixel nothing drew carries, the velocity sentinel says "nothing dynamic
 * wrote this", the occlusion clear says "unoccluded" and the reversed-Z depth clears to the far
 * plane. */
namespace {

wgpu::Color ClearOf(Resource resource) {
  switch (resource) {
    case Resource::SceneVelocity: return {kVelocityStatic, kVelocityStatic, 0, 0};
    case Resource::AoBuffer: return {1, 1, 1, 1};
    default: return {0, 0, 0, 1};
  }
}

} // namespace

/* THE ONE PLACE A PASS DESCRIPTOR IS BUILT, and every field of it comes from the plan: the target
 * set is the union of what the pass's stages write and contribute, in the catalogue's own order, so
 * a pipeline's attachment order and a pass's attachment order cannot disagree. */
void Renderer::EncodePass(wgpu::CommandEncoder &enc, size_t pass, const FrameContext &ctx) {
  const RenderPlan::Pass &declared = Plan_->Passes()[pass];
  if (declared.Kind == PassKind::Compute) {
    wgpu::ComputePassDescriptor cpd{};
    cpd.timestampWrites = GpuTime.Writes((int)pass);
    wgpu::ComputePassEncoder cp = enc.BeginComputePass(&cpd);
    for (size_t at = 0; at < declared.Count; ++at)
      EncodeStage(Plan_->Order()[declared.First + at], ctx, cp);
    cp.End();
    return;
  }

  wgpu::RenderPassColorAttachment colours[kMaxColourAttachments] = {};
  uint32_t colourCount = 0;
  for (const Resource target : declared.Colours) {
    colours[colourCount].view = View(target);
    colours[colourCount].loadOp = wgpu::LoadOp::Clear;
    colours[colourCount].storeOp = wgpu::StoreOp::Store;
    colours[colourCount].clearValue = ClearOf(target);
    colourCount++;
  }
  wgpu::RenderPassDepthStencilAttachment depth{};
  if (declared.Depth != kNoEdge) {
    depth.view = View(declared.Depth);
    depth.depthLoadOp = wgpu::LoadOp::Clear;
    depth.depthStoreOp = wgpu::StoreOp::Store;
    /* The scene is reversed-Z and clears to the far plane at 0; the shadow atlas is a plain [0,1]
     * ortho depth and clears to 1. */
    depth.depthClearValue = declared.Depth == Resource::ShadowAtlas ? 1.0f : 0.0f;
  }

  wgpu::RenderPassDescriptor rp{};
  rp.colorAttachmentCount = colourCount;
  rp.colorAttachments = colourCount ? colours : nullptr;
  rp.depthStencilAttachment = declared.Depth != kNoEdge ? &depth : nullptr;
  rp.timestampWrites = GpuTime.Writes((int)pass);
  wgpu::RenderPassEncoder encoder = enc.BeginRenderPass(&rp);
  for (size_t at = 0; at < declared.Count; ++at)
    EncodeStage(Plan_->Order()[declared.First + at], ctx, encoder);
  encoder.End();
}

void Renderer::RenderFrame(void) {
#ifdef FB_GPU_NOOP
  return;   /* bisect stage 2: totally inert frames — init-side death vs frame-side */
#endif
  if (!DeviceReady || DeviceLost) return;
#ifdef FB_GPU_BISECT
  FrameNo++;
  return;   /* bisect: acquire only — does the device still die? */
#endif

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

  /* THE SAMPLE POSITION MOVES BEFORE THE MATRIX IS BUILT, and both halves of the pair read the same
   * two numbers afterwards — the projection and the atmosphere's ray reconstruction. */
  /* FB_TAA=0 RETIRES THE PAIR AT RUNTIME: no sub-pixel offset and no history, so the resolve copies
   * this frame through unchanged. It is the measurement instrument the deletion question needs — the
   * picture is exactly the one a tree without TaaStage produces, off the same binary. */
  if (!TaaOn) Jitter.Disarm();
  else Jitter.Advance();
  const float jitNdcX = 2.0f * Jitter.PixelX() / (float)Width;
  const float jitNdcY = 2.0f * Jitter.PixelY() / (float)Height;

  float u[20];
  MvpCamRel(u, right, camUp, fwd, Width, Height, FovDeg, jitNdcX, jitNdcY, OrthoM);
  /* ONE sun for terrain diffuse and atmosphere, so sky and ground agree, and it is the ephemeris
   * sun of the scene's declared instant — there is no second, time-independent one to choose. */
  const double elDeg = SceneState.Env.SunElDeg, azDeg = SceneState.Env.SunAzDeg;
  const double el = elDeg * 3.14159265 / 180.0, az = azDeg * 3.14159265 / 180.0;
  const double ce = std::cos(el), se = std::sin(el), caz = std::cos(az), saz = std::sin(az);
  double sun[3];
  for (int a = 0; a < 3; a++) sun[a] = up[a] * se + (north[a] * caz + east[a] * saz) * ce;
  Norm3(sun);
  u[16] = (float)sun[0]; u[17] = (float)sun[1]; u[18] = (float)sun[2]; u[19] = 0;
  double sunElDeg = std::asin(std::max(-1.0, std::min(1.0, sun[0] * up[0] + sun[1] * up[1] + sun[2] * up[2]))) * 180.0 / 3.14159265;
  const double dayF = DaylightFactor(sunElDeg);
  double moon[3];
  {
    double mel = SceneState.Env.MoonElDeg * 3.14159265 / 180.0, maz = SceneState.Env.MoonAzDeg * 3.14159265 / 180.0;
    double cme = std::cos(mel);
    for (int a = 0; a < 3; a++)
      moon[a] = up[a] * std::sin(mel) + (north[a] * std::cos(maz) + east[a] * std::sin(maz)) * cme;
    Norm3(moon);
  }
  /* There is ONE cloud field and CloudLayerStage draws it — as a march or as a sheet, both out of
*/
  if (Plan_->Holds(Resource::AtmosphereUniform))
    UpdateAtmosphere(eye, sun, right, camUp, fwd, moon, dayF, SceneState.Env.MoonPhase);
  if (Plan_->Holds(Stage::Stars)) Stars->Update(SkyClock);

  /* The shared per-frame state every stage's Encode() reads; built before the cloud update so
   * CloudLayerStage::Update() can read it too. */
  FrameContext ctx{};
  for (int a = 0; a < 3; a++) { ctx.Eye[a] = eye[a]; ctx.Fwd[a] = fwd[a]; ctx.Right[a] = right[a]; ctx.CamUp[a] = camUp[a]; ctx.Up[a] = up[a]; }
  for (int i = 0; i < 20; i++) ctx.Mvp20[i] = u[i];
  for (int a = 0; a < 3; a++) { ctx.SunDir[a] = sun[a]; ctx.MoonDir[a] = moon[a]; }
  ctx.DayFactor = dayF; ctx.MoonPhase = SceneState.Env.MoonPhase;
  ctx.SkyClock = SkyClock; ctx.DayFade = (float)dayF;
  ctx.CloudCover = SceneState.Env.CloudCover;
  ctx.CloudLow = SceneState.Env.CloudLow; ctx.CloudMid = SceneState.Env.CloudMid; ctx.CloudHigh = SceneState.Env.CloudHigh;
  ctx.CloudBaseAGL = SceneState.Env.CloudBaseAglM; ctx.AltM = SceneState.Platform.AltM;
  ctx.FrameNo = FrameNo; ctx.Width = Width; ctx.Height = Height;
  ctx.FovDeg = FovDeg;
  /* THE PREVIOUS FRAME, as the three things a motion vector needs. On the first frame the previous
   * matrix IS this one and the history is declared invalid, so nothing is blended against a buffer
   * that was never written. */
  for (int i = 0; i < 16; i++) ctx.PrevMvp16[i] = HaveHistory ? PrevMvp[i] : u[i];
  for (int a = 0; a < 3; a++) ctx.EyeDeltaM[a] = HaveHistory ? (float)(eye[a] - PrevEye[a]) : 0.0f;
  ctx.JitterNdc[0] = jitNdcX;
  ctx.JitterNdc[1] = jitNdcY;
  ctx.PrevJitterNdc[0] = 2.0f * Jitter.PrevPixelX() / (float)Width;
  ctx.PrevJitterNdc[1] = 2.0f * Jitter.PrevPixelY() / (float)Height;
  ctx.HistoryValid = HaveHistory && TaaOn;
  /* The renderer's OWN frame clock (the `t` above), not a wall clock: adaptation must not depend on
   * how fast the bench happens to run (CLAUDE.md, Prinzip 5). */
  ctx.Dt = 1.0f / 60.0f;

  /* THE CASCADE MATRICES, built once a frame because every receiver reads the same uniform. The
   * atlas is four viewports into one depth target, so the plan carries one pass however many
   * cascades there are. */
  if (Plan_->Holds(Stage::ShadowMap)) {
    Shadow->SetCasters(Geometry->Buildings().CasterBuffer(),
                       Geometry->Buildings().CasterIndexBuffer(),
                       Geometry->Buildings().CasterVertexCount(),
                       Geometry->Buildings().CasterClusters(),
                       Geometry->Buildings().CasterClusterCount(),
                       Geometry->Buildings().CasterAnchor());
    /* The terrain casts too, and at a low sun that is the larger half of the shadow: a ridge shadows
     * a whole valley while a building shadows a street. */
    Geometry->Terrain().CollectCasters(TerrainCasters);
    ShadowTerrain.resize(TerrainCasters.size());
    for (size_t i = 0; i < TerrainCasters.size(); i++) {
      ShadowTerrain[i].Vtx = TerrainCasters[i].Vtx;
      ShadowTerrain[i].Idx = TerrainCasters[i].Idx;
      ShadowTerrain[i].NVerts = TerrainCasters[i].NVerts;
      ShadowTerrain[i].NIdx = TerrainCasters[i].NIdx;
      ShadowTerrain[i].Clusters = TerrainCasters[i].Clusters;
      ShadowTerrain[i].NClusters = TerrainCasters[i].NClusters;
      for (int axis = 0; axis < 3; axis++) {
        ShadowTerrain[i].Origin[axis] = TerrainCasters[i].Origin[axis];
        ShadowTerrain[i].BoundCtr[axis] = TerrainCasters[i].BoundCtr[axis];
      }
      ShadowTerrain[i].BoundRad = TerrainCasters[i].BoundRad;
    }
    Shadow->SetTerrainCasters(ShadowTerrain);
    Shadow->Update(ctx);
    Queue.WriteBuffer(CsmBuf, 0, Shadow->CsmUniform(), kShadowUniFloats * sizeof(float));
  }
  Geometry->SetSun(ctx.SunDir, ctx.Up, NightAmbient(ctx));
  Geometry->OpenCut(ctx);

  GpuTime.BeginFrame();
  wgpu::CommandEncoder enc = Device.CreateCommandEncoder();

  /* THE PASSES ARE THE COMPILER'S. There is no tally here and no fixed enumeration to keep one
   * against: the count, the order and the attachment set of every pass are what Compile derived. */
  for (size_t pass = 0; pass < Plan_->Passes().size(); ++pass) EncodePass(enc, pass, ctx);

  GpuTime.Resolve(enc);
  wgpu::CommandBuffer cmd = enc.Finish();
  Queue.Submit(1, &cmd);
  GpuTime.Poll();

  /* THIS FRAME BECOMES THE PREVIOUS ONE. The matrix is stored WITH its jitter: the resolve subtracts
   * the difference of the two jitters once, so both the depth reprojection and the velocity a moving
   * vertex wrote are corrected by the same term and can never drift apart. */
  for (int i = 0; i < 16; i++) PrevMvp[i] = u[i];
  for (int a = 0; a < 3; a++) PrevEye[a] = eye[a];
  HaveHistory = true;

  /* 2-phase-commit assertion (once/sec): no frame should ever have drawn an uncommitted layer. */
  if (FrameNo % 60 == 0) {
    long notReady = Geometry->Terrain().GetNotReadyDraws(), wrongMode = Geometry->Terrain().GetWrongModeDraws(), black = Geometry->Terrain().GetBlackDraws();
    bool violation = notReady || wrongMode || black;
    Log::Debug("render", "present", {{"notReadyDraws", (int)notReady}, {"wrongModeDraws", (int)wrongMode},
                                       {"blackDraws", (int)black}, {"violation", violation},
                                       {"bundleRecords", (int)Geometry->Terrain().GetBundleRecords()}});
  }
}

/* Ready when the queue has retired everything submitted so far. Watched once and polled after, so
 * the encoder is never re-entered from inside the answer. */
ReadState Renderer::GpuIdle(void) {
  if (!DeviceUsable()) return ReadState::Failed;
  if (!WorkWatched) {
    WorkWatched = true;
    WorkRetired = false;
    Queue.OnSubmittedWorkDone(wgpu::CallbackMode::AllowProcessEvents,
        [this](wgpu::QueueWorkDoneStatus, wgpu::StringView) { WorkRetired = true; });
  }
  Instance.ProcessEvents();
  if (!WorkRetired) return ReadState::Pending;
  WorkWatched = false;
  return ReadState::Ready;
}

/* THE SOURCE IS FrameTex AND NOT THE TARGET. Only the offscreen path had a copyable target — on a
 * canvas the swapchain texture is the browser's and carries no CopySrc — so a browser run threw
 * `Failed to read the 'texture' property` on its first readback and the run died there, which is
 * every product the browser never delivered. FrameTex exists in BOTH translations, is the same
 * declared-size picture the present pass reads, and carries CopySrc; reading it makes the two
 * clients read the same texture instead of two different ones. */
ReadState Renderer::ReadPixels(std::vector<uint8_t> &rgba) {
  if (!DeviceUsable() || !FrameTex) return ReadState::Failed;
  if (PixelRead.Idle())
    PixelRead.FromTexture(Device, Queue, FrameTex, wgpu::TextureAspect::All, (uint32_t)Width,
                          (uint32_t)Height, 4u);
  const ReadState st = PixelRead.Poll(Instance);
  if (st == ReadState::Pending) return st;
  if (st == ReadState::Failed) {
    PixelRead.Release();
    return st;
  }
  const uint32_t unpaddedRow = (uint32_t)Width * 4u;
  const uint32_t paddedRow = PixelRead.RowBytes();
  const uint8_t *mapped = PixelRead.Rows();
  rgba.resize((size_t)unpaddedRow * (size_t)Height);
  for (int y = 0; y < Height; y++)
    memcpy(&rgba[(size_t)y * unpaddedRow], mapped + (size_t)y * paddedRow, unpaddedRow);
  PixelRead.Release();
  /* Chrome's canvas offers only BGRA (measured, `swap=27`); native Dawn takes RGBA. A PNG writer
   * takes one order, so the swap happens once, here, rather than in every consumer. */
  if (SurfaceFormat == wgpu::TextureFormat::BGRA8Unorm ||
      SurfaceFormat == wgpu::TextureFormat::BGRA8UnormSrgb)
    for (size_t i = 0; i + 2 < rgba.size(); i += 4) std::swap(rgba[i], rgba[i + 2]);
  return ReadState::Ready;
}

ReadState Renderer::ReadDepth(std::vector<float> &depth) {
  if (!DeviceUsable() || !DepthTex) return ReadState::Failed;
  if (DepthRead.Idle())
    DepthRead.FromTexture(Device, Queue, DepthTex, wgpu::TextureAspect::DepthOnly, (uint32_t)Width,
                          (uint32_t)Height, 4u);
  const ReadState st = DepthRead.Poll(Instance);
  if (st == ReadState::Pending) return st;
  if (st == ReadState::Failed) {
    DepthRead.Release();
    return st;
  }
  const uint32_t unpaddedRow = (uint32_t)Width * 4u;
  const uint32_t paddedRow = DepthRead.RowBytes();
  const uint8_t *mapped = DepthRead.Rows();
  depth.resize((size_t)Width * (size_t)Height);
  for (int y = 0; y < Height; y++)
    memcpy(&depth[(size_t)y * Width], mapped + (size_t)y * paddedRow, unpaddedRow);
  DepthRead.Release();
  return ReadState::Ready;
}

namespace {

/* binary16 -> f32, by the format's own rules and not by a library: a subnormal, an infinity and a
 * NaN all appear in a render target and a conversion that flushed one would be a second thing that
 * can be wrong inside a measurement. */
float HalfToFloat(uint16_t bits) {
  const uint32_t sign = (uint32_t)(bits & 0x8000u) << 16;
  uint32_t exponent = (bits >> 10) & 0x1Fu;
  uint32_t mantissa = bits & 0x3FFu;
  uint32_t assembled = 0;
  if (exponent == 0) {
    if (mantissa != 0) {
      int shift = 0;
      while ((mantissa & 0x400u) == 0) {
        mantissa <<= 1;
        ++shift;
      }
      mantissa &= 0x3FFu;
      assembled = ((uint32_t)(127 - 15 - shift + 1) << 23) | (mantissa << 13);
    }
  } else if (exponent == 0x1Fu) {
    assembled = 0x7F800000u | (mantissa << 13);
  } else {
    assembled = ((exponent + 127 - 15) << 23) | (mantissa << 13);
  }
  assembled |= sign;
  float value = 0;
  memcpy(&value, &assembled, sizeof value);
  return value;
}

} // namespace

/* THE SCENE-REFERRED LINEAR TAP. Same shape as ReadDepth and the same cost model: it copies a
 * texture that already exists, on the frames a caller asks for and never on a frame nobody asks.
 * `SceneLinear` may be the resolve's own attachment or, through the plan's alias, the scene target
 * it falls back to -- and the plan publishes which, so a comparison knows which image it got. */
ReadState Renderer::ReadSceneLinear(std::vector<float> &rgba) {
  const wgpu::Texture &source =
      Plan_->Bound(Resource::SceneLinear) == Resource::SceneLinear ? Taa->OutputTexture(FrameNo)
                                                                    : HdrTex;
  if (!DeviceUsable() || !source) return ReadState::Failed;
  const bool wide = Plan_->Format(Resource::SceneLinear) == TexelFormat::Rgba32Float;
  const uint32_t texel = wide ? 16u : 8u;
  if (LinearRead.Idle())
    LinearRead.FromTexture(Device, Queue, source, wgpu::TextureAspect::All, (uint32_t)Width,
                           (uint32_t)Height, texel);
  const ReadState st = LinearRead.Poll(Instance);
  if (st == ReadState::Pending) return st;
  if (st == ReadState::Failed) {
    LinearRead.Release();
    return st;
  }
  const uint32_t paddedRow = LinearRead.RowBytes();
  const uint8_t *mapped = LinearRead.Rows();
  rgba.resize((size_t)Width * (size_t)Height * 4u);
  for (int y = 0; y < Height; y++) {
    const uint8_t *row = mapped + (size_t)y * paddedRow;
    if (wide) {
      memcpy(rgba.data() + (size_t)y * (size_t)Width * 4u, row, (size_t)Width * 4u * sizeof(float));
      continue;
    }
    /* The half path widens rather than reinterpreting: binary16 to f32 is exact, so one currency
     * leaves this call and the plan is what says which storage produced it. */
    for (size_t component = 0; component < (size_t)Width * 4u; ++component) {
      uint16_t bits = 0;
      memcpy(&bits, row + component * sizeof(uint16_t), sizeof bits);
      rgba[(size_t)y * (size_t)Width * 4u + component] = HalfToFloat(bits);
    }
  }
  LinearRead.Release();
  return ReadState::Ready;
}

namespace {

/* The two meter buffers differ only in which buffer and how many bytes, and both answer into a
 * plain float array — so the poll is written once. */
[[nodiscard]] ReadState TakeFloats(Readback &read, const wgpu::Instance &instance, const wgpu::Device &device,
                     const wgpu::Queue &queue, const wgpu::Buffer &src, uint64_t bytes,
                     float *out) {
  if (read.Idle()) read.FromBuffer(device, queue, src, bytes);
  const ReadState st = read.Poll(instance);
  if (st == ReadState::Pending) return st;
  if (st == ReadState::Ready) memcpy(out, read.Rows(), (size_t)bytes);
  read.Release();
  return st;
}

}  // namespace

ReadState Renderer::ReadIrradiance(float out[IrradianceStage::kFloats]) {
  if (!DeviceUsable() || !IrrBuf) return ReadState::Failed;
  return TakeFloats(IrrRead, Instance, Device, Queue, IrrBuf, IrradianceStage::kBufferBytes, out);
}

ReadState Renderer::ReadExposure(float out[ExposureStage::kMeterFloats]) {
  if (!DeviceUsable() || !MeterBuf) return ReadState::Failed;
  return TakeFloats(MeterRead, Instance, Device, Queue, MeterBuf, ExposureStage::kMeterBytes, out);
}

} // namespace outshine::Render
