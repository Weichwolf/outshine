#include "Heap.h"
#include "Renderer.h"

#include <numbers>
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
  const float fov = fovDeg * std::numbers::pi_v<float> / 180.0f, asp = (float)w / (float)h;
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
    Eye_[axis] = eye[axis];
    Fwd_[axis] = fwd[axis];
    Right_[axis] = right[axis];
    Up_[axis] = up[axis];
  }
  CameraFull_ = true;
}

const Renderer::Executor Renderer::kExecutors[] = {
    {Stage::MediumTransmittance, &Renderer::ConfigureMediumTransmittance,
     &Renderer::EncodeMediumTransmittance},
    {Stage::MediumMultiScatter, &Renderer::ConfigureMediumMultiScatter,
     &Renderer::EncodeMediumMultiScatter},
    {Stage::MediumRadiance, &Renderer::ConfigureMediumRadiance, &Renderer::EncodeMediumRadiance},
    {Stage::LightVisibility, &Renderer::ConfigureLightVisibility,
     &Renderer::EncodeLightVisibility},
    {Stage::Sky, &Renderer::ConfigureSky, &Renderer::EncodeSky},
    {Stage::Subjects, &Renderer::ConfigureSubjects, &Renderer::EncodeSubjects},
    {Stage::SubjectsTransmissive, &Renderer::ConfigureGlass, &Renderer::EncodeGlass},
    {Stage::CompositeTransmission, &Renderer::ConfigureCompositeTransmission,
     &Renderer::EncodeCompositeTransmission},
    {Stage::TemporalResolve, &Renderer::ConfigureTemporalResolve, nullptr},
    {Stage::Tonemap, &Renderer::ConfigureTonemap, &Renderer::EncodeTonemap},
    {Stage::Overlay, &Renderer::ConfigureOverlay, &Renderer::EncodeOverlay},
    {Stage::Present, &Renderer::ConfigurePresent, &Renderer::EncodePresent},
};
const size_t Renderer::kExecutorCount = sizeof kExecutors / sizeof kExecutors[0];

const Renderer::Executor *Renderer::ExecutorOf(Stage stage) {
  for (size_t at = 0; at < kExecutorCount; ++at) {
    if (kExecutors[at].Named == stage) { return &kExecutors[at]; }
  }
  return nullptr;
}

bool Renderer::Executable(Stage stage) { return ExecutorOf(stage) != nullptr; }

bool Renderer::Stands() {
  if (Device_) { return true; }
  if (!SDL_WasInit(SDL_INIT_VIDEO)) {
    Log::Error("render", "no_video", {{"msg", "the client did not initialise SDL video"}});
    WhyNot_ =
        "SDL's video subsystem is not running: outshine renders through SDL3 and the CLIENT owns "
        "the process, so the client calls SDL_Init(SDL_INIT_VIDEO) before it declares a scenario";
    return false;
  }
  SDL_GPUDevice *device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, kGpuValidation, nullptr);
  if (!device) {
    Log::Error("render", "no_device", {{"msg", SDL_GetError()}});
    WhyNot_ = std::string("no gpu device: ") + SDL_GetError();
    return false;
  }
  Device_ = OwnedDevice(device);
  return true;
}

bool Renderer::StandsOffscreen() {
  if (Showing_ != nullptr || Offscreen_ != nullptr || Plan_ == nullptr || Width_ <= 0) {
    return true;
  }
  SDL_GPUTextureCreateInfo wanted{};
  wanted.type = SDL_GPU_TEXTURETYPE_2D;
  wanted.format = SurfaceFormat();
  wanted.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
  wanted.width = (Uint32)Width_;
  wanted.height = (Uint32)Height_;
  wanted.layer_count_or_depth = 1;
  wanted.num_levels = 1;
  Offscreen_ = SDL_CreateGPUTexture(Device_.Get(), &wanted);
  if (Offscreen_ == nullptr) {
    WhyNot_ = std::string("the device refused a canvas of that extent: ") + SDL_GetError();
    return false;
  }
  HostSurface_ = Offscreen_;
  return true;
}

void Renderer::Init(int width, int height, std::shared_ptr<const RenderPlan> plan) {
  WhyNot_.clear();
  Plan_ = std::move(plan);
  Width_ = width;
  Height_ = height;

  for (const Stage stage : Plan_->Order()) {
    if (Executable(stage)) { continue; }
    Log::Error("render", "stage_not_executed", {{"stage", Row(stage).Name}});
    WhyNot_ = std::string("this device layer does not execute the stage '") + Row(stage).Name +
              "', which the catalogue offers and the consumer declared";
    return;
  }

  Ready_ = false;
  if (!Stands()) { return; }

  SDL_WaitForGPUIdle(Device_.Get());
  SDL_GPUDevice *const device = Device_.Get();
  Handles_.Device = device;
  Handles_.HdrFormat = FormatOf(Plan_->Format(Resource::SceneHdr));
  Handles_.SurfaceFormat = FormatOf(Plan_->Format(Resource::FrameTex));
  Handles_.Width = Width_;
  Handles_.Height = Height_;

  for (const RenderPlan::Pass &pass : Plan_->Passes()) {
    if (pass.Kind == PassKind::Compute || pass.Depth == kNoEdge) { continue; }
    Handles_.SceneColours = pass.Targets;
    break;
  }
  const auto coloursOfPassWith = [this](Stage wanted) {
    for (const RenderPlan::Pass &pass : Plan_->Passes()) {
      for (size_t at = pass.First; at < pass.First + pass.Count; ++at) {
        if (Plan_->Order()[at] == wanted) { return pass.Targets; }
      }
    }
    return Handles_.SceneColours;
  };
  Handles_.FiltersFloat32 =
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
    Handles_.SceneColours = coloursOfPassWith(stage);
    if (Configure(stage, why)) { continue; }
    Log::Error("render", "stage_not_configured", {{"stage", Row(stage).Name}, {"msg", why}});
    WhyNot_ = std::string("the stage '") + Row(stage).Name + "' did not configure: " + why;
    return;
  }
  if (!StandsOffscreen()) { return; }
  Ready_ = true;

  Log::Info("render", "device_ready",
            {{"width", Width_},
             {"height", Height_},
             {"driver", SDL_GetGPUDeviceDriver(device)},
             {"plan", Plan_->Digest()},
             {"passes", Plan_->PassCount()},
             {"stages", (int)Plan_->Order().size()},
             {"f32filter", Handles_.FiltersFloat32}});
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
    wanted.width = (uint32_t)Width_;
    wanted.height = (uint32_t)Height_;
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
      Samp_ = OwnedSampler(Device_.Get(), SDL_CreateGPUSampler(Device_.Get(), &wanted));
      return;
    }

    case Resource::OverlayAtlas: return;
    case Resource::SceneHdr: HdrTex_ = target(resource, colour); return;
    case Resource::SceneTransmissive: TransmissiveTex_ = target(resource, colour); return;
    case Resource::SceneComposited: CompositedTex_ = target(resource, colour); return;
    case Resource::SceneVelocity: VelTex_ = target(resource, colour); return;
    case Resource::SceneShadingNormal: ShadingNormalTex_ = target(resource, colour); return;
    case Resource::SceneSurfaceIdentity: SurfaceIdentityTex_ = target(resource, colour); return;
    case Resource::SceneDepth:

      DepthTex_ = target(resource, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
                                      SDL_GPU_TEXTUREUSAGE_SAMPLER);
      return;
    case Resource::FrameTex: FrameTex_ = target(resource, colour); return;

    case Resource::Surface: return;
    case Resource::TransmittanceLut:
    case Resource::MultiScatterLut:
    case Resource::SkyViewLut: {
      SDL_GPUTextureCreateInfo wanted{};
      wanted.type = SDL_GPU_TEXTURETYPE_2D;
      wanted.format = FormatOf(Plan_->Format(resource));
      wanted.usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_SAMPLER;
      wanted.width = resource == Resource::MultiScatterLut ? kMultiScatterLutSize
                     : resource == Resource::SkyViewLut    ? kSkyViewLutWidth
                                                           : kTransmittanceLutWidth;
      wanted.height = resource == Resource::MultiScatterLut ? kMultiScatterLutSize
                      : resource == Resource::SkyViewLut    ? kSkyViewLutHeight
                                                            : kTransmittanceLutHeight;
      wanted.layer_count_or_depth = 1;
      wanted.num_levels = 1;
      wanted.sample_count = SDL_GPU_SAMPLECOUNT_1;
      OwnedTexture &held = resource == Resource::MultiScatterLut ? MultiScatterLut_
                           : resource == Resource::SkyViewLut    ? SkyViewLut_
                                                                 : TransmittanceLut_;
      held = OwnedTexture(Device_.Get(), SDL_CreateGPUTexture(Device_.Get(), &wanted));
      return;
    }
    case Resource::LutSampler: {
      SDL_GPUSamplerCreateInfo wanted{};
      wanted.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
      wanted.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
      wanted.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
      wanted.min_filter = SDL_GPU_FILTER_LINEAR;
      wanted.mag_filter = SDL_GPU_FILTER_LINEAR;
      wanted.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
      LutSamp_ = OwnedSampler(Device_.Get(), SDL_CreateGPUSampler(Device_.Get(), &wanted));
      return;
    }
    case Resource::AtmosphereUniform:
    case Resource::CascadeUniform:
    case Resource::VegetationTable:
    case Resource::IrradianceBuffer:
    case Resource::Meter:
    case Resource::ShadowAtlas: {
      SDL_GPUTextureCreateInfo wanted{};
      wanted.type = SDL_GPU_TEXTURETYPE_2D;
      wanted.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
      wanted.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
      wanted.width = (uint32_t)kShadowAtlasPx;
      wanted.height = (uint32_t)kShadowAtlasPx;
      wanted.layer_count_or_depth = 1;
      wanted.num_levels = 1;
      wanted.sample_count = SDL_GPU_SAMPLECOUNT_1;
      ShadowAtlas_ = OwnedTexture(Device_.Get(), SDL_CreateGPUTexture(Device_.Get(), &wanted));
      return;
    }
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
    case Resource::SceneHdr: return HdrTex_.Get();
    case Resource::SceneTransmissive: return TransmissiveTex_.Get();
    case Resource::SceneComposited: return CompositedTex_.Get();
    case Resource::SceneVelocity: return VelTex_.Get();
    case Resource::SceneShadingNormal: return ShadingNormalTex_.Get();
    case Resource::SceneSurfaceIdentity: return SurfaceIdentityTex_.Get();
    case Resource::SceneDepth: return DepthTex_.Get();
    case Resource::FrameTex: return FrameTex_.Get();

    case Resource::Surface: return HostSurface_;
    case Resource::TransmittanceLut: return TransmittanceLut_.Get();
    case Resource::MultiScatterLut: return MultiScatterLut_.Get();
    case Resource::SkyViewLut: return SkyViewLut_.Get();
    case Resource::ShadowAtlas: return ShadowAtlas_.Get();
    case Resource::LinearSampler:
    case Resource::LutSampler:
    case Resource::AtmosphereUniform:
    case Resource::CascadeUniform:
    case Resource::VegetationTable:
    case Resource::IrradianceBuffer:
    case Resource::Meter:
    case Resource::AoBuffer:
      return nullptr;

    case Resource::SceneLinear:
      return LinearTex_[LinearAt_].Get();
    case Resource::kCount:
      return nullptr;
  }
  return nullptr;
}

DisplayOptions Renderer::Display() const {
  DisplayOptions options;
  options.Exposure = Plan_->Exposure();
  options.Curve = Plan_->Display();

  options.Temporal = Plan_->Holds(Stage::TemporalResolve);
  return options;
}

Renderer::Placed Renderer::PictureRect() const {
  Placed out;
  out.LeftPx = 0;
  out.TopPx = 0;
  out.WidthPx = (double)Width_;
  out.HeightPx = (double)Height_;
  if (RegionW_ > 0 && RegionH_ > 0) {
    out.LeftPx = RegionX_ * (double)Width_;
    out.TopPx = RegionY_ * (double)Height_;
    out.WidthPx = RegionW_ * (double)Width_;
    out.HeightPx = RegionH_ * (double)Height_;
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

double Renderer::PictureW() const { return PictureRect().WidthPx; }
double Renderer::PictureH() const { return PictureRect().HeightPx; }

SDL_GPUTextureFormat Renderer::SurfaceFormat() const {
  if (Showing_ != nullptr) {
    return SDL_GetGPUSwapchainTextureFormat(Device_.Get(), Showing_);
  }
  return Plan_ ? FormatOf(Plan_->Format(Resource::Surface)) : SDL_GPU_TEXTUREFORMAT_INVALID;
}

SDL_GPUTexture *Renderer::LinearSource() const {
  return Target(Plan_->Bound(Resource::SceneLinear));
}

bool Renderer::Configure(Stage stage, std::string &error) {
  const Executor *seat = ExecutorOf(stage);
  if (seat == nullptr) {
    error = "this device layer does not execute the stage";
    return false;
  }
  return (this->*(seat->Configure))(error);
}

bool Renderer::ConfigureSubjects(std::string &error) {
  if (DrawsGlass_) { Subjects_.GlassIsDrawnElsewhere(); }
  return Subjects_.Configure(Handles_, error);
}

bool Renderer::ConfigureGlass(std::string &error) {
  Glass_.SeeThroughTo(HdrTex_.Get(), Samp_.Get());
  return Glass_.Configure(Handles_, error);
}

bool Renderer::ConfigureCompositeTransmission(std::string &error) {
  return CompositeTransmission_.Configure(Handles_, HdrTex_.Get(), TransmissiveTex_.Get(), Samp_.Get(),
                                          FormatOf(Plan_->Format(Resource::SceneComposited)),
                                          error);
}

bool Renderer::ConfigureTemporalResolve(std::string &error) {
  (void)error;
  return true;
}

bool Renderer::ConfigureOverlay(std::string &error) {
  return Overlay_.Configure(Handles_, Samp_.Get(), FormatOf(Plan_->Format(Resource::FrameTex)),
                            error);
}

bool Renderer::ConfigurePresent(std::string &error) {
  return Present_.Configure(Handles_, FrameTex_.Get(), Samp_.Get(), error);
}

bool Renderer::ConfigureTonemap(std::string &error) {
  return Tonemap_.Configure(Handles_, Target(Plan_->Bound(Resource::SceneComposited)),
                            DepthTex_.Get(), Samp_.Get(),
                            FormatOf(Plan_->Format(Resource::SceneLinear)), Display(), error);
}

bool Renderer::ConfigureMediumTransmittance(std::string &error) {
  return MediumTransmittance_.Configure(Handles_, TransmittanceLut_.Get(), error);
}

bool Renderer::ConfigureMediumMultiScatter(std::string &error) {
  return MultiScatter_.Configure(Handles_, TransmittanceLut_.Get(), LutSamp_.Get(),
                                 MultiScatterLut_.Get(), error);
}

bool Renderer::ConfigureMediumRadiance(std::string &error) {
  return Radiance_.Configure(Handles_, TransmittanceLut_.Get(), MultiScatterLut_.Get(),
                             LutSamp_.Get(), SkyViewLut_.Get(), error);
}

bool Renderer::ConfigureSky(std::string &error) {
  return Sky_.Configure(Handles_, SkyViewLut_.Get(), LutSamp_.Get(), error);
}

bool Renderer::ConfigureLightVisibility(std::string &error) {
  return Shadow_.Configure(Subjects_, Handles_, error);
}

void Renderer::Picture(bool picture, const PassRecording &into) {
  SDL_GPUViewport where{};
  const Placed rect = PictureRect();
  where.x = picture ? (float)rect.LeftPx : 0.0f;
  where.y = picture ? (float)rect.TopPx : 0.0f;
  where.w = picture ? (float)rect.WidthPx : (float)Width_;
  where.h = picture ? (float)rect.HeightPx : (float)Height_;
  where.min_depth = 0.0f;
  where.max_depth = 1.0f;
  if (into.Pass != nullptr) { SDL_SetGPUViewport(into.Pass, &where); }
}

void Renderer::EncodeStage(Stage stage, const PassRecording &into) {
  const Executor *seat = ExecutorOf(stage);
  if (seat == nullptr || seat->Encode == nullptr) { return; }

  FrameContext ctx{};
  for (int axis = 0; axis < 3; axis++) { ctx.Eye[axis] = Eye_[axis]; }

  MvpCamRel(ctx.Mvp16, Right_, Up_, Fwd_, PictureW(), PictureH(), FovDeg_, OrthoM_, Jitter_[0],
            Jitter_[1], NearM_);
  for (int axis = 0; axis < 3; axis++) {
    ctx.PrevEye[axis] = Submitted_ ? PrevEye_[axis] : ctx.Eye[axis];
  }
  for (int at = 0; at < 16; at++) {
    ctx.PrevMvp16[at] = Submitted_ ? PrevMvp16_[at] : ctx.Mvp16[at];
  }

  const outshine::Heap::Tagged encoding(Row(stage).Name);
  (this->*(seat->Encode))(ctx, into);
}

void Renderer::EncodeSubjects(const FrameContext &ctx, const PassRecording &into) {
  Picture(true, into);
  Subjects_.Encode(ctx, into);
}

void Renderer::EncodeGlass(const FrameContext &ctx, const PassRecording &into) {
  Picture(true, into);
  Glass_.Encode(ctx, into);
}

void Renderer::EncodeCompositeTransmission(const FrameContext &ctx, const PassRecording &into) {
  Picture(true, into);
  CompositeTransmission_.Encode(ctx, into);
}

void Renderer::EncodeTonemap(const FrameContext &ctx, const PassRecording &into) {
  Tonemap_.Bind(Target(Plan_->Bound(Resource::SceneComposited)));
  const float delta[2] = {Jitter_[0] - PrevJitter_[0], Jitter_[1] - PrevJitter_[1]};
  Tonemap_.BindTemporal(LinearTex_[1 - LinearAt_].Get(), VelTex_.Get(), Width_, Height_, delta,
                        HistoryHeld_);
  Picture(true, into);
  Tonemap_.Encode(ctx, into);
}

void Renderer::EncodeOverlay(const FrameContext &ctx, const PassRecording &into) {
  Picture(false, into);
  Overlay_.Bind(Width_, Height_);
  Overlay_.Encode(ctx, into);
}

void Renderer::EncodePresent(const FrameContext &ctx, const PassRecording &into) {
  {
    std::string why;
    if (!Present_.For(Handles_, SurfaceFormat(), why)) {
      Log::Error("render", "present_not_built", {{"msg", why}});
      return;
    }
  }
  Picture(false, into);
  Present_.Encode(ctx, into);
}

void Renderer::EncodeMediumTransmittance(const FrameContext &ctx, const PassRecording &into) {
  (void)ctx;
  MediumTransmittance_.Encode(into);
}

void Renderer::EncodeMediumMultiScatter(const FrameContext &ctx, const PassRecording &into) {
  (void)ctx;
  MultiScatter_.Encode(into);
}

void Renderer::EncodeMediumRadiance(const FrameContext &ctx, const PassRecording &into) {
  (void)ctx;
  Radiance_.Encode(into);
}

void Renderer::EncodeLightVisibility(const FrameContext &ctx, const PassRecording &into) {
  Shadow_.Encode(ctx, into);
}

void Renderer::EncodeSky(const FrameContext &ctx, const PassRecording &into) {
  Picture(true, into);
  const float tanHalfH = std::tan((float)(FovDeg_ * std::numbers::pi / 180.0) * 0.5f);
  const float tanHalfW = tanHalfH * (PictureH() > 0.0 ? (float)(PictureW() / PictureH()) : 1.0f);
  float right[3], up[3], fwd[3];
  for (int axis = 0; axis < 3; ++axis) {
    right[axis] = (float)Right_[axis];
    up[axis] = (float)Up_[axis];
    fwd[axis] = (float)Fwd_[axis];
  }
  Sky_.SetBasis(right, up, fwd, tanHalfW, tanHalfH);
  Sky_.Encode(ctx, into);
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
  if (declared.Kind == PassKind::Compute) {
    SDL_GPUStorageTextureReadWriteBinding written[kMaxColourAttachments] = {};
    uint32_t writtenCount = 0;
    for (const Resource wanted : declared.Targets) {
      SDL_GPUStorageTextureReadWriteBinding &binding = written[writtenCount++];
      binding.texture = Target(wanted);
      binding.cycle = false;
    }
    PassRecording into{commands, nullptr,
                       SDL_BeginGPUComputePass(commands, written, writtenCount, nullptr, 0)};
    for (size_t at = 0; at < declared.Count; ++at) {
      EncodeStage(Plan_->Order()[declared.First + at], into);
    }
    SDL_EndGPUComputePass(into.Dispatch);
    return;
  }
  SDL_GPUColorTargetInfo colours[kMaxColourAttachments] = {};
  uint32_t colourCount = 0;
  for (const Resource wanted : declared.Targets) {
    SDL_GPUColorTargetInfo &attachment = colours[colourCount++];
    attachment.texture = Target(wanted);
    attachment.load_op =
        Touched_[(size_t)wanted] ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
    Touched_[(size_t)wanted] = true;
    attachment.store_op =
        Plan_->Stored(wanted) ? SDL_GPU_STOREOP_STORE : SDL_GPU_STOREOP_DONT_CARE;

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
    depth.load_op = Touched_[(size_t)declared.Depth] ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
    Touched_[(size_t)declared.Depth] = true;
    depth.store_op =
        Plan_->Stored(declared.Depth) ? SDL_GPU_STOREOP_STORE : SDL_GPU_STOREOP_DONT_CARE;
    depth.clear_depth = 0.0f;
    depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
  }
  PassRecording into{commands,
                     SDL_BeginGPURenderPass(commands, colours, colourCount,
                                            declared.Depth != kNoEdge ? &depth : nullptr),
                     nullptr};
  for (size_t at = 0; at < declared.Count; ++at) {
    EncodeStage(Plan_->Order()[declared.First + at], into);
  }
  SDL_EndGPURenderPass(into.Pass);
}

void Renderer::BeginTemporalRun() {
  HistoryStarted_ = false;
  JitterAt_ = 0;
  Jitter_[0] = 0.0f;
  Jitter_[1] = 0.0f;
  PrevJitter_[0] = 0.0f;
  PrevJitter_[1] = 0.0f;
  LinearAt_ = 0;
  HistoryHeld_ = false;
}

void Renderer::RenderFrame() {
  if (!Ready_) { return; }
  if (!CameraFull_) {
    WhyNot_ = "no camera basis reached this renderer, so a frame has no eye to be seen from -- "
              "SetCameraBasis takes the eye, forward, right and up the picture is composed about";
    return;
  }

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
  for (bool &touched : Touched_) { touched = false; }
  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(Device_.Get());

  SDL_GPUTexture *swapchain = nullptr;
  if (Showing_ != nullptr) {
    Uint32 gotW = 0, gotH = 0;
    if (SDL_WaitAndAcquireGPUSwapchainTexture(commands, Showing_, &swapchain, &gotW, &gotH) &&
        swapchain != nullptr) {
      Shown_.WidthPx = (int)gotW;
      Shown_.HeightPx = (int)gotH;
      HostSurface_ = swapchain;
    } else {
      Log::Error("render", "no_swapchain", {{"msg", SDL_GetError()}});
    }
  }

  Subjects_.FlushCrossings(commands);
  if (DrawsGlass_) { Glass_.FlushCrossings(commands); }

  for (size_t pass = 0; pass < Plan_->Passes().size(); ++pass) { EncodePass(commands, pass); }

  if (Landed_[LandedAt_] != nullptr) {
    SDL_WaitForGPUFences(Device_.Get(), true, &Landed_[LandedAt_], 1);
    SDL_ReleaseGPUFence(Device_.Get(), Landed_[LandedAt_]);
    Landed_[LandedAt_] = nullptr;
  }
  SDL_GPUTransferBuffer *taking = nullptr;
  if (Wanted_ && HostSurface_ != nullptr) {
    SDL_GPUTransferBufferCreateInfo wanted{};
    wanted.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    wanted.size = (Uint32)((size_t)Width_ * (size_t)Height_ * 4u);
    taking = SDL_CreateGPUTransferBuffer(Device_.Get(), &wanted);
    if (taking != nullptr) {
      SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
      SDL_GPUTextureRegion region{};
      region.texture = HostSurface_;
      region.w = (Uint32)Width_;
      region.h = (Uint32)Height_;
      region.d = 1;
      SDL_GPUTextureTransferInfo into{};
      into.transfer_buffer = taking;
      into.pixels_per_row = (Uint32)Width_;
      into.rows_per_layer = (Uint32)Height_;
      SDL_DownloadFromGPUTexture(copy, &region, &into);
      SDL_EndGPUCopyPass(copy);
    }
  }

  Landed_[LandedAt_] = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
  if (taking != nullptr) {
    SDL_WaitForGPUFences(Device_.Get(), true, &Landed_[LandedAt_], 1);
    if (const void *pixels = SDL_MapGPUTransferBuffer(Device_.Get(), taking, false)) {
      const uint8_t *bytes = (const uint8_t *)pixels;
      Taken_.assign(bytes, bytes + (size_t)Width_ * (size_t)Height_ * 4u);
      SDL_UnmapGPUTransferBuffer(Device_.Get(), taking);
    }
    SDL_ReleaseGPUTransferBuffer(Device_.Get(), taking);
    Wanted_ = false;
  }
  if (swapchain != nullptr) { HostSurface_ = Offscreen_; }
  LandedAt_ = (LandedAt_ + 1) % kFramesInFlight;
  for (int axis = 0; axis < 3; axis++) { PrevEye_[axis] = Eye_[axis]; }

  MvpCamRel(PrevMvp16_, Right_, Up_, Fwd_, PictureW(), PictureH(), FovDeg_, OrthoM_, Jitter_[0],
            Jitter_[1], NearM_);
  Submitted_ = true;
}

void Renderer::WaitForGpu() {
  if (!Ready_) { return; }
  SDL_WaitForGPUIdle(Device_.Get());

  for (SDL_GPUFence *&held : Landed_) {
    if (held == nullptr) { continue; }
    SDL_ReleaseGPUFence(Device_.Get(), held);
    held = nullptr;
  }
}

void Renderer::WantsPixels() { Wanted_ = true; }

ReadState Renderer::ReadPixels(std::vector<uint8_t> &rgba) {
  if (!Ready_) { return ReadState::Failed; }
  if (Showing_ == nullptr) {
    if (HostSurface_ == nullptr) { return ReadState::Failed; }
    Readback read;
    if (read.FromTexture(Device_.Get(), HostSurface_, (uint32_t)Width_, (uint32_t)Height_, 4u) !=
        ReadState::Ready) {
      return ReadState::Failed;
    }
    rgba.resize((size_t)Width_ * (size_t)Height_ * 4u);
    std::memcpy(rgba.data(), read.Rows(), rgba.size());
    return ReadState::Ready;
  }
  if (Taken_.size() == (size_t)Width_ * (size_t)Height_ * 4u) {
    rgba = Taken_;
    return ReadState::Ready;
  }
  Wanted_ = true;
  return ReadState::Failed;
}

ReadState Renderer::ReadDepth(std::vector<float> &depth) {
  if (!Ready_ || !DepthTex_) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.Get(), DepthTex_.Get(), (uint32_t)Width_, (uint32_t)Height_, 4u) !=
      ReadState::Ready) {
    return ReadState::Failed;
  }
  depth.resize((size_t)Width_ * (size_t)Height_);
  std::memcpy(depth.data(), read.Rows(), depth.size() * sizeof(float));
  return ReadState::Ready;
}

ReadState Renderer::ReadSceneLinear(std::vector<float> &rgba) {
  SDL_GPUTexture *source = LinearSource();
  if (!Ready_ || !source) { return ReadState::Failed; }
  const bool wide = Plan_->Format(Resource::SceneLinear) == TexelFormat::Rgba32Float;
  Readback read;
  if (read.FromTexture(Device_.Get(), source, (uint32_t)Width_, (uint32_t)Height_, wide ? 16u : 8u) !=
      ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components = (size_t)Width_ * (size_t)Height_ * 4u;
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

ReadState Renderer::ReadShadowAtlas(std::vector<float> &depth) {
  if (!Ready_ || !ShadowAtlas_) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.Get(), ShadowAtlas_.Get(), (uint32_t)kShadowAtlasPx,
                       (uint32_t)kShadowAtlasPx, 4u) != ReadState::Ready) {
    return ReadState::Failed;
  }
  depth.resize((size_t)kShadowAtlasPx * (size_t)kShadowAtlasPx);
  std::memcpy(depth.data(), read.Rows(), depth.size() * sizeof(float));
  return ReadState::Ready;
}

ReadState Renderer::ReadShadingNormal(std::vector<float> &xyz) {
  SDL_GPUTexture *source = ShadingNormalTex_.Get();
  if (!Ready_ || !source) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.Get(), source, (uint32_t)Width_, (uint32_t)Height_, 8u) !=
      ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components = (size_t)Width_ * (size_t)Height_ * 4u;
  xyz.resize(components);
  for (size_t component = 0; component < components; ++component) {
    uint16_t bits = 0;
    std::memcpy(&bits, read.Rows() + component * sizeof(uint16_t), sizeof bits);
    xyz[component] = HalfToFloat(bits);
  }
  return ReadState::Ready;
}

ReadState Renderer::ReadSceneVelocity(std::vector<float> &xy) {
  SDL_GPUTexture *source = VelTex_.Get();
  if (!Ready_ || !source) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.Get(), source, (uint32_t)Width_, (uint32_t)Height_, 4u) !=
      ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components = (size_t)Width_ * (size_t)Height_ * 2u;
  xy.resize(components);
  for (size_t component = 0; component < components; ++component) {
    uint16_t bits = 0;
    std::memcpy(&bits, read.Rows() + component * sizeof(uint16_t), sizeof bits);
    xy[component] = HalfToFloat(bits);
  }
  return ReadState::Ready;
}

ReadState Renderer::ReadSurfaceIdentity(std::vector<float> &slot) {
  SDL_GPUTexture *source = SurfaceIdentityTex_.Get();
  if (!Ready_ || !source) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.Get(), source, (uint32_t)Width_, (uint32_t)Height_, 16u) !=
      ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components = (size_t)Width_ * (size_t)Height_ * 4u;
  slot.resize(components);
  std::memcpy(slot.data(), read.Rows(), components * sizeof(float));
  return ReadState::Ready;
}

void Renderer::StopShowing() {
  if (Offscreen_ != nullptr) {
    if (Device_.Get() != nullptr) { SDL_ReleaseGPUTexture(Device_.Get(), Offscreen_); }
    Offscreen_ = nullptr;
    HostSurface_ = nullptr;
  }
  if (Showing_ == nullptr) { return; }
  if (Device_.Get() != nullptr) { SDL_ReleaseWindowFromGPUDevice(Device_.Get(), Showing_); }
  Showing_ = nullptr;
}

std::expected<void, std::string_view> Renderer::DrawsInto(int widthPx, int heightPx,
                                                          SDL_Window *presents) {
  if (widthPx <= 0 || heightPx <= 0) {
    return std::unexpected("a canvas has an extent, and this one declares none");
  }
  if (!Stands()) {
    return std::unexpected("the renderer has no device to stand a canvas on");
  }

  if (Showing_ != presents) {
    StopShowing();
    if (presents != nullptr && !SDL_ClaimWindowForGPUDevice(Device_.Get(), presents)) {
      WhyNot_ = std::string("the window was refused by the device: ") + SDL_GetError();
      return std::unexpected(
          "the window was refused by the device, and WhyNot carries what it said");
    }
    Showing_ = presents;
  }

  if (Offscreen_ != nullptr) {
    SDL_ReleaseGPUTexture(Device_.Get(), Offscreen_);
    Offscreen_ = nullptr;
  }
  HostSurface_ = nullptr;
  Width_ = widthPx;
  Height_ = heightPx;
  if (!StandsOffscreen()) {
    return std::unexpected("the device refused a canvas of that extent");
  }
  return {};
}

std::expected<std::optional<Renderer::Shown>, std::string_view> Renderer::Presented() const {
  if (Showing_ == nullptr) {
    return std::unexpected(
        "no window is being shown on: a frame is presented to a surface the caller declared, "
        "and a Canvas names one");
  }
  if (Shown_.WidthPx == 0) { return std::optional<Shown>(); }
  return std::optional<Shown>(Shown_);
}

}
