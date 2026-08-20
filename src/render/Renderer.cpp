#include "Renderer.h"

#include <cmath>
#include <cstring>

#include <SDL3/SDL.h>

#include "Log.h"
#include "stages/SceneTargets.h"

namespace outshine::Render {
namespace {

#ifdef OUTSHINE_GPU_VALIDATION
constexpr bool kGpuValidation = true;
#else
constexpr bool kGpuValidation = false;
#endif
}

namespace {

void MvpCamRel(float *m, const double R[3], const double Uc[3], const double F[3], double w, double h,
               float fovDeg, float orthoM, float jitterX, float jitterY, float nearM) {
  const float fov = fovDeg * 3.14159265f / 180.0f, asp = (float)w / (float)h;
  const float zn = nearM;
  const float f = 1.0f / std::tan(fov / 2.0f);
  const float v[16] = {(float)R[0], (float)Uc[0], -(float)F[0], 0,
                       (float)R[1], (float)Uc[1], -(float)F[1], 0,
                       (float)R[2], (float)Uc[2], -(float)F[2], 0,
                       0,           0,            0,            1};

  float p[16] = {f / asp, 0, 0, 0, 0, f, 0, 0, 0, 0, 0, -1, 0, 0, zn, 0};

  const float ndcX = w > 0 ? 2.0f * jitterX / (float)w : 0.0f;
  const float ndcY = h > 0 ? 2.0f * jitterY / (float)h : 0.0f;
  p[8] = -ndcX;
  p[9] = -ndcY;
  if (orthoM > 0.0f) {

    const float hw = 0.5f * orthoM * asp, hh = 0.5f * orthoM;
    const float zf = 60000.0f, rz = 1.0f / (zf - zn);
    float q[16] = {1.0f / hw, 0, 0, 0, 0, 1.0f / hh, 0, 0, 0, 0, rz, 0, 0, 0, zf * rz, 1};
    q[12] = ndcX;
    q[13] = ndcY;
    for (int i = 0; i < 16; i++) { p[i] = q[i]; }
  }
  for (int c = 0; c < 4; c++) {
    for (int r = 0; r < 4; r++) {
      m[c * 4 + r] = 0;
      for (int k = 0; k < 4; k++) { m[c * 4 + r] += p[k * 4 + r] * v[c * 4 + k]; }
    }
  }
}

SDL_GPUTextureFormat FormatOf(TexelFormat declared) {
  switch (declared) {
    case TexelFormat::Handle: return SDL_GPU_TEXTUREFORMAT_INVALID;
    case TexelFormat::Rgba16Float: return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    case TexelFormat::Rgba32Float: return SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    case TexelFormat::Rg16Float: return SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT;
    case TexelFormat::R8Unorm: return SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    case TexelFormat::Rgba8UnormSrgb: return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    case TexelFormat::Depth32Float: return SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
  }
  return SDL_GPU_TEXTUREFORMAT_INVALID;
}

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
  std::memcpy(&value, &assembled, sizeof value);
  return value;
}

}

void Renderer::SetCameraBasis(const double eye[3], const double fwd[3], const double right[3],
                              const double up[3]) {
  for (int axis = 0; axis < 3; axis++) {
    Eye[axis] = eye[axis];
    Fwd[axis] = fwd[axis];
    Right[axis] = right[axis];
    Up[axis] = up[axis];
  }
  CameraFull = true;
}

bool Renderer::Executable(Stage stage) {
  switch (stage) {
    case Stage::Subjects:
    case Stage::Tonemap:
      return true;
    case Stage::Overlay:
    case Stage::Present:
      return true;
    case Stage::TemporalResolve:
      return true;
    case Stage::SubjectsTransmissive:
    case Stage::CompositeTransmission:
      return true;
    case Stage::MediumTransmittance:
    case Stage::MediumMultiScatter:
    case Stage::MediumRadiance:
    case Stage::Irradiance:
    case Stage::AutoExposure:
    case Stage::LightVisibility:
    case Stage::Sky:
    case Stage::Sun:
    case Stage::Moon:
    case Stage::Stars:
    case Stage::Terrain:
    case Stage::Buildings:
    case Stage::Water:
    case Stage::Models:
    case Stage::AmbientOcclusion:
    case Stage::kCount:
      return false;
  }
  return false;
}

void Renderer::Init(int width, int height, std::shared_ptr<const RenderPlan> plan) {
  Plan_ = std::move(plan);
  Width = width;
  Height = height;

  for (const Stage stage : Plan_->Order()) {
    if (Executable(stage)) { continue; }
    Log::Error("render", "stage_not_executed", {{"stage", Row(stage).Name}});
    return;
  }

  Ready = false;
  if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
    Log::Error("render", "no_video", {{"msg", SDL_GetError()}});
    return;
  }

  if (!Device_) {
    SDL_GPUDevice *device =
        SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, kGpuValidation, nullptr);
    if (!device) {
      Log::Error("render", "no_device", {{"msg", SDL_GetError()}});
      return;
    }
    Device_ = OwnedDevice(device);
  }

  SDL_WaitForGPUIdle(Device_.Get());
  SDL_GPUDevice *const device = Device_.Get();
  Handles.Device = device;
  Handles.HdrFormat = FormatOf(Plan_->Format(Resource::SceneHdr));
  Handles.SurfaceFormat = FormatOf(Plan_->Format(Resource::FrameTex));
  Handles.Width = Width;
  Handles.Height = Height;

  for (const RenderPlan::Pass &pass : Plan_->Passes()) {
    if (pass.Kind == PassKind::Compute || pass.Depth == kNoEdge) { continue; }
    Handles.SceneColours = pass.Colours;
    break;
  }
  const auto coloursOfPassWith = [this](Stage wanted) {
    for (const RenderPlan::Pass &pass : Plan_->Passes()) {
      for (size_t at = pass.First; at < pass.First + pass.Count; ++at) {
        if (Plan_->Order()[at] == wanted) { return pass.Colours; }
      }
    }
    return Handles.SceneColours;
  };
  Handles.FiltersFloat32 =
      SDL_GPUTextureSupportsFormat(device, SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
                                   SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_SAMPLER);

  for (size_t r = 0; r < kResourceCount; ++r) {
    const Resource id = static_cast<Resource>(r);
    if (Plan_->Holds(id)) { Create(id); }
  }

  for (const Stage stage : Plan_->Order()) {
    if (stage == Stage::SubjectsTransmissive) { DrawsGlass_ = true; }
  }
  for (const Stage stage : Plan_->Order()) {
    std::string why;
    Handles.SceneColours = coloursOfPassWith(stage);
    if (Configure(stage, why)) { continue; }
    Log::Error("render", "stage_not_configured", {{"stage", Row(stage).Name}, {"msg", why}});
    return;
  }
  Ready = true;

  Log::Info("render", "device_ready",
            {{"width", Width},
             {"height", Height},
             {"driver", SDL_GetGPUDeviceDriver(device)},
             {"plan", Plan_->Digest()},
             {"passes", Plan_->PassCount()},
             {"stages", (int)Plan_->Order().size()},
             {"f32filter", Handles.FiltersFloat32}});
  for (const std::string &merge : Plan_->Merges()) {
    Log::Info("render", "plan_merge", {{"merge", merge}});
  }
  for (const std::string &alias : Plan_->Aliases()) {
    Log::Info("render", "plan_alias", {{"alias", alias}});
  }
}

void Renderer::Create(Resource resource) {
  const auto target = [&](Resource of, SDL_GPUTextureUsageFlags usage) {
    SDL_GPUTextureCreateInfo wanted{};
    wanted.type = SDL_GPU_TEXTURETYPE_2D;
    wanted.format = FormatOf(Plan_->Format(of));
    wanted.usage = usage;
    wanted.width = (uint32_t)Width;
    wanted.height = (uint32_t)Height;
    wanted.layer_count_or_depth = 1;
    wanted.num_levels = 1;
    wanted.sample_count = SDL_GPU_SAMPLECOUNT_1;
    return OwnedTexture(Device_.Get(), SDL_CreateGPUTexture(Device_.Get(), &wanted));
  };
  const SDL_GPUTextureUsageFlags colour =
      SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;

  switch (resource) {
    case Resource::LinearSampler: {
      SDL_GPUSamplerCreateInfo wanted{};
      wanted.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
      wanted.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
      wanted.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
      wanted.min_filter = SDL_GPU_FILTER_LINEAR;
      wanted.mag_filter = SDL_GPU_FILTER_LINEAR;
      wanted.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
      Samp = OwnedSampler(Device_.Get(), SDL_CreateGPUSampler(Device_.Get(), &wanted));
      return;
    }

    case Resource::OverlayAtlas: return;
    case Resource::SceneHdr: HdrTex = target(resource, colour); return;
    case Resource::SceneTransmissive: TransmissiveTex = target(resource, colour); return;
    case Resource::SceneComposited: CompositedTex = target(resource, colour); return;
    case Resource::SceneVelocity: VelTex = target(resource, colour); return;
    case Resource::SceneShadingNormal: ShadingNormalTex = target(resource, colour); return;
    case Resource::SceneSurfaceIdentity: SurfaceIdentityTex = target(resource, colour); return;
    case Resource::SceneDepth:

      DepthTex = target(resource, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
                                      SDL_GPU_TEXTUREUSAGE_SAMPLER);
      return;
    case Resource::FrameTex: FrameTex = target(resource, colour); return;

    case Resource::Surface: return;
    case Resource::LutSampler:
    case Resource::AtmosphereUniform:
    case Resource::CascadeUniform:
    case Resource::VegetationTable:
    case Resource::TransmittanceLut:
    case Resource::MultiScatterLut:
    case Resource::SkyViewLut:
    case Resource::IrradianceBuffer:
    case Resource::Meter:
    case Resource::ShadowAtlas:
    case Resource::AoBuffer:
      return;

    case Resource::SceneLinear:

      LinearTex_[0] = target(resource, colour);
      LinearTex_[1] = target(resource, colour);
      LinearAt_ = 0;
      HistoryHeld_ = false;
      return;
    case Resource::kCount:
      return;
  }
}

SDL_GPUTexture *Renderer::Target(Resource resource) const {
  switch (resource) {

    case Resource::OverlayAtlas: return nullptr;
    case Resource::SceneHdr: return HdrTex.Get();
    case Resource::SceneTransmissive: return TransmissiveTex.Get();
    case Resource::SceneComposited: return CompositedTex.Get();
    case Resource::SceneVelocity: return VelTex.Get();
    case Resource::SceneShadingNormal: return ShadingNormalTex.Get();
    case Resource::SceneSurfaceIdentity: return SurfaceIdentityTex.Get();
    case Resource::SceneDepth: return DepthTex.Get();
    case Resource::FrameTex: return FrameTex.Get();

    case Resource::Surface: return HostSurface_;
    case Resource::LinearSampler:
    case Resource::LutSampler:
    case Resource::AtmosphereUniform:
    case Resource::CascadeUniform:
    case Resource::VegetationTable:
    case Resource::TransmittanceLut:
    case Resource::MultiScatterLut:
    case Resource::SkyViewLut:
    case Resource::IrradianceBuffer:
    case Resource::Meter:
    case Resource::ShadowAtlas:
    case Resource::AoBuffer:
      return nullptr;

    case Resource::SceneLinear:
      return LinearTex_[LinearAt_].Get();
    case Resource::kCount:
      return nullptr;
  }
  return nullptr;
}

DisplayOptions Renderer::Display(void) const {
  DisplayOptions options;
  options.Exposure = Plan_->Exposure();
  options.Curve = Plan_->Display();

  options.Temporal = Plan_->Holds(Stage::TemporalResolve);
  return options;
}

Renderer::Placed Renderer::PictureRect(void) const {
  Placed out;
  out.LeftPx = 0;
  out.TopPx = 0;
  out.WidthPx = (double)Width;
  out.HeightPx = (double)Height;
  if (RegionW_ > 0 && RegionH_ > 0) {
    out.LeftPx = RegionX_ * (double)Width;
    out.TopPx = RegionY_ * (double)Height;
    out.WidthPx = RegionW_ * (double)Width;
    out.HeightPx = RegionH_ * (double)Height;
  }
  if (RegionAspect_ > 0 && out.WidthPx > 0 && out.HeightPx > 0) {
    const double fitted = out.WidthPx / out.HeightPx > RegionAspect_ ? out.HeightPx * RegionAspect_
                                                                    : out.WidthPx;
    const double tall = fitted / RegionAspect_;
    out.LeftPx += (out.WidthPx - fitted) / 2.0;
    out.TopPx += (out.HeightPx - tall) / 2.0;
    out.WidthPx = fitted;
    out.HeightPx = tall;
  }
  return out;
}

double Renderer::PictureW(void) const { return PictureRect().WidthPx; }
double Renderer::PictureH(void) const { return PictureRect().HeightPx; }

SDL_GPUTextureFormat Renderer::SurfaceFormat(void) const {
  return Plan_ ? FormatOf(Plan_->Format(Resource::Surface)) : SDL_GPU_TEXTUREFORMAT_INVALID;
}

SDL_GPUTexture *Renderer::LinearSource(void) const {
  return Target(Plan_->Bound(Resource::SceneLinear));
}

bool Renderer::Configure(Stage stage, std::string &error) {
  switch (stage) {
    case Stage::Subjects:
      if (DrawsGlass_) { Subjects_.GlassIsDrawnElsewhere(); }
      return Subjects_.Configure(Handles, error);
    case Stage::SubjectsTransmissive:

      Glass_.SeeThroughTo(HdrTex.Get(), Samp.Get());
      return Glass_.Configure(Handles, error);
    case Stage::CompositeTransmission:
      return CompositeTransmission_.Configure(Handles, HdrTex.Get(), TransmissiveTex.Get(),
                                              Samp.Get(),
                                              FormatOf(Plan_->Format(Resource::SceneComposited)),
                                              error);
    case Stage::TemporalResolve:

      return true;
    case Stage::Overlay:
      return Overlay_.Configure(Handles, Samp.Get(), FormatOf(Plan_->Format(Resource::FrameTex)),
                                error);
    case Stage::Present:
      return Present_.Configure(Handles, FrameTex.Get(), Samp.Get(), error);
    case Stage::Tonemap:
      return Tonemap_.Configure(Handles, Target(Plan_->Bound(Resource::SceneComposited)),
                                DepthTex.Get(), Samp.Get(),
                                FormatOf(Plan_->Format(Resource::SceneLinear)), Display(), error);
    case Stage::MediumTransmittance:
    case Stage::MediumMultiScatter:
    case Stage::MediumRadiance:
    case Stage::Irradiance:
    case Stage::AutoExposure:
    case Stage::LightVisibility:
    case Stage::Sky:
    case Stage::Sun:
    case Stage::Moon:
    case Stage::Stars:
    case Stage::Terrain:
    case Stage::Buildings:
    case Stage::Water:
    case Stage::Models:
    case Stage::AmbientOcclusion:
    case Stage::kCount:
      error = "this device layer does not execute the stage";
      return false;
  }
  error = "this device layer does not execute the stage";
  return false;
}

void Renderer::EncodeStage(Stage stage, const PassRecording &into) {
  FrameContext ctx{};
  for (int axis = 0; axis < 3; axis++) { ctx.Eye[axis] = Eye[axis]; }

  MvpCamRel(ctx.Mvp16, Right, Up, Fwd, PictureW(), PictureH(), FovDeg, OrthoM, Jitter_[0],
            Jitter_[1], NearM);
  for (int axis = 0; axis < 3; axis++) {
    ctx.PrevEye[axis] = Submitted ? PrevEye[axis] : ctx.Eye[axis];
  }
  for (int at = 0; at < 16; at++) {
    ctx.PrevMvp16[at] = Submitted ? PrevMvp16[at] : ctx.Mvp16[at];
  }

  const auto within = [&](bool picture) {

    SDL_GPUViewport where{};
    const Placed rect = PictureRect();
    where.x = picture ? (float)rect.LeftPx : 0.0f;
    where.y = picture ? (float)rect.TopPx : 0.0f;
    where.w = picture ? (float)rect.WidthPx : (float)Width;
    where.h = picture ? (float)rect.HeightPx : (float)Height;
    where.min_depth = 0.0f;
    where.max_depth = 1.0f;
    if (into.Pass != nullptr) { SDL_SetGPUViewport(into.Pass, &where); }
  };

  switch (stage) {
    case Stage::Subjects:
      within(true);
      Subjects_.Encode(ctx, into);
      return;
    case Stage::SubjectsTransmissive:
      within(true);
      Glass_.Encode(ctx, into);
      return;
    case Stage::CompositeTransmission:
      within(true);
      CompositeTransmission_.Encode(ctx, into);
      return;

    case Stage::TemporalResolve: return;
    case Stage::Tonemap: {

      Tonemap_.Bind(Target(Plan_->Bound(Resource::SceneComposited)));
      const float delta[2] = {Jitter_[0] - PrevJitter_[0], Jitter_[1] - PrevJitter_[1]};
      Tonemap_.BindTemporal(LinearTex_[1 - LinearAt_].Get(), VelTex.Get(), Width, Height, delta,
                            HistoryHeld_);
      within(true);
      Tonemap_.Encode(ctx, into);
      return;
    }

    case Stage::Overlay:
      within(false);
      Overlay_.Bind(Width, Height);
      Overlay_.Encode(ctx, into);
      return;

    case Stage::Present:

      {
        std::string why;
        if (!Present_.For(Handles, SurfaceFormat(), why)) {
          Log::Error("render", "present_not_built", {{"msg", why}});
          return;
        }
      }
      within(false);
      Present_.Encode(ctx, into);
      return;
    case Stage::MediumTransmittance:
    case Stage::MediumMultiScatter:
    case Stage::MediumRadiance:
    case Stage::Irradiance:
    case Stage::AutoExposure:
    case Stage::LightVisibility:
    case Stage::Sky:
    case Stage::Sun:
    case Stage::Moon:
    case Stage::Stars:
    case Stage::Terrain:
    case Stage::Buildings:
    case Stage::Water:
    case Stage::Models:
    case Stage::AmbientOcclusion:
    case Stage::kCount:
      return;
  }
}

static float RadicalInverse(int index, int base) {
  float result = 0.0f;
  float weight = 1.0f / (float)base;
  int at = index + 1;
  while (at > 0) {
    result += weight * (float)(at % base);
    at /= base;
    weight /= (float)base;
  }
  return result;
}

void Renderer::EncodePass(SDL_GPUCommandBuffer *commands, size_t pass) {
  const RenderPlan::Pass &declared = Plan_->Passes()[pass];
  SDL_GPUColorTargetInfo colours[kMaxColourAttachments] = {};
  uint32_t colourCount = 0;
  for (const Resource wanted : declared.Colours) {
    SDL_GPUColorTargetInfo &attachment = colours[colourCount++];
    attachment.texture = Target(wanted);
    attachment.load_op = SDL_GPU_LOADOP_CLEAR;
    attachment.store_op = SDL_GPU_STOREOP_STORE;

    const bool carriesCoverage = wanted == Resource::SceneHdr || wanted == Resource::SceneComposited ||
                                 wanted == Resource::SceneTransmissive ||
                                 wanted == Resource::SceneLinear;
    attachment.clear_color = wanted == Resource::SceneVelocity
                                 ? SDL_FColor{kVelocityStatic, kVelocityStatic, 0, 0}
                                 : SDL_FColor{0, 0, 0, carriesCoverage ? 0.0f : 1.0f};
  }
  SDL_GPUDepthStencilTargetInfo depth{};
  if (declared.Depth != kNoEdge) {
    depth.texture = Target(declared.Depth);
    depth.load_op = SDL_GPU_LOADOP_CLEAR;
    depth.store_op = SDL_GPU_STOREOP_STORE;
    depth.clear_depth = 0.0f;
    depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
  }
  PassRecording into{commands, SDL_BeginGPURenderPass(commands, colours, colourCount,
                                                      declared.Depth != kNoEdge ? &depth : nullptr)};
  for (size_t at = 0; at < declared.Count; ++at) {
    EncodeStage(Plan_->Order()[declared.First + at], into);
  }
  SDL_EndGPURenderPass(into.Pass);
}

void Renderer::BeginTemporalRun(void) {
  HistoryStarted_ = false;
  JitterAt_ = 0;
  Jitter_[0] = 0.0f;
  Jitter_[1] = 0.0f;
  PrevJitter_[0] = 0.0f;
  PrevJitter_[1] = 0.0f;
  LinearAt_ = 0;
  HistoryHeld_ = false;
}

void Renderer::RenderFrame(void) {
  if (!Ready || !CameraFull) { return; }

  if (Plan_->Holds(Stage::TemporalResolve)) {
    PrevJitter_[0] = Jitter_[0];
    PrevJitter_[1] = Jitter_[1];
    JitterAt_ = (JitterAt_ + 1) % kJitterPeriod;
    Jitter_[0] = RadicalInverse(JitterAt_, 2) - 0.5f;
    Jitter_[1] = RadicalInverse(JitterAt_, 3) - 0.5f;

    HistoryHeld_ = HistoryStarted_;
    HistoryStarted_ = true;
    LinearAt_ = 1 - LinearAt_;
  }
  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(Device_.Get());

  for (size_t pass = 0; pass < Plan_->Passes().size(); ++pass) { EncodePass(commands, pass); }

  if (Landed_[LandedAt_] != nullptr) {
    SDL_WaitForGPUFences(Device_.Get(), true, &Landed_[LandedAt_], 1);
    SDL_ReleaseGPUFence(Device_.Get(), Landed_[LandedAt_]);
    Landed_[LandedAt_] = nullptr;
  }
  Landed_[LandedAt_] = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
  LandedAt_ = (LandedAt_ + 1) % kFramesInFlight;
  for (int axis = 0; axis < 3; axis++) { PrevEye[axis] = Eye[axis]; }

  MvpCamRel(PrevMvp16, Right, Up, Fwd, PictureW(), PictureH(), FovDeg, OrthoM, Jitter_[0],
            Jitter_[1], NearM);
  Submitted = true;
}

void Renderer::WaitForGpu(void) {
  if (!Ready) { return; }
  SDL_WaitForGPUIdle(Device_.Get());

  for (SDL_GPUFence *&held : Landed_) {
    if (held == nullptr) { continue; }
    SDL_ReleaseGPUFence(Device_.Get(), held);
    held = nullptr;
  }
}

ReadState Renderer::ReadPixels(std::vector<uint8_t> &rgba) {
  if (!Ready || !FrameTex) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.Get(), FrameTex.Get(), (uint32_t)Width, (uint32_t)Height, 4u) !=
      ReadState::Ready) {
    return ReadState::Failed;
  }
  rgba.assign(read.Rows(), read.Rows() + (size_t)Width * (size_t)Height * 4u);
  return ReadState::Ready;
}

ReadState Renderer::ReadDepth(std::vector<float> &depth) {
  if (!Ready || !DepthTex) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.Get(), DepthTex.Get(), (uint32_t)Width, (uint32_t)Height, 4u) !=
      ReadState::Ready) {
    return ReadState::Failed;
  }
  depth.resize((size_t)Width * (size_t)Height);
  std::memcpy(depth.data(), read.Rows(), depth.size() * sizeof(float));
  return ReadState::Ready;
}

ReadState Renderer::ReadSceneLinear(std::vector<float> &rgba) {
  SDL_GPUTexture *source = LinearSource();
  if (!Ready || !source) { return ReadState::Failed; }
  const bool wide = Plan_->Format(Resource::SceneLinear) == TexelFormat::Rgba32Float;
  Readback read;
  if (read.FromTexture(Device_.Get(), source, (uint32_t)Width, (uint32_t)Height, wide ? 16u : 8u) !=
      ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components = (size_t)Width * (size_t)Height * 4u;
  rgba.resize(components);
  if (wide) {
    std::memcpy(rgba.data(), read.Rows(), components * sizeof(float));
    return ReadState::Ready;
  }

  for (size_t component = 0; component < components; ++component) {
    uint16_t bits = 0;
    std::memcpy(&bits, read.Rows() + component * sizeof(uint16_t), sizeof bits);
    rgba[component] = HalfToFloat(bits);
  }
  return ReadState::Ready;
}

ReadState Renderer::ReadShadingNormal(std::vector<float> &xyz) {
  SDL_GPUTexture *source = ShadingNormalTex.Get();
  if (!Ready || !source) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.Get(), source, (uint32_t)Width, (uint32_t)Height, 8u) !=
      ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components = (size_t)Width * (size_t)Height * 4u;
  xyz.resize(components);
  for (size_t component = 0; component < components; ++component) {
    uint16_t bits = 0;
    std::memcpy(&bits, read.Rows() + component * sizeof(uint16_t), sizeof bits);
    xyz[component] = HalfToFloat(bits);
  }
  return ReadState::Ready;
}

ReadState Renderer::ReadSceneVelocity(std::vector<float> &xy) {
  SDL_GPUTexture *source = VelTex.Get();
  if (!Ready || !source) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.Get(), source, (uint32_t)Width, (uint32_t)Height, 4u) !=
      ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components = (size_t)Width * (size_t)Height * 2u;
  xy.resize(components);
  for (size_t component = 0; component < components; ++component) {
    uint16_t bits = 0;
    std::memcpy(&bits, read.Rows() + component * sizeof(uint16_t), sizeof bits);
    xy[component] = HalfToFloat(bits);
  }
  return ReadState::Ready;
}

ReadState Renderer::ReadSurfaceIdentity(std::vector<float> &slot) {
  SDL_GPUTexture *source = SurfaceIdentityTex.Get();
  if (!Ready || !source) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.Get(), source, (uint32_t)Width, (uint32_t)Height, 16u) !=
      ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components = (size_t)Width * (size_t)Height * 4u;
  slot.resize(components);
  std::memcpy(slot.data(), read.Rows(), components * sizeof(float));
  return ReadState::Ready;
}

}
