#include "Heap.h"
#include <chrono>

#include "SceneRenderer.h"

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
} // namespace

namespace {

void MvpCamRel(float *m,
               const double R[3],
               const double Uc[3],
               const double F[3],
               double w,
               double h,
               float fovDeg,
               float orthoM,
               float jitterX,
               float jitterY,
               float nearM) {
  const float fov = fovDeg * std::numbers::pi_v<float> / 180.0f, asp = (float)w / (float)h;
  const float zn = nearM;
  const float f = 1.0f / std::tan(fov / 2.0f);
  const float v[16] = {(float)R[0],
                       (float)Uc[0],
                       -(float)F[0],
                       0,
                       (float)R[1],
                       (float)Uc[1],
                       -(float)F[1],
                       0,
                       (float)R[2],
                       (float)Uc[2],
                       -(float)F[2],
                       0,
                       0,
                       0,
                       0,
                       1};

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
    case TexelFormat::Handle:
    case TexelFormat::Table: return SDL_GPU_TEXTUREFORMAT_INVALID;
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

} // namespace

void SceneRenderer::SetCameraBasis(const double eye[3],
                                   const double fwd[3],
                                   const double right[3],
                                   const double up[3]) {
  for (int axis = 0; axis < 3; axis++) {
    Eye_[axis] = eye[axis];
    Fwd_[axis] = fwd[axis];
    Right_[axis] = right[axis];
    Up_[axis] = up[axis];
  }
  CameraFull_ = true;
}

const SceneRenderer::Executor SceneRenderer::kExecutors[] = {
    {Stage::MediumTransmittance,
     &SceneRenderer::ConfigureMediumTransmittance,
     &SceneRenderer::EncodeMediumTransmittance},
    {Stage::MediumMultiScatter,
     &SceneRenderer::ConfigureMediumMultiScatter,
     &SceneRenderer::EncodeMediumMultiScatter},
    {Stage::MediumRadiance,
     &SceneRenderer::ConfigureMediumRadiance,
     &SceneRenderer::EncodeMediumRadiance},
    {Stage::Irradiance, &SceneRenderer::ConfigureIrradiance, &SceneRenderer::EncodeIrradiance},
    {Stage::SubjectCull, &SceneRenderer::ConfigureSubjectCull, &SceneRenderer::EncodeSubjectCull},
    {Stage::SubjectScan, &SceneRenderer::ConfigureSubjectCull, &SceneRenderer::EncodeSubjectScan},
    {Stage::SubjectCompact,
     &SceneRenderer::ConfigureSubjectCull,
     &SceneRenderer::EncodeSubjectCompact},
    {Stage::LightVisibility,
     &SceneRenderer::ConfigureLightVisibility,
     &SceneRenderer::EncodeLightVisibility},
    {Stage::Sky, &SceneRenderer::ConfigureSky, &SceneRenderer::EncodeSky},
    {Stage::Subjects, &SceneRenderer::ConfigureSubjects, &SceneRenderer::EncodeSubjects},
    {Stage::SubjectsTransmissive, &SceneRenderer::ConfigureGlass, &SceneRenderer::EncodeGlass},
    {Stage::CompositeTransmission,
     &SceneRenderer::ConfigureCompositeTransmission,
     &SceneRenderer::EncodeCompositeTransmission},
    {Stage::AerialPerspective,
     &SceneRenderer::ConfigureAerialPerspective,
     &SceneRenderer::EncodeAerialPerspective},
    {Stage::TemporalResolve, &SceneRenderer::ConfigureTemporalResolve, nullptr},
    {Stage::Tonemap, &SceneRenderer::ConfigureTonemap, &SceneRenderer::EncodeTonemap},
    {Stage::Overlay, &SceneRenderer::ConfigureOverlay, &SceneRenderer::EncodeOverlay},
    {Stage::Present, &SceneRenderer::ConfigurePresent, &SceneRenderer::EncodePresent},
};
const size_t SceneRenderer::kExecutorCount = sizeof kExecutors / sizeof kExecutors[0];

const SceneRenderer::Executor *SceneRenderer::ExecutorOf(Stage stage) {
  for (size_t at = 0; at < kExecutorCount; ++at) {
    if (kExecutors[at].Named == stage) { return &kExecutors[at]; }
  }
  return nullptr;
}

bool SceneRenderer::Executable(Stage stage) {
  return ExecutorOf(stage) != nullptr;
}

bool SceneRenderer::Stands() {
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

std::expected<void, std::string_view> SceneRenderer::StandsOffscreen() {
  if (Showing_ != nullptr || Offscreen_ != nullptr || Plan_ == nullptr || Width_ <= 0) {
    return {};
  }
  if (!Plan_->Holds(Resource::Surface)) { return {}; }
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
    return std::unexpected("the canvas did not stand, and WhyNot carries what the device said");
  }
  HostSurface_ = Offscreen_;
  return {};
}

void SceneRenderer::Init(int width, int height, std::shared_ptr<const Compiled> plan) {
  WhyNot_.clear();
  Ready_ = false;
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

  if (!Stands()) { return; }

  SDL_WaitForGPUIdle(Device_.Get());
  SDL_GPUDevice *const device = Device_.Get();
  Handles_.Device = device;
  Handles_.HdrFormat = FormatOf(Plan_->Format(Resource::SceneHdr));
  Handles_.SurfaceFormat = FormatOf(Plan_->Format(Resource::FrameTex));
  Handles_.Width = Width_;
  Handles_.Height = Height_;

  for (const Compiled::Pass &pass : Plan_->Passes()) {
    if (pass.Kind == PassKind::Compute || pass.Depth == kNoEdge) { continue; }
    Handles_.SceneColours = pass.Targets;
    break;
  }
  const auto coloursOfPassWith = [this](Stage wanted) {
    for (const Compiled::Pass &pass : Plan_->Passes()) {
      for (size_t at = pass.First; at < pass.First + pass.Count; ++at) {
        if (Plan_->Order()[at] == wanted) { return pass.Targets; }
      }
    }
    return Handles_.SceneColours;
  };
  Handles_.FiltersFloat32 = SDL_GPUTextureSupportsFormat(device,
                                                         SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
                                                         SDL_GPU_TEXTURETYPE_2D,
                                                         SDL_GPU_TEXTUREUSAGE_SAMPLER);

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

  Log::Info("render",
            "device_ready",
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

void SceneRenderer::Create(Resource resource) {
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
    case Resource::SceneAerial: AerialTex_ = target(resource, colour); return;
    case Resource::SceneVelocity: VelTex_ = target(resource, colour); return;
    case Resource::SceneShadingNormal: ShadingNormalTex_ = target(resource, colour); return;
    case Resource::SceneSurfaceIdentity: SurfaceIdentityTex_ = target(resource, colour); return;
    case Resource::SceneDepth:

      DepthTex_ = target(resource,
                         SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER);
      return;
    case Resource::FrameTex: FrameTex_ = target(resource, colour); return;

    case Resource::Surface: return;

    case Resource::ClusterSphere:
    case Resource::ClusterIndex:
    case Resource::ClusterJobs:
    case Resource::ClusterBatches:
    case Resource::ClusterKept:
    case Resource::ClusterSlot:
    case Resource::DrawIndex:
    case Resource::DrawArguments: return;
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
    case Resource::IrradianceBuffer: {
      SDL_GPUBufferCreateInfo wanted{};
      wanted.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
      wanted.size = kIrradianceFloats * (uint32_t)sizeof(float);
      Irradiance_ = OwnedBuffer(Handles_.Device, SDL_CreateGPUBuffer(Handles_.Device, &wanted));
      return;
    }
    case Resource::VegetationTable:
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
    case Resource::AoBuffer: return;

    case Resource::SceneLinear:

      LinearTex_[0] = target(resource, colour);
      LinearTex_[1] = target(resource, colour);
      LinearAt_ = 0;
      HistoryHeld_ = false;
      return;
    case Resource::kCount: return;
  }
}

SDL_GPUTexture *SceneRenderer::Target(Resource resource) const {
  switch (resource) {
    case Resource::OverlayAtlas: return nullptr;
    case Resource::SceneHdr: return HdrTex_.Get();
    case Resource::SceneTransmissive: return TransmissiveTex_.Get();
    case Resource::SceneComposited: return CompositedTex_.Get();
    case Resource::SceneAerial: return AerialTex_.Get();
    case Resource::SceneVelocity: return VelTex_.Get();
    case Resource::SceneShadingNormal: return ShadingNormalTex_.Get();
    case Resource::SceneSurfaceIdentity: return SurfaceIdentityTex_.Get();
    case Resource::SceneDepth: return DepthTex_.Get();
    case Resource::FrameTex: return FrameTex_.Get();

    case Resource::Surface: return HostSurface_;

    case Resource::ClusterSphere:
    case Resource::ClusterIndex:
    case Resource::ClusterJobs:
    case Resource::ClusterBatches:
    case Resource::ClusterKept:
    case Resource::ClusterSlot:
    case Resource::DrawIndex:
    case Resource::DrawArguments: return nullptr;
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
    case Resource::AoBuffer: return nullptr;

    case Resource::SceneLinear: return LinearTex_[LinearAt_].Get();
    case Resource::kCount: return nullptr;
  }
  return nullptr;
}

SDL_GPUBuffer *SceneRenderer::BufferFor(Resource resource) const {
  const SubjectResidency &resident = Subjects_.Resident();
  switch (resource) {
    case Resource::ClusterSphere: return resident.ClusterSpheres.Get();
    case Resource::ClusterIndex: return resident.Idx.Get();
    case Resource::ClusterJobs: return resident.ClusterJobs.Get();
    case Resource::ClusterBatches: return resident.ClusterBatches.Get();
    case Resource::ClusterKept: return resident.ClusterKept.Get();
    case Resource::ClusterSlot: return resident.ClusterSlot.Get();
    case Resource::DrawIndex: return resident.DrawIdx.Get();
    case Resource::DrawArguments: return resident.DrawArgs.Get();
    case Resource::IrradianceBuffer: return Irradiance_.Get();
    default: return nullptr;
  }
}

DisplayOptions SceneRenderer::Display() const {
  DisplayOptions options;
  options.Exposure = Plan_->Exposure();
  options.Curve = Plan_->Display();

  options.Temporal = Plan_->Holds(Stage::TemporalResolve);
  return options;
}

SceneRenderer::Placed SceneRenderer::PictureRect() const {
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
    const double fitted =
        out.WidthPx / out.HeightPx > RegionAspect_ ? out.HeightPx * RegionAspect_ : out.WidthPx;
    const double tall = fitted / RegionAspect_;
    out.LeftPx += (out.WidthPx - fitted) / 2.0;
    out.TopPx += (out.HeightPx - tall) / 2.0;
    out.WidthPx = fitted;
    out.HeightPx = tall;
  }
  return out;
}

double SceneRenderer::PictureW() const {
  return PictureRect().WidthPx;
}

double SceneRenderer::PictureH() const {
  return PictureRect().HeightPx;
}

SDL_GPUTextureFormat SceneRenderer::SurfaceFormat() const {
  if (Showing_ != nullptr) { return SDL_GetGPUSwapchainTextureFormat(Device_.Get(), Showing_); }
  return Plan_ ? FormatOf(Plan_->Format(Resource::Surface)) : SDL_GPU_TEXTUREFORMAT_INVALID;
}

SDL_GPUTexture *SceneRenderer::LinearSource() const {
  return Target(Plan_->Bound(Resource::SceneLinear));
}

bool SceneRenderer::Configure(Stage stage, std::string &error) {
  const Executor *seat = ExecutorOf(stage);
  if (seat == nullptr) {
    error = "this device layer does not execute the stage";
    return false;
  }
  return (this->*(seat->Configure))(error);
}

bool SceneRenderer::ConfigureSubjects(std::string &error) {
  if (DrawsGlass_) { Subjects_.GlassIsDrawnElsewhere(); }
  return Subjects_.Configure(Handles_, error);
}

bool SceneRenderer::ConfigureGlass(std::string &error) {
  Glass_.Shares(Subjects_.Owned());
  Glass_.SeeThroughTo(HdrTex_.Get(), Samp_.Get());
  return Glass_.Configure(Handles_, error);
}

bool SceneRenderer::ConfigureCompositeTransmission(std::string &error) {
  return CompositeTransmission_.Configure(Handles_,
                                          HdrTex_.Get(),
                                          TransmissiveTex_.Get(),
                                          Samp_.Get(),
                                          FormatOf(Plan_->Format(Resource::SceneComposited)),
                                          error);
}

bool SceneRenderer::ConfigureTemporalResolve(std::string &error) {
  (void)error;
  return true;
}

bool SceneRenderer::ConfigureOverlay(std::string &error) {
  return Overlay_.Configure(
      Handles_, Samp_.Get(), FormatOf(Plan_->Format(Resource::FrameTex)), error);
}

bool SceneRenderer::ConfigurePresent(std::string &error) {
  return Present_.Configure(Handles_, FrameTex_.Get(), Samp_.Get(), error);
}

bool SceneRenderer::ConfigureTonemap(std::string &error) {
  return Tonemap_.Configure(Handles_,
                            Target(Plan_->Bound(Resource::SceneLinear)),
                            DepthTex_.Get(),
                            Samp_.Get(),
                            FormatOf(Plan_->Format(Resource::SceneLinear)),
                            Display(),
                            error);
}

bool SceneRenderer::ConfigureMediumTransmittance(std::string &error) {
  return MediumTransmittance_.Configure(Handles_, TransmittanceLut_.Get(), error);
}

bool SceneRenderer::ConfigureMediumMultiScatter(std::string &error) {
  return MultiScatter_.Configure(
      Handles_, TransmittanceLut_.Get(), LutSamp_.Get(), MultiScatterLut_.Get(), error);
}

bool SceneRenderer::ConfigureMediumRadiance(std::string &error) {
  return Radiance_.Configure(Handles_,
                             TransmittanceLut_.Get(),
                             MultiScatterLut_.Get(),
                             LutSamp_.Get(),
                             SkyViewLut_.Get(),
                             error);
}

bool SceneRenderer::ConfigureSky(std::string &error) {
  return Sky_.Configure(
      Handles_, SkyViewLut_.Get(), TransmittanceLut_.Get(), LutSamp_.Get(), error);
}

bool SceneRenderer::ConfigureAerialPerspective(std::string &error) {
  return Aerial_.Configure(Handles_,
                           Target(Plan_->Bound(Resource::SceneComposited)),
                           DepthTex_.Get(),
                           SkyViewLut_.Get(),
                           TransmittanceLut_.Get(),
                           Samp_.Get(),
                           LutSamp_.Get(),
                           FormatOf(Plan_->Format(Resource::SceneAerial)),
                           error);
}

bool SceneRenderer::ConfigureLightVisibility(std::string &error) {
  return Shadow_.Configure(Subjects_, Handles_, error);
}

void SceneRenderer::Picture(bool picture, const PassRecording &into) {
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

FrameContext SceneRenderer::Framing() const {
  FrameContext ctx{};
  for (int axis = 0; axis < 3; axis++) { ctx.PreViewTranslation[axis] = -Eye_[axis]; }

  MvpCamRel(ctx.Mvp16,
            Right_,
            Up_,
            Fwd_,
            PictureW(),
            PictureH(),
            FovDeg_,
            OrthoM_,
            Jitter_[0],
            Jitter_[1],
            NearM_);
  for (int axis = 0; axis < 3; axis++) {
    ctx.PrevPreViewTranslation[axis] = Submitted_ ? -PrevEye_[axis] : ctx.PreViewTranslation[axis];
  }
  for (int at = 0; at < 16; at++) {
    ctx.PrevMvp16[at] = Submitted_ ? PrevMvp16_[at] : ctx.Mvp16[at];
  }
  return ctx;
}

void SceneRenderer::EncodeStage(Stage stage, const PassRecording &into) {
  const Executor *seat = ExecutorOf(stage);
  if (seat == nullptr || seat->Encode == nullptr) { return; }

  const FrameContext ctx = Framing();

  const outshine::Heap::Tagged encoding(Row(stage).Name);
  const auto began = std::chrono::steady_clock::now();
  (this->*(seat->Encode))(ctx, into);
  Effort &spent = Spent_[(size_t)stage];
  spent.TookMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  spent.Draws = 0;
  spent.Triangles = 0;
  spent.Surfaces = 0;
  spent.Placements = 0;
  if (stage == Stage::Subjects || stage == Stage::SubjectsTransmissive) {
    const SubjectDraw &drew = stage == Stage::Subjects ? Subjects_ : Glass_;
    uint32_t surfaces = 0, placements = 0;
    for (const DrawBatch &batch : drew.Drawn()) {
      spent.Draws += 1u;
      spent.Triangles += (batch.IndexCount / 3u) * batch.Instances;
      surfaces = batch.MaterialSlot + 1u > surfaces ? batch.MaterialSlot + 1u : surfaces;
      const uint32_t past = batch.ModelSlot + batch.Instances;
      placements = past > placements ? past : placements;
    }
    spent.Surfaces = surfaces;
    spent.Placements = placements;
    spent.Textured = drew.Textured();
    spent.Palettes = drew.ColourImages();
    spent.Distinct = drew.DistinctPlacements();
    spent.DeviceBytes = drew.HeldBytes();
    spent.Layouts = drew.Layouts();
  }
}

void SceneRenderer::EncodeSubjects(const FrameContext &ctx, const PassRecording &into) {
  Picture(true, into);
  Subjects_.Encode(ctx, into);
}

void SceneRenderer::EncodeGlass(const FrameContext &ctx, const PassRecording &into) {
  Picture(true, into);
  Glass_.Encode(ctx, into);
}

void SceneRenderer::EncodeCompositeTransmission(const FrameContext &ctx,
                                                const PassRecording &into) {
  Picture(true, into);
  CompositeTransmission_.Encode(ctx, into);
}

void SceneRenderer::EncodeTonemap(const FrameContext &ctx, const PassRecording &into) {
  Tonemap_.Bind(Target(Plan_->Bound(Resource::SceneLinear)));
  const float delta[2] = {Jitter_[0] - PrevJitter_[0], Jitter_[1] - PrevJitter_[1]};
  Tonemap_.BindTemporal(
      LinearTex_[1 - LinearAt_].Get(), VelTex_.Get(), Width_, Height_, delta, HistoryHeld_);
  Picture(true, into);
  Tonemap_.Encode(ctx, into);
}

void SceneRenderer::EncodeOverlay(const FrameContext &ctx, const PassRecording &into) {
  Picture(false, into);
  Overlay_.Bind(Width_, Height_);
  Overlay_.Encode(ctx, into);
}

void SceneRenderer::EncodePresent(const FrameContext &ctx, const PassRecording &into) {
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

void SceneRenderer::EncodeMediumTransmittance(const FrameContext &ctx, const PassRecording &into) {
  (void)ctx;
  MediumTransmittance_.Encode(into);
}

void SceneRenderer::EncodeMediumMultiScatter(const FrameContext &ctx, const PassRecording &into) {
  (void)ctx;
  MultiScatter_.Encode(into);
}

void SceneRenderer::EncodeMediumRadiance(const FrameContext &ctx, const PassRecording &into) {
  (void)ctx;
  Radiance_.Encode(into);
}

bool SceneRenderer::ConfigureIrradiance(std::string &error) {
  Subjects_.SkyFrom(Irradiance_.Get());
  if (DrawsGlass_) { Glass_.SkyFrom(Irradiance_.Get()); }
  return Irradiance__.Configure(Handles_,
                                TransmittanceLut_.Get(),
                                MultiScatterLut_.Get(),
                                LutSamp_.Get(),
                                Irradiance_.Get(),
                                error);
}

void SceneRenderer::EncodeIrradiance(const FrameContext &ctx, const PassRecording &into) {
  (void)ctx;
  Irradiance__.Encode(into);
}

bool SceneRenderer::ConfigureSubjectCull(std::string &error) {
  return Cull_.Configure(Subjects_, Handles_, error);
}

void SceneRenderer::EncodeSubjectCull(const FrameContext &ctx, const PassRecording &into) {
  Cull_.Projects((float)Height_);
  Cull_.EncodeCull(ctx, into);
}

void SceneRenderer::EncodeSubjectScan(const FrameContext &ctx, const PassRecording &into) {
  Cull_.EncodeScan(ctx, into);
}

void SceneRenderer::EncodeSubjectCompact(const FrameContext &ctx, const PassRecording &into) {
  Cull_.EncodeCompact(ctx, into);
}

void SceneRenderer::EncodeLightVisibility(const FrameContext &ctx, const PassRecording &into) {
  Shadow_.Encode(ctx, into);
  Subjects_.ShadowedBy(ShadowAtlas_.Get(), LutSamp_.Get(), Shadow_.LightFromWorld());
}

void SceneRenderer::SettleShadow() {
  Shadow_.Prepare(Framing());
  Touched_[(size_t)Resource::ShadowAtlas] = Shadow_.Cached();
}

void SceneRenderer::EncodeAerialPerspective(const FrameContext &ctx, const PassRecording &into) {
  Picture(true, into);
  const float tanHalfH = std::tan((float)(FovDeg_ * std::numbers::pi / 180.0) * 0.5f);
  const float tanHalfW = tanHalfH * (PictureH() > 0.0 ? (float)(PictureW() / PictureH()) : 1.0f);
  float right[3], up[3], fwd[3];
  for (int axis = 0; axis < 3; ++axis) {
    right[axis] = (float)Right_[axis];
    up[axis] = (float)Up_[axis];
    fwd[axis] = (float)Fwd_[axis];
  }
  Aerial_.SetBasis(right, up, fwd, tanHalfW, tanHalfH);
  Aerial_.SetNear(NearMetres());
  Aerial_.Encode(ctx, into);
}

void SceneRenderer::EncodeSky(const FrameContext &ctx, const PassRecording &into) {
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

void SceneRenderer::EncodePass(SDL_GPUCommandBuffer *commands, size_t pass) {
  const Compiled::Pass &declared = Plan_->Passes()[pass];
  if (declared.Kind == PassKind::Compute) {
    SDL_GPUStorageTextureReadWriteBinding written[kMaxColourAttachments] = {};
    uint32_t writtenCount = 0;
    for (const Resource wanted : declared.Targets) {
      SDL_GPUStorageTextureReadWriteBinding &binding = written[writtenCount++];
      binding.texture = Target(wanted);
      binding.cycle = false;
    }

    SDL_GPUStorageBufferReadWriteBinding tables[kMaxColourAttachments] = {};
    uint32_t tableCount = 0;
    for (const Resource wanted : declared.Buffers) {
      SDL_GPUBuffer *const held = BufferFor(wanted);
      if (held == nullptr) { continue; }
      SDL_GPUStorageBufferReadWriteBinding &binding = tables[tableCount++];
      binding.buffer = held;
      binding.cycle = false;
    }
    PassRecording into{
        commands,
        nullptr,
        SDL_BeginGPUComputePass(commands, written, writtenCount, tables, tableCount)};
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
    attachment.load_op = Touched_[(size_t)wanted] ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
    Touched_[(size_t)wanted] = true;
    attachment.store_op = Plan_->Stored(wanted) ? SDL_GPU_STOREOP_STORE : SDL_GPU_STOREOP_DONT_CARE;

    const bool carriesCoverage =
        wanted == Resource::SceneHdr || wanted == Resource::SceneComposited ||
        wanted == Resource::SceneTransmissive || wanted == Resource::SceneLinear;
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
  PassRecording into{
      commands,
      SDL_BeginGPURenderPass(
          commands, colours, colourCount, declared.Depth != kNoEdge ? &depth : nullptr),
      nullptr};
  for (size_t at = 0; at < declared.Count; ++at) {
    EncodeStage(Plan_->Order()[declared.First + at], into);
  }
  SDL_EndGPURenderPass(into.Pass);
}

void SceneRenderer::BeginTemporalRun() {
  HistoryStarted_ = false;
  JitterAt_ = 0;
  Jitter_[0] = 0.0f;
  Jitter_[1] = 0.0f;
  PrevJitter_[0] = 0.0f;
  PrevJitter_[1] = 0.0f;
  LinearAt_ = 0;
  HistoryHeld_ = false;
}

void SceneRenderer::RenderFrame() {
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
  Subjects_.CastsNoShadow();
  for (bool &touched : Touched_) { touched = false; }
  SettleShadow();
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

  {
    std::string why;
    if (!Subjects_.HandDrawArguments(true, why)) {
      Log::Error("render", "cull_arguments_not_reset", {{"msg", why}});
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
  Subjects_.CarryFrame();
  Glass_.CarryFrame();

  MvpCamRel(PrevMvp16_,
            Right_,
            Up_,
            Fwd_,
            PictureW(),
            PictureH(),
            FovDeg_,
            OrthoM_,
            Jitter_[0],
            Jitter_[1],
            NearM_);
  Submitted_ = true;
}

void SceneRenderer::WaitForGpu() {
  if (!Ready_) { return; }
  SDL_WaitForGPUIdle(Device_.Get());

  for (SDL_GPUFence *&held : Landed_) {
    if (held == nullptr) { continue; }
    SDL_ReleaseGPUFence(Device_.Get(), held);
    held = nullptr;
  }
}

void SceneRenderer::WantsPixels() {
  Wanted_ = true;
}

ReadState SceneRenderer::ReadPixels(std::vector<uint8_t> &rgba) {
  if (!Ready_) { return ReadState::Failed; }
  const auto asRgba = [](std::vector<uint8_t> &held, SDL_GPUTextureFormat holds) {
    if (holds != SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM &&
        holds != SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB) {
      return;
    }
    for (size_t at = 0; at + 3 < held.size(); at += 4) { std::swap(held[at], held[at + 2]); }
  };
  if (Showing_ == nullptr) {
    SDL_GPUTexture *const held = FrameTex_.Get() != nullptr ? FrameTex_.Get() : HostSurface_;
    if (held == nullptr) { return ReadState::Failed; }
    Readback read;
    if (read.FromTexture(Device_.Get(), held, (uint32_t)Width_, (uint32_t)Height_, 4u) !=
        ReadState::Ready) {
      return ReadState::Failed;
    }
    rgba.resize((size_t)Width_ * (size_t)Height_ * 4u);
    std::memcpy(rgba.data(), read.Rows(), rgba.size());
    asRgba(rgba,
           Plan_ ? FormatOf(
                       Plan_->Format(held == HostSurface_ ? Resource::Surface : Resource::FrameTex))
                 : SDL_GPU_TEXTUREFORMAT_INVALID);
    return ReadState::Ready;
  }
  if (Taken_.size() == (size_t)Width_ * (size_t)Height_ * 4u) {
    rgba = Taken_;
    asRgba(rgba, SurfaceFormat());
    return ReadState::Ready;
  }
  Wanted_ = true;
  return ReadState::Failed;
}

ReadState SceneRenderer::ReadDepth(std::vector<float> &depth) {
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

ReadState SceneRenderer::ReadSceneLinear(std::vector<float> &rgba) {
  SDL_GPUTexture *source = LinearSource();
  if (!Ready_ || !source) { return ReadState::Failed; }
  const bool wide = Plan_->Format(Resource::SceneLinear) == TexelFormat::Rgba32Float;
  Readback read;
  if (read.FromTexture(
          Device_.Get(), source, (uint32_t)Width_, (uint32_t)Height_, wide ? 16u : 8u) !=
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

ReadState SceneRenderer::ReadShadowAtlas(std::vector<float> &depth) {
  if (!Ready_ || !ShadowAtlas_) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.Get(),
                       ShadowAtlas_.Get(),
                       (uint32_t)kShadowAtlasPx,
                       (uint32_t)kShadowAtlasPx,
                       4u) != ReadState::Ready) {
    return ReadState::Failed;
  }
  depth.resize((size_t)kShadowAtlasPx * (size_t)kShadowAtlasPx);
  std::memcpy(depth.data(), read.Rows(), depth.size() * sizeof(float));
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadSkyIrradiance(float out[kIrradianceFloats]) {
  if (!Ready_ || !Irradiance_) { return ReadState::Failed; }
  Readback read;
  if (read.FromBuffer(Device_.Get(),
                      Irradiance_.Get(),
                      kIrradianceFloats * (uint32_t)sizeof(float)) != ReadState::Ready) {
    return ReadState::Failed;
  }
  std::memcpy(out, read.Rows(), kIrradianceFloats * sizeof(float));
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadShadingNormal(std::vector<float> &xyz) {
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

ReadState SceneRenderer::ReadSceneVelocity(std::vector<float> &xy) {
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

ReadState SceneRenderer::ReadSurfaceIdentity(std::vector<float> &slot) {
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

void SceneRenderer::StopShowing() {
  if (Offscreen_ != nullptr) {
    if (Device_.Get() != nullptr) { SDL_ReleaseGPUTexture(Device_.Get(), Offscreen_); }
    Offscreen_ = nullptr;
    HostSurface_ = nullptr;
  }
  if (Showing_ == nullptr) { return; }
  if (Device_.Get() != nullptr) { SDL_ReleaseWindowFromGPUDevice(Device_.Get(), Showing_); }
  Showing_ = nullptr;
}

std::expected<void, std::string_view>
SceneRenderer::DrawsInto(int widthPx, int heightPx, SDL_Window *presents) {
  if (widthPx <= 0 || heightPx <= 0) {
    return std::unexpected("a canvas has an extent, and this one declares none");
  }
  if (!Stands()) { return std::unexpected("the renderer has no device to stand a canvas on"); }

  if (Showing_ != presents) {
    StopShowing();
    if (presents != nullptr && !SDL_ClaimWindowForGPUDevice(Device_.Get(), presents)) {
      WhyNot_ = std::string("the window was refused by the device: ") + SDL_GetError();
      return std::unexpected(
          "the window was refused by the device, and WhyNot carries what it said");
    }
    if (presents != nullptr) {
      const SDL_GPUSwapchainComposition wanted = SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR;
      if (!SDL_WindowSupportsGPUSwapchainComposition(Device_.Get(), presents, wanted)) {
        WhyNot_ = std::string("this window cannot present the transfer the plan declares: ") +
                  SDL_GetError();
        return std::unexpected(
            "the window cannot present the transfer the plan declares, and WhyNot carries what "
            "the device said");
      }
      const SDL_GPUPresentMode unqueued[] = {
          SDL_GPU_PRESENTMODE_MAILBOX, SDL_GPU_PRESENTMODE_IMMEDIATE, SDL_GPU_PRESENTMODE_VSYNC};
      bool took = false;
      for (const SDL_GPUPresentMode mode : unqueued) {
        if (!SDL_WindowSupportsGPUPresentMode(Device_.Get(), presents, mode)) { continue; }
        took = SDL_SetGPUSwapchainParameters(Device_.Get(), presents, wanted, mode);
        if (took) {
          Presenting_ = mode;
          break;
        }
      }
      if (!took) {
        WhyNot_ =
            std::string("this window took no present mode the device offers: ") + SDL_GetError();
        return std::unexpected(
            "the window took no present mode, and WhyNot carries what the device said");
      }
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
  if (const auto laid = StandsOffscreen(); !laid) { return std::unexpected(laid.error()); }
  return {};
}

std::expected<std::optional<SceneRenderer::Shown>, std::string_view>
SceneRenderer::Presented() const {
  if (Showing_ == nullptr) {
    return std::unexpected(
        "no window is being shown on: a frame is presented to a surface the caller declared, "
        "and `DrawsInto` names one");
  }
  if (Shown_.WidthPx == 0) { return std::optional<Shown>(); }
  return std::optional<Shown>(Shown_);
}

} // namespace outshine::Render
