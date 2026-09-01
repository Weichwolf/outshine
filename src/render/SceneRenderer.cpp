#include "Units.h"
#include "math/Mat4.h"
#include "math/Vec2.h"
#include "Heap.h"
#include <span>
#include <array>
#include <chrono>

#include "SceneRenderer.h"
#include "math/Vec3.h"

#include <cstdint>
#include <expected>
#include <memory>
#include <numbers>
#include <cmath>
#include <cstring>

#include <SDL3/SDL.h>
#include <string_view>
#include <utility>
#include <ratio>
#include <vector>
#include <optional>

#include "Log.h"
#include "stages/DepthPyramid.h"
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

void MvpCamRel(Mat4f &m,
               const Vec3 &R,
               const Vec3 &Uc,
               const Vec3 &F,
               double w,
               double h,
               float fovDeg,
               float orthoM,
               float jitterX,
               float jitterY,
               float nearM) {
  const float fov = fovDeg * static_cast<float>(kDeg2Rad);
  const float asp = static_cast<float>(w) / static_cast<float>(h);
  const float zn = nearM;
  const float f = 1.0f / std::tan(fov / 2.0f);
  const Mat4f v = {{static_cast<float>(R[0]),
                    static_cast<float>(Uc[0]),
                    -static_cast<float>(F[0]),
                    0,
                    static_cast<float>(R[1]),
                    static_cast<float>(Uc[1]),
                    -static_cast<float>(F[1]),
                    0,
                    static_cast<float>(R[2]),
                    static_cast<float>(Uc[2]),
                    -static_cast<float>(F[2]),
                    0,
                    0,
                    0,
                    0,
                    1}};

  Mat4f p = {{f / asp, 0, 0, 0, 0, f, 0, 0, 0, 0, 0, -1, 0, 0, zn, 0}};

  const float ndcX = w > 0 ? 2.0f * jitterX / static_cast<float>(w) : 0.0f;
  const float ndcY = h > 0 ? 2.0f * jitterY / static_cast<float>(h) : 0.0f;
  p[8] = -ndcX;
  p[9] = -ndcY;
  if (orthoM > 0.0f) {
    const float hw = 0.5f * orthoM * asp;
    const float hh = 0.5f * orthoM;
    const float zf = 60000.0f;
    const float rz = 1.0f / (zf - zn);
    Mat4f q = {{1.0f / hw, 0, 0, 0, 0, 1.0f / hh, 0, 0, 0, 0, rz, 0, 0, 0, zf * rz, 1}};
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
  const auto held = static_cast<uint32_t>(bits);
  const uint32_t sign = (held & 0x8000u) << 16u;
  const uint32_t exponent = (held >> 10u) & 0x1Fu;
  uint32_t mantissa = held & 0x3FFu;
  uint32_t assembled = 0;
  if (exponent == 0) {
    if (mantissa != 0) {
      int shift = 0;
      while ((mantissa & 0x400u) == 0) {
        mantissa <<= 1u;
        ++shift;
      }
      mantissa &= 0x3FFu;
      assembled = (static_cast<uint32_t>(127 - 15 - shift + 1) << 23u) | (mantissa << 13u);
    }
  } else if (exponent == 0x1Fu) {
    assembled = 0x7F800000u | (mantissa << 13u);
  } else {
    assembled = ((exponent + 127 - 15) << 23u) | (mantissa << 13u);
  }
  assembled |= sign;
  float value = 0;
  std::memcpy(&value, &assembled, sizeof value);
  return value;
}

} // namespace

void SceneRenderer::SetCameraBasis(const Vec3 &eye,
                                   const Vec3 &fwd,
                                   const Vec3 &right,
                                   const Vec3 &up) {
  for (int axis = 0; axis < 3; axis++) {
    Eye_[axis] = eye[axis];
    Fwd_[axis] = fwd[axis];
    Right_[axis] = right[axis];
    Up_[axis] = up[axis];
  }
  CameraFull_ = true;
}

const std::array<SceneRenderer::Executor, SceneRenderer::kExecutorCount> SceneRenderer::kExecutors =
    {{
        {.Named = Stage::MediumTransmittance,
         .Configure = &SceneRenderer::ConfigureMediumTransmittance,
         .Encode = &SceneRenderer::EncodeMediumTransmittance},
        {.Named = Stage::MediumMultiScatter,
         .Configure = &SceneRenderer::ConfigureMediumMultiScatter,
         .Encode = &SceneRenderer::EncodeMediumMultiScatter},
        {.Named = Stage::MediumRadiance,
         .Configure = &SceneRenderer::ConfigureMediumRadiance,
         .Encode = &SceneRenderer::EncodeMediumRadiance},
        {.Named = Stage::Irradiance,
         .Configure = &SceneRenderer::ConfigureIrradiance,
         .Encode = &SceneRenderer::EncodeIrradiance},
        {.Named = Stage::SubjectCull,
         .Configure = &SceneRenderer::ConfigureSubjectCull,
         .Encode = &SceneRenderer::EncodeSubjectCull},
        {.Named = Stage::SubjectScan,
         .Configure = &SceneRenderer::ConfigureSubjectCull,
         .Encode = &SceneRenderer::EncodeSubjectScan},
        {.Named = Stage::SubjectCompact,
         .Configure = &SceneRenderer::ConfigureSubjectCull,
         .Encode = &SceneRenderer::EncodeSubjectCompact},
        {.Named = Stage::LightVisibility,
         .Configure = &SceneRenderer::ConfigureLightVisibility,
         .Encode = &SceneRenderer::EncodeLightVisibility},
        {.Named = Stage::Sky,
         .Configure = &SceneRenderer::ConfigureSky,
         .Encode = &SceneRenderer::EncodeSky},
        {.Named = Stage::Subjects,
         .Configure = &SceneRenderer::ConfigureSubjects,
         .Encode = &SceneRenderer::EncodeSubjects},
        {.Named = Stage::SubjectsTransmissive,
         .Configure = &SceneRenderer::ConfigureGlass,
         .Encode = &SceneRenderer::EncodeGlass},
        {.Named = Stage::CompositeTransmission,
         .Configure = &SceneRenderer::ConfigureCompositeTransmission,
         .Encode = &SceneRenderer::EncodeCompositeTransmission},
        {.Named = Stage::AerialPerspective,
         .Configure = &SceneRenderer::ConfigureAerialPerspective,
         .Encode = &SceneRenderer::EncodeAerialPerspective},
        {.Named = Stage::DepthPyramid,
         .Configure = &SceneRenderer::ConfigureDepthPyramid,
         .Encode = &SceneRenderer::EncodeDepthPyramid},
        {.Named = Stage::TemporalResolve,
         .Configure = &SceneRenderer::ConfigureTemporalResolve,
         .Encode = nullptr},
        {.Named = Stage::Tonemap,
         .Configure = &SceneRenderer::ConfigureTonemap,
         .Encode = &SceneRenderer::EncodeTonemap},
        {.Named = Stage::Overlay,
         .Configure = &SceneRenderer::ConfigureOverlay,
         .Encode = &SceneRenderer::EncodeOverlay},
        {.Named = Stage::Present,
         .Configure = &SceneRenderer::ConfigurePresent,
         .Encode = &SceneRenderer::EncodePresent},
    }};

const SceneRenderer::Executor *SceneRenderer::ExecutorOf(Stage stage) {
  for (const Executor &one : kExecutors) {
    if (one.Named == stage) { return &one; }
  }
  return nullptr;
}

bool SceneRenderer::Executable(Stage stage) {
  return ExecutorOf(stage) != nullptr;
}

bool SceneRenderer::Stands() {
  if (Device_) { return true; }
  if (SDL_WasInit(SDL_INIT_VIDEO) == 0u) {
    Log::Error("render", "no_video", {{"msg", "the client did not initialise SDL video"}});
    WhyNot_ =
        "SDL's video subsystem is not running: outshine renders through SDL3 and the CLIENT owns "
        "the process, so the client calls SDL_Init(SDL_INIT_VIDEO) before it declares a scenario";
    return false;
  }
  SDL_GPUDevice *device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, kGpuValidation, nullptr);
  if (device == nullptr) {
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
  wanted.width = static_cast<Uint32>(Width_);
  wanted.height = static_cast<Uint32>(Height_);
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
    const auto id = static_cast<Resource>(r);
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
             {"stages", static_cast<int>(Plan_->Order().size())},
             {"f32filter", Handles_.FiltersFloat32}});
  for (size_t at = 0; at < kStageCount; ++at) {
    const auto stage = static_cast<Stage>(at);
    if (Executable(stage)) { continue; }
    Log::Info("render", "stage_without_a_body", {{"stage", Row(stage).Name}});
  }
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
    wanted.width = static_cast<uint32_t>(Width_);
    wanted.height = static_cast<uint32_t>(Height_);
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
      wanted.size = kIrradianceFloats * static_cast<uint32_t>(sizeof(float));
      IrradianceBuffer_ =
          OwnedBuffer(Handles_.Device, SDL_CreateGPUBuffer(Handles_.Device, &wanted));
      return;
    }
    case Resource::DepthPyramid: {
      SDL_GPUBufferCreateInfo wanted{};
      wanted.usage =
          SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
      wanted.size =
          PyramidOver(static_cast<uint32_t>(Width_), static_cast<uint32_t>(Height_)).Texels *
          static_cast<uint32_t>(sizeof(float));
      Pyramid_ = OwnedBuffer(Handles_.Device, SDL_CreateGPUBuffer(Handles_.Device, &wanted));
      return;
    }
    case Resource::VegetationTable:
    case Resource::Meter:
    case Resource::ShadowAtlas: {
      SDL_GPUTextureCreateInfo wanted{};
      wanted.type = SDL_GPU_TEXTURETYPE_2D;
      wanted.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
      wanted.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
      wanted.width = static_cast<uint32_t>(kShadowAtlasPx);
      wanted.height = static_cast<uint32_t>(kShadowAtlasPx);
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
    case Resource::DepthPyramid:
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
    case Resource::IrradianceBuffer: return IrradianceBuffer_.Get();
    case Resource::DepthPyramid: return Pyramid_.Get();
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
  out.WidthPx = static_cast<double>(Width_);
  out.HeightPx = static_cast<double>(Height_);
  if (RegionW_ > 0 && RegionH_ > 0) {
    out.LeftPx = RegionX_ * static_cast<double>(Width_);
    out.TopPx = RegionY_ * static_cast<double>(Height_);
    out.WidthPx = RegionW_ * static_cast<double>(Width_);
    out.HeightPx = RegionH_ * static_cast<double>(Height_);
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
  where.x = picture ? static_cast<float>(rect.LeftPx) : 0.0f;
  where.y = picture ? static_cast<float>(rect.TopPx) : 0.0f;
  where.w = picture ? static_cast<float>(rect.WidthPx) : static_cast<float>(Width_);
  where.h = picture ? static_cast<float>(rect.HeightPx) : static_cast<float>(Height_);
  where.min_depth = 0.0f;
  where.max_depth = 1.0f;
  if (into.Pass != nullptr) { SDL_SetGPUViewport(into.Pass, &where); }
}

FrameContext SceneRenderer::Framing() const {
  FrameContext ctx{};
  for (int axis = 0; axis < 3; axis++) { ctx.PreViewTranslation[axis] = -Eye_[axis]; }

  MvpCamRel(ctx.Mvp,
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
  for (int at = 0; at < 16; at++) { ctx.PrevMvp[at] = Submitted_ ? PrevMvp_[at] : ctx.Mvp[at]; }
  return ctx;
}

void SceneRenderer::EncodeStage(Stage stage, const PassRecording &into) {
  const Executor *seat = ExecutorOf(stage);
  if (seat == nullptr || seat->Encode == nullptr) { return; }

  const FrameContext ctx = Framing();

  const outshine::Heap::Tagged encoding(Row(stage).Name);
  const auto began = std::chrono::steady_clock::now();
  (this->*(seat->Encode))(ctx, into);
  Effort &spent = Spent_[static_cast<size_t>(stage)];
  spent.TookMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  spent.Draws = 0;
  spent.Triangles = 0;
  spent.Surfaces = 0;
  spent.Placements = 0;
  if (stage == Stage::Subjects || stage == Stage::SubjectsTransmissive) {
    const SubjectDraw &drew = stage == Stage::Subjects ? Subjects_ : Glass_;
    uint32_t surfaces = 0;
    uint32_t placements = 0;
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
  const Vec2f delta = {{Jitter_[0] - PrevJitter_[0], Jitter_[1] - PrevJitter_[1]}};
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

namespace {

constexpr uint32_t kLeastGroundBytes = 16;

[[nodiscard]] bool StandsBuffer(SDL_GPUDevice *device,
                                OwnedBuffer &held,
                                uint32_t &heldBytes,
                                uint32_t bytes,
                                const void *from,
                                const char *what,
                                std::string &error) {
  if (!held || heldBytes < bytes) {
    SDL_GPUBufferCreateInfo wanted{};
    wanted.usage = static_cast<SDL_GPUBufferUsageFlags> SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
    wanted.size = bytes;
    SDL_GPUBuffer *made = SDL_CreateGPUBuffer(device, &wanted);
    if (made == nullptr) {
      error = std::string("the ") + what + " has no buffer: " + SDL_GetError();
      return false;
    }
    held = OwnedBuffer(device, made);
    heldBytes = bytes;
  }
  if (from == nullptr || bytes == 0) { return true; }
  SDL_GPUTransferBufferCreateInfo wantedTransfer{};
  wantedTransfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  wantedTransfer.size = bytes;
  SDL_GPUTransferBuffer *staging = SDL_CreateGPUTransferBuffer(device, &wantedTransfer);
  if (staging == nullptr) {
    error = std::string("the ") + what + " has no staging buffer: " + SDL_GetError();
    return false;
  }
  std::memcpy(SDL_MapGPUTransferBuffer(device, staging, false), from, bytes);
  SDL_UnmapGPUTransferBuffer(device, staging);
  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device);
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
  const SDL_GPUTransferBufferLocation source{.transfer_buffer = staging, .offset = 0};
  const SDL_GPUBufferRegion into{.buffer = held.Get(), .offset = 0, .size = bytes};
  SDL_UploadToGPUBuffer(copy, &source, &into, false);
  SDL_EndGPUCopyPass(copy);
  SDL_SubmitGPUCommandBuffer(commands);
  SDL_ReleaseGPUTransferBuffer(device, staging);
  return true;
}

} // namespace

bool SceneRenderer::SetGroundClasses(const uint32_t *words,
                                     size_t wordCount,
                                     const float *palette,
                                     size_t paletteFloats,
                                     std::string &error) {
  if (Handles_.Device == nullptr) { return true; }
  const size_t wanted = wordCount * sizeof(uint32_t);
  const size_t wantedPalette = paletteFloats * sizeof(float);
  const auto classBytes = static_cast<uint32_t>(
      wanted > kLeastGroundBytes ? wanted : static_cast<size_t>(kLeastGroundBytes));
  const auto paletteBytes = static_cast<uint32_t>(
      wantedPalette > kLeastGroundBytes ? wantedPalette : static_cast<size_t>(kLeastGroundBytes));
  if (!StandsBuffer(Handles_.Device,
                    GroundClasses_,
                    GroundClassBytes_,
                    classBytes,
                    words,
                    "ground class structure",
                    error) ||
      !StandsBuffer(Handles_.Device,
                    GroundPalette_,
                    GroundPaletteBytes_,
                    paletteBytes,
                    palette,
                    "ground palette",
                    error)) {
    return false;
  }
  Subjects_.GroundFrom(GroundClasses_.Get(), GroundPalette_.Get());
  if (DrawsGlass_) { Glass_.GroundFrom(GroundClasses_.Get(), GroundPalette_.Get()); }
  return true;
}

bool SceneRenderer::ConfigureIrradiance(std::string &error) {
  Subjects_.SkyFrom(IrradianceBuffer_.Get());
  if (DrawsGlass_) { Glass_.SkyFrom(IrradianceBuffer_.Get()); }
  {
    std::string ignored;
    (void)SetGroundClasses(nullptr, 0, nullptr, 0, ignored);
  }
  return SkyIrradianceStage_.Configure(Handles_,
                                       TransmittanceLut_.Get(),
                                       MultiScatterLut_.Get(),
                                       LutSamp_.Get(),
                                       IrradianceBuffer_.Get(),
                                       error);
}

void SceneRenderer::EncodeIrradiance(const FrameContext &ctx, const PassRecording &into) {
  (void)ctx;
  SkyIrradianceStage_.Encode(into);
}

bool SceneRenderer::ConfigureDepthPyramid(std::string &error) {
  return PyramidStage_.Configure(
      Handles_, DepthTex_.Get(), Samp_.Get(), Pyramid_.Get(), Width_, Height_, error);
}

void SceneRenderer::EncodeDepthPyramid(const FrameContext &ctx, const PassRecording &into) {
  (void)ctx;
  PyramidStage_.Encode(into);
}

bool SceneRenderer::ConfigureSubjectCull(std::string &error) {
  Cull_.PyramidFrom(Pyramid_.Get(),
                    PyramidOver(static_cast<uint32_t>(Width_), static_cast<uint32_t>(Height_)));
  return Cull_.Configure(Subjects_, Handles_, error);
}

void SceneRenderer::EncodeSubjectCull(const FrameContext &ctx, const PassRecording &into) {
  Cull_.Projects(static_cast<float>(Height_));
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
  Touched_[static_cast<size_t>(Resource::ShadowAtlas)] = Shadow_.Cached();
}

void SceneRenderer::EncodeAerialPerspective(const FrameContext &ctx, const PassRecording &into) {
  Picture(true, into);
  const float tanHalfH = std::tan(static_cast<float>(FovDeg_ * kDeg2Rad) * 0.5f);
  const float tanHalfW =
      tanHalfH * (PictureH() > 0.0 ? static_cast<float>(PictureW() / PictureH()) : 1.0f);
  Vec3f right;
  Vec3f up;
  Vec3f fwd;
  for (int axis = 0; axis < 3; ++axis) {
    right[axis] = static_cast<float>(Right_[axis]);
    up[axis] = static_cast<float>(Up_[axis]);
    fwd[axis] = static_cast<float>(Fwd_[axis]);
  }
  Aerial_.SetBasis(right, up, fwd, tanHalfW, tanHalfH);
  Aerial_.SetNear(NearMetres());
  Aerial_.Encode(ctx, into);
}

void SceneRenderer::EncodeSky(const FrameContext &ctx, const PassRecording &into) {
  Picture(true, into);
  const float tanHalfH = std::tan(static_cast<float>(FovDeg_ * kDeg2Rad) * 0.5f);
  const float tanHalfW =
      tanHalfH * (PictureH() > 0.0 ? static_cast<float>(PictureW() / PictureH()) : 1.0f);
  Vec3f right;
  Vec3f up;
  Vec3f fwd;
  for (int axis = 0; axis < 3; ++axis) {
    right[axis] = static_cast<float>(Right_[axis]);
    up[axis] = static_cast<float>(Up_[axis]);
    fwd[axis] = static_cast<float>(Fwd_[axis]);
  }
  Sky_.SetBasis(right, up, fwd, tanHalfW, tanHalfH);
  Sky_.Encode(ctx, into);
}

static float RadicalInverse(int index, int base) {
  float result = 0.0f;
  float weight = 1.0f / static_cast<float>(base);
  int at = index + 1;
  while (at > 0) {
    result += weight * static_cast<float>(at % base);
    at /= base;
    weight /= static_cast<float>(base);
  }
  return result;
}

void SceneRenderer::EncodePass(SDL_GPUCommandBuffer *commands, size_t pass) {
  const Compiled::Pass &declared = Plan_->Passes()[pass];
  if (declared.Kind == PassKind::Compute) {
    std::array<SDL_GPUStorageTextureReadWriteBinding, kMaxColourAttachments> written = {{}};
    uint32_t writtenCount = 0;
    for (const Resource wanted : declared.Targets) {
      SDL_GPUStorageTextureReadWriteBinding &binding = written[writtenCount++];
      binding.texture = Target(wanted);
      binding.cycle = false;
    }

    std::array<SDL_GPUStorageBufferReadWriteBinding, kMaxColourAttachments> tables = {{}};
    uint32_t tableCount = 0;
    for (const Resource wanted : declared.Buffers) {
      SDL_GPUBuffer *const held = BufferFor(wanted);
      if (held == nullptr) { continue; }
      SDL_GPUStorageBufferReadWriteBinding &binding = tables[tableCount++];
      binding.buffer = held;
      binding.cycle = false;
    }
    const PassRecording into{
        .Commands = commands,
        .Pass = nullptr,
        .Dispatch = SDL_BeginGPUComputePass(
            commands, written.data(), writtenCount, tables.data(), tableCount)};
    for (size_t at = 0; at < declared.Count; ++at) {
      EncodeStage(Plan_->Order()[declared.First + at], into);
    }
    SDL_EndGPUComputePass(into.Dispatch);
    return;
  }
  std::array<SDL_GPUColorTargetInfo, kMaxColourAttachments> colours = {{}};
  uint32_t colourCount = 0;
  for (const Resource wanted : declared.Targets) {
    SDL_GPUColorTargetInfo &attachment = colours[colourCount++];
    attachment.texture = Target(wanted);
    attachment.load_op =
        Touched_[static_cast<size_t>(wanted)] ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
    Touched_[static_cast<size_t>(wanted)] = true;
    attachment.store_op = Plan_->Stored(wanted) ? SDL_GPU_STOREOP_STORE : SDL_GPU_STOREOP_DONT_CARE;

    const bool carriesCoverage =
        wanted == Resource::SceneHdr || wanted == Resource::SceneComposited ||
        wanted == Resource::SceneTransmissive || wanted == Resource::SceneLinear;
    attachment.clear_color =
        wanted == Resource::SceneVelocity
            ? SDL_FColor{.r = kVelocityStatic, .g = kVelocityStatic, .b = 0, .a = 0}
            : SDL_FColor{.r = 0, .g = 0, .b = 0, .a = carriesCoverage ? 0.0f : 1.0f};
  }
  SDL_GPUDepthStencilTargetInfo depth{};
  if (declared.Depth != kNoEdge) {
    depth.texture = Target(declared.Depth);
    depth.load_op =
        Touched_[static_cast<size_t>(declared.Depth)] ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
    Touched_[static_cast<size_t>(declared.Depth)] = true;
    depth.store_op =
        Plan_->Stored(declared.Depth) ? SDL_GPU_STOREOP_STORE : SDL_GPU_STOREOP_DONT_CARE;
    depth.clear_depth = 0.0f;
    depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
  }
  const PassRecording into{
      .Commands = commands,
      .Pass = SDL_BeginGPURenderPass(
          commands, colours.data(), colourCount, declared.Depth != kNoEdge ? &depth : nullptr),
      .Dispatch = nullptr};
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
    Uint32 gotW = 0;
    Uint32 gotH = 0;
    if (SDL_WaitAndAcquireGPUSwapchainTexture(commands, Showing_, &swapchain, &gotW, &gotH) &&
        swapchain != nullptr) {
      Shown_.WidthPx = static_cast<int>(gotW);
      Shown_.HeightPx = static_cast<int>(gotH);
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
    wanted.size =
        static_cast<Uint32>(static_cast<size_t>(Width_) * static_cast<size_t>(Height_) * 4u);
    taking = SDL_CreateGPUTransferBuffer(Device_.Get(), &wanted);
    if (taking != nullptr) {
      SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
      SDL_GPUTextureRegion region{};
      region.texture = HostSurface_;
      region.w = static_cast<Uint32>(Width_);
      region.h = static_cast<Uint32>(Height_);
      region.d = 1;
      SDL_GPUTextureTransferInfo into{};
      into.transfer_buffer = taking;
      into.pixels_per_row = static_cast<Uint32>(Width_);
      into.rows_per_layer = static_cast<Uint32>(Height_);
      SDL_DownloadFromGPUTexture(copy, &region, &into);
      SDL_EndGPUCopyPass(copy);
    }
  }

  Landed_[LandedAt_] = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
  if (taking != nullptr) {
    SDL_WaitForGPUFences(Device_.Get(), true, &Landed_[LandedAt_], 1);
    if (const void *pixels = SDL_MapGPUTransferBuffer(Device_.Get(), taking, false)) {
      const auto *bytes = static_cast<const uint8_t *>(pixels);
      Taken_.assign(bytes, bytes + static_cast<size_t>(Width_) * static_cast<size_t>(Height_) * 4u);
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

  MvpCamRel(PrevMvp_,
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
    if (read.FromTexture(Device_.Get(),
                         held,
                         static_cast<uint32_t>(Width_),
                         static_cast<uint32_t>(Height_),
                         4u) != ReadState::Ready) {
      return ReadState::Failed;
    }
    rgba.resize(static_cast<size_t>(Width_) * static_cast<size_t>(Height_) * 4u);
    std::memcpy(rgba.data(), read.Rows(), rgba.size());
    asRgba(rgba,
           Plan_ ? FormatOf(
                       Plan_->Format(held == HostSurface_ ? Resource::Surface : Resource::FrameTex))
                 : SDL_GPU_TEXTUREFORMAT_INVALID);
    return ReadState::Ready;
  }
  if (Taken_.size() == static_cast<size_t>(Width_) * static_cast<size_t>(Height_) * 4u) {
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
  if (read.FromTexture(Device_.Get(),
                       DepthTex_.Get(),
                       static_cast<uint32_t>(Width_),
                       static_cast<uint32_t>(Height_),
                       4u) != ReadState::Ready) {
    return ReadState::Failed;
  }
  depth.resize(static_cast<size_t>(Width_) * static_cast<size_t>(Height_));
  std::memcpy(depth.data(), read.Rows(), depth.size() * sizeof(float));
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadSceneLinear(std::vector<float> &rgba) {
  SDL_GPUTexture *source = LinearSource();
  if (!Ready_ || (source == nullptr)) { return ReadState::Failed; }
  const bool wide = Plan_->Format(Resource::SceneLinear) == TexelFormat::Rgba32Float;
  Readback read;
  if (read.FromTexture(Device_.Get(),
                       source,
                       static_cast<uint32_t>(Width_),
                       static_cast<uint32_t>(Height_),
                       wide ? 16u : 8u) != ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components = static_cast<size_t>(Width_) * static_cast<size_t>(Height_) * 4u;
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
                       static_cast<uint32_t>(kShadowAtlasPx),
                       static_cast<uint32_t>(kShadowAtlasPx),
                       4u) != ReadState::Ready) {
    return ReadState::Failed;
  }
  depth.resize(static_cast<size_t>(kShadowAtlasPx) * static_cast<size_t>(kShadowAtlasPx));
  std::memcpy(depth.data(), read.Rows(), depth.size() * sizeof(float));
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadKeptIndices(uint32_t &kept, uint32_t &batches) {
  kept = 0;
  batches = 0;
  const SubjectResidency &resident = Subjects_.Resident();
  SDL_GPUBuffer *const args = resident.DrawArgs.Get();
  const uint32_t rows = Subjects_.ClusterBatchRows();
  if (!Ready_ || args == nullptr || rows == 0) { return ReadState::Failed; }
  Readback read;
  const uint32_t bytes = rows * 5u * static_cast<uint32_t>(sizeof(uint32_t));
  if (read.FromBuffer(Device_.Get(), args, bytes) != ReadState::Ready) { return ReadState::Failed; }
  const auto *const held = static_cast<const uint32_t *>(static_cast<const void *>(read.Rows()));
  for (uint32_t at = 0; at < rows; ++at) {
    kept += held[at * 5u];
    batches += held[at * 5u] > 0u ? 1u : 0u;
  }
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadPyramid(float &nearest, float &farthest, float &mean) {
  nearest = 0.0f;
  farthest = 1.0f;
  mean = 0.0f;
  if (!Ready_ || !Pyramid_) { return ReadState::Failed; }
  const PyramidShape shape =
      PyramidOver(static_cast<uint32_t>(Width_), static_cast<uint32_t>(Height_));
  const uint32_t texels = shape.Wide[0] * shape.High[0];
  if (texels == 0) { return ReadState::Failed; }
  Readback read;
  if (read.FromBuffer(Device_.Get(),
                      Pyramid_.Get(),
                      texels * static_cast<uint32_t>(sizeof(float))) != ReadState::Ready) {
    return ReadState::Failed;
  }
  const auto *const held = static_cast<const float *>(static_cast<const void *>(read.Rows()));
  double summed = 0.0;
  nearest = held[0];
  farthest = held[0];
  for (uint32_t at = 0; at < texels; ++at) {
    nearest = held[at] > nearest ? held[at] : nearest;
    farthest = held[at] < farthest ? held[at] : farthest;
    summed += static_cast<double>(held[at]);
  }
  mean = static_cast<float>(summed / static_cast<double>(texels));
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadSkyIrradiance(std::span<float, kIrradianceFloats> out) {
  if (!Ready_ || !IrradianceBuffer_) { return ReadState::Failed; }
  Readback read;
  if (read.FromBuffer(Device_.Get(),
                      IrradianceBuffer_.Get(),
                      kIrradianceFloats * static_cast<uint32_t>(sizeof(float))) !=
      ReadState::Ready) {
    return ReadState::Failed;
  }
  std::memcpy(out.data(), read.Rows(), kIrradianceFloats * sizeof(float));
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadShadingNormal(std::vector<float> &xyz) {
  SDL_GPUTexture *source = ShadingNormalTex_.Get();
  if (!Ready_ || (source == nullptr)) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.Get(),
                       source,
                       static_cast<uint32_t>(Width_),
                       static_cast<uint32_t>(Height_),
                       8u) != ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components = static_cast<size_t>(Width_) * static_cast<size_t>(Height_) * 4u;
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
  if (!Ready_ || (source == nullptr)) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.Get(),
                       source,
                       static_cast<uint32_t>(Width_),
                       static_cast<uint32_t>(Height_),
                       4u) != ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components = static_cast<size_t>(Width_) * static_cast<size_t>(Height_) * 2u;
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
  if (!Ready_ || (source == nullptr)) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.Get(),
                       source,
                       static_cast<uint32_t>(Width_),
                       static_cast<uint32_t>(Height_),
                       16u) != ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components = static_cast<size_t>(Width_) * static_cast<size_t>(Height_) * 4u;
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
      std::array<const SDL_GPUPresentMode, 3> unqueued = {
          {SDL_GPU_PRESENTMODE_MAILBOX, SDL_GPU_PRESENTMODE_IMMEDIATE, SDL_GPU_PRESENTMODE_VSYNC}};
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
