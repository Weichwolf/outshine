#include <SDL3_shadercross/SDL_shadercross.h>
#include "math/Units.h"
#include "math/Mat4.h"
#include "math/Vec2.h"
#include "Heap.h"
#include <algorithm>
#include <cassert>
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

namespace Says {
constexpr auto kNoReadbackFrame = "no submitted image is available for colour readback";
constexpr auto kNoColourOutput = "the render plan has no final colour output";
constexpr auto kNoColourTexture = "the final colour output has no owned texture";
constexpr auto kColourReadbackFailed = "colour readback failed: ";

constexpr auto kGpuWaitFailed = "GPU idle wait failed: ";
constexpr auto kRendererNotReady = "GPU renderer is not initialized";
constexpr auto kCameraNotConfigured = "render camera is not configured";
constexpr auto kInvalidTargetExtent = "render target dimensions must be positive";
constexpr auto kTargetTextureFailed = "could not create the offscreen target: ";
constexpr auto kWindowClaimFailed = "could not claim the target window: ";
constexpr auto kUnsupportedTransfer = "the window does not support linear SDR presentation";
constexpr auto kNoPresentMode = "the window supports none of the requested present modes";
constexpr auto kPresentModeFailed = "could not configure the swapchain: ";
}

constexpr uint32_t kHalfSignBit = 0x8000u;
constexpr unsigned kHalfSignShift = 16u;
constexpr unsigned kHalfMantissaBits = 10u;
constexpr uint32_t kHalfExponentMask = 0x1Fu;
constexpr uint32_t kHalfMantissaMask = 0x3FFu;
constexpr uint32_t kHalfHiddenBit = 0x400u;
constexpr int kHalfExponentBias = 15;
constexpr int kSingleExponentBias = 127;
constexpr unsigned kSingleMantissaBits = 23u;
constexpr unsigned kMantissaWiden = kSingleMantissaBits - kHalfMantissaBits;
constexpr uint32_t kSingleInfinity = 0x7F800000u;

namespace {

#ifdef OUTSHINE_GPU_VALIDATION
constexpr bool kGpuValidation = true;
#else
constexpr bool kGpuValidation = false;
#endif
}

namespace {

Mat4f MvpCamRel(const CameraBasis &stands, const Lens &through) {
  const Vec3 &right = stands.Right;
  const Vec3 &up = stands.Up;
  const Vec3 &forward = stands.Forward;
  const Mat4f v = {{static_cast<float>(right[0]),
                    static_cast<float>(up[0]),
                    -static_cast<float>(forward[0]),
                    0,
                    static_cast<float>(right[1]),
                    static_cast<float>(up[1]),
                    -static_cast<float>(forward[1]),
                    0,
                    static_cast<float>(right[2]),
                    static_cast<float>(up[2]),
                    -static_cast<float>(forward[2]),
                    0,
                    0,
                    0,
                    0,
                    1}};
  const Mat4f p = through.Projection();
  Mat4f m = {};
  for (int c = 0; c < 4; c++) {
    for (int r = 0; r < 4; r++) {
      m[c * 4 + r] = 0;
      for (int k = 0; k < 4; k++) { m[c * 4 + r] += p[k * 4 + r] * v[c * 4 + k]; }
    }
  }
  return m;
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
  const uint32_t sign = (held & kHalfSignBit) << kHalfSignShift;
  const uint32_t exponent = (held >> kHalfMantissaBits) & kHalfExponentMask;
  uint32_t mantissa = held & kHalfMantissaMask;
  uint32_t assembled = 0;
  if (exponent == 0) {
    if (mantissa != 0) {
      int shift = 0;
      while ((mantissa & kHalfHiddenBit) == 0) {
        mantissa <<= 1u;
        ++shift;
      }
      mantissa &= kHalfMantissaMask;
      assembled = (static_cast<uint32_t>(kSingleExponentBias - kHalfExponentBias - shift + 1)
                   << kSingleMantissaBits) |
                  (mantissa << kMantissaWiden);
    }
  } else if (exponent == kHalfExponentMask) {
    assembled = kSingleInfinity | (mantissa << kMantissaWiden);
  } else {
    assembled = ((exponent + kSingleExponentBias - kHalfExponentBias) << kSingleMantissaBits) |
                (mantissa << kMantissaWiden);
  }
  assembled |= sign;
  float value = 0;
  std::memcpy(&value, &assembled, sizeof value);
  return value;
}

}

void SceneRenderer::SetCamera(const CameraBasis &basis, const Lens &lens) noexcept {
  Camera_ = basis;
  FovDeg_ = lens.FovDeg;
  OrthoWidthM_ = lens.OrthoWidthM;
  OrthoM_ = lens.OrthoM;
  NearM_ = lens.NearM;
  FarM_ = lens.FarM;
  CameraFull_ = true;
}

Lens SceneRenderer::Through() const {
  return {.WidePx = PictureW(),
          .HighPx = PictureH(),
          .FovDeg = FovDeg_,
          .OrthoWidthM = OrthoWidthM_,
          .OrthoM = OrthoM_,
          .NearM = NearM_,
          .FarM = FarM_,
          .Jitter = Jitter_};
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
        {.Named = Stage::TemporalResolve, .Configure = nullptr, .Encode = nullptr},
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
    Log::Error(LogTag::Render, "no_video", {{"msg", "the client did not initialise SDL video"}});
    WhyNot_ =
        "SDL's video subsystem is not running: outshine renders through SDL3 and the CLIENT owns "
        "the process, so the client calls SDL_Init(SDL_INIT_VIDEO) before it declares a scenario";
    return false;
  }
  SDL_GPUDevice *device =
      SDL_CreateGPUDevice(SDL_ShaderCross_GetSPIRVShaderFormats(), kGpuValidation, nullptr);
  if (device == nullptr) {
    Log::Error(LogTag::Render, "no_device", {{"msg", SDL_GetError()}});
    WhyNot_ = std::string("no gpu device: ") + SDL_GetError();
    return false;
  }
  Device_ = OwnedDevice(device);
  return true;
}

std::expected<OwnedTexture, std::string> SceneRenderer::MakeOffscreen(Extent frame) {
  if (Plan_ == nullptr || !Plan_->Holds(Resource::Surface)) { return OwnedTexture{}; }
  SDL_GPUTextureCreateInfo wanted{};
  wanted.type = SDL_GPU_TEXTURETYPE_2D;
  wanted.format = FormatOf(Plan_->Format(Resource::Surface));
  wanted.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
  wanted.width = static_cast<Uint32>(frame.WidthPx);
  wanted.height = static_cast<Uint32>(frame.HeightPx);
  wanted.layer_count_or_depth = 1;
  wanted.num_levels = 1;
  OwnedTexture texture(Device_.Get(), SDL_CreateGPUTexture(Device_.Get(), &wanted));
  if (!texture) {
    WhyNot_ = std::string(Says::kTargetTextureFailed) + SDL_GetError();
    return std::unexpected(WhyNot_);
  }
  return texture;
}

std::expected<void, std::string> SceneRenderer::StandsOffscreen() {
  if (Showing_ != nullptr || Frame_.Offscreen || Frame_.Width <= 0 || Frame_.Height <= 0) {
    return {};
  }
  auto texture = MakeOffscreen({.WidthPx = Frame_.Width, .HeightPx = Frame_.Height});
  if (!texture) { return std::unexpected(texture.error()); }
  Frame_.Offscreen = std::move(*texture);
  Frame_.HostSurface = Frame_.Offscreen.Get();
  return {};
}

std::expected<void, std::string> SceneRenderer::Init(Extent frame,
                                                     std::shared_ptr<const Compiled> plan) {
  WhyNot_.clear();
  Ready_ = false;
  if (Device_ && !Settle(WhyNot_)) { return std::unexpected(WhyNot_); }
  Submitted_ = false;
  BeginTemporalRun();
  Frame_.Offscreen.Reset();
  Frame_.HostSurface = nullptr;
  Frame_.Shown = {};
  Plan_ = std::move(plan);
  Frame_.Width = frame.WidthPx;
  Frame_.Height = frame.HeightPx;

  for (const Stage stage : Plan_->Order()) {
    if (Executable(stage)) { continue; }
    Log::Error(LogTag::Render, "stage_not_executed", {{"stage", Row(stage).Name}});
    WhyNot_ = std::string("this device layer does not execute the stage '") + Row(stage).Name +
              "', which the catalogue offers and the consumer declared";
    return std::unexpected(WhyNot_);
  }

  if (!Stands()) { return std::unexpected(WhyNot_); }

  SDL_GPUDevice *const device = Device_.Get();
  Frame_.Handles.Device = device;
  Frame_.Handles.HdrFormat = FormatOf(Plan_->Format(Resource::SceneHdr));
  Frame_.Handles.SurfaceFormat = FormatOf(Plan_->Format(Resource::FrameTex));
  Frame_.Handles.Width = Frame_.Width;
  Frame_.Handles.Height = Frame_.Height;

  for (const Compiled::Pass &pass : Plan_->Passes()) {
    if (pass.Kind == PassKind::Compute || pass.Depth == kNoEdge) { continue; }
    Frame_.Handles.SceneColours = pass.Targets;
    break;
  }
  for (size_t r = 0; r < kResourceCount; ++r) {
    const auto id = static_cast<Resource>(r);
    if (!Plan_->Holds(id)) { continue; }
    Create(id);
    if (Created(id)) { continue; }
    WhyNot_ =
        std::string("could not create render resource '") + Row(id).Name + "': " + SDL_GetError();
    return std::unexpected(WhyNot_);
  }

  if (!ConfigurePlanStages()) { return std::unexpected(WhyNot_); }
  if (const auto stood = StandsOffscreen(); !stood) { return std::unexpected(stood.error()); }
  Ready_ = true;

  Log::Info(LogTag::Render,
            "device_ready",
            {{"width", Frame_.Width},
             {"height", Frame_.Height},
             {"driver", SDL_GetGPUDeviceDriver(device)},
             {"plan", Plan_->Digest()},
             {"passes", Plan_->PassCount()},
             {"stages", static_cast<int>(Plan_->Order().size())}});
  for (size_t at = 0; at < kStageCount; ++at) {
    const auto stage = static_cast<Stage>(at);
    if (Executable(stage)) { continue; }
    Log::Info(LogTag::Render, "stage_without_a_body", {{"stage", Row(stage).Name}});
  }
  for (const std::string &merge : Plan_->Merges()) {
    Log::Info(LogTag::Render, "plan_merge", {{"merge", merge}});
  }
  for (const std::string &alias : Plan_->Aliases()) {
    Log::Info(LogTag::Render, "plan_alias", {{"alias", alias}});
  }
  return {};
}

AttachmentSet SceneRenderer::ColoursForStage(Stage wanted) const {
  for (const Compiled::Pass &pass : Plan_->Passes()) {
    for (size_t at = pass.First; at < pass.First + pass.Count; ++at) {
      if (Plan_->Order()[at] == wanted) { return pass.Targets; }
    }
  }
  return Frame_.Handles.SceneColours;
}

bool SceneRenderer::ConfigurePlanStages() {
  DrawsGlass_ = Plan_->Holds(Stage::SubjectsTransmissive);
  for (const Stage stage : Plan_->Order()) {
    std::string why;
    Frame_.Handles.SceneColours = ColoursForStage(stage);
    if (Configure(stage, why)) { continue; }
    Log::Error(LogTag::Render, "stage_not_configured", {{"stage", Row(stage).Name}, {"msg", why}});
    WhyNot_ = std::string("the stage '") + Row(stage).Name + "' did not configure: " + why;
    return false;
  }
  return true;
}

void SceneRenderer::Create(Resource resource) {
  const auto target = [&](Resource of, SDL_GPUTextureUsageFlags usage) {
    SDL_GPUTextureCreateInfo wanted{};
    wanted.type = SDL_GPU_TEXTURETYPE_2D;
    wanted.format = FormatOf(Plan_->Format(of));
    wanted.usage = usage;
    wanted.width = static_cast<uint32_t>(Frame_.Width);
    wanted.height = static_cast<uint32_t>(Frame_.Height);
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
      Frame_.Samp = OwnedSampler(Device_.Get(), SDL_CreateGPUSampler(Device_.Get(), &wanted));
      return;
    }

    case Resource::OverlayAtlas: return;
    case Resource::SceneHdr: Frame_.HdrTex = target(resource, colour); return;
    case Resource::SceneTransmissive: Frame_.TransmissiveTex = target(resource, colour); return;
    case Resource::SceneComposited: Frame_.CompositedTex = target(resource, colour); return;
    case Resource::SceneAerial: Frame_.AerialTex = target(resource, colour); return;
    case Resource::SceneVelocity: Frame_.VelTex = target(resource, colour); return;
    case Resource::SceneShadingNormal: Frame_.ShadingNormalTex = target(resource, colour); return;
    case Resource::SceneSurfaceIdentity:
      Frame_.SurfaceIdentityTex = target(resource, colour);
      return;
    case Resource::SceneDepth:

      Frame_.DepthTex = target(
          resource, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER);
      return;
    case Resource::FrameTex: Frame_.FrameTex = target(resource, colour); return;

    case Resource::Surface:
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

      struct LutShape {
        uint32_t WidthPx;
        uint32_t HeightPx;
        OwnedTexture *Into;
      };

      const LutShape shape = [this, resource] -> LutShape {
        switch (resource) {
          case Resource::MultiScatterLut:
            return {.WidthPx = kMultiScatterLutSize,
                    .HeightPx = kMultiScatterLutSize,
                    .Into = &Frame_.MultiScatterLut};
          case Resource::SkyViewLut:
            return {.WidthPx = kSkyViewLutWidth,
                    .HeightPx = kSkyViewLutHeight,
                    .Into = &Frame_.SkyViewLut};
          default:
            return {.WidthPx = kTransmittanceLutWidth,
                    .HeightPx = kTransmittanceLutHeight,
                    .Into = &Frame_.TransmittanceLut};
        }
      }();
      wanted.width = shape.WidthPx;
      wanted.height = shape.HeightPx;
      wanted.layer_count_or_depth = 1;
      wanted.num_levels = 1;
      wanted.sample_count = SDL_GPU_SAMPLECOUNT_1;
      *shape.Into = OwnedTexture(Device_.Get(), SDL_CreateGPUTexture(Device_.Get(), &wanted));
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
      Frame_.LutSamp = OwnedSampler(Device_.Get(), SDL_CreateGPUSampler(Device_.Get(), &wanted));
      return;
    }
    case Resource::AtmosphereUniform:
    case Resource::CascadeUniform:
    case Resource::IrradianceBuffer: {
      SDL_GPUBufferCreateInfo wanted{};
      wanted.usage =
          SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
      wanted.size = kIrradianceFloats * static_cast<uint32_t>(sizeof(float));
      Frame_.IrradianceBuffer =
          OwnedBuffer(Frame_.Handles.Device, SDL_CreateGPUBuffer(Frame_.Handles.Device, &wanted));
      return;
    }
    case Resource::DepthPyramid: {
      SDL_GPUBufferCreateInfo wanted{};
      wanted.usage =
          SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
      wanted.size = PyramidOver({.WidthPx = static_cast<uint32_t>(Frame_.Width),
                                 .HeightPx = static_cast<uint32_t>(Frame_.Height)})
                        .Texels *
                    static_cast<uint32_t>(sizeof(float));
      Frame_.Pyramid =
          OwnedBuffer(Frame_.Handles.Device, SDL_CreateGPUBuffer(Frame_.Handles.Device, &wanted));
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
      Frame_.ShadowAtlas =
          OwnedTexture(Device_.Get(), SDL_CreateGPUTexture(Device_.Get(), &wanted));
      return;
    }
    case Resource::AoBuffer: return;

    case Resource::SceneLinear:

      Frame_.LinearTex[0] = target(resource, colour);
      Frame_.LinearTex[1] = target(resource, colour);
      Frame_.LinearAt = 0;
      Frame_.HistoryHeld = false;
      return;
    case Resource::kCount: return;
  }
}

bool SceneRenderer::Created(Resource resource) const {
  switch (resource) {
    case Resource::LinearSampler: return static_cast<bool>(Frame_.Samp);
    case Resource::SceneHdr: return static_cast<bool>(Frame_.HdrTex);
    case Resource::SceneTransmissive: return static_cast<bool>(Frame_.TransmissiveTex);
    case Resource::SceneComposited: return static_cast<bool>(Frame_.CompositedTex);
    case Resource::SceneAerial: return static_cast<bool>(Frame_.AerialTex);
    case Resource::SceneVelocity: return static_cast<bool>(Frame_.VelTex);
    case Resource::SceneShadingNormal: return static_cast<bool>(Frame_.ShadingNormalTex);
    case Resource::SceneSurfaceIdentity: return static_cast<bool>(Frame_.SurfaceIdentityTex);
    case Resource::SceneDepth: return static_cast<bool>(Frame_.DepthTex);
    case Resource::FrameTex: return static_cast<bool>(Frame_.FrameTex);
    case Resource::TransmittanceLut: return static_cast<bool>(Frame_.TransmittanceLut);
    case Resource::MultiScatterLut: return static_cast<bool>(Frame_.MultiScatterLut);
    case Resource::SkyViewLut: return static_cast<bool>(Frame_.SkyViewLut);
    case Resource::LutSampler: return static_cast<bool>(Frame_.LutSamp);
    case Resource::AtmosphereUniform:
    case Resource::CascadeUniform:
    case Resource::IrradianceBuffer: return static_cast<bool>(Frame_.IrradianceBuffer);
    case Resource::DepthPyramid: return static_cast<bool>(Frame_.Pyramid);
    case Resource::VegetationTable:
    case Resource::Meter:
    case Resource::ShadowAtlas: return static_cast<bool>(Frame_.ShadowAtlas);
    case Resource::SceneLinear: return Frame_.LinearTex[0] && Frame_.LinearTex[1];
    case Resource::OverlayAtlas:
    case Resource::Surface:
    case Resource::ClusterSphere:
    case Resource::ClusterIndex:
    case Resource::ClusterJobs:
    case Resource::ClusterBatches:
    case Resource::ClusterKept:
    case Resource::ClusterSlot:
    case Resource::DrawIndex:
    case Resource::DrawArguments:
    case Resource::AoBuffer:
    case Resource::kCount: return true;
  }
  return false;
}

SDL_GPUTexture *SceneRenderer::Target(Resource resource) const {
  switch (resource) {
    case Resource::OverlayAtlas: return nullptr;
    case Resource::SceneHdr: return Frame_.HdrTex.Get();
    case Resource::SceneTransmissive: return Frame_.TransmissiveTex.Get();
    case Resource::SceneComposited: return Frame_.CompositedTex.Get();
    case Resource::SceneAerial: return Frame_.AerialTex.Get();
    case Resource::SceneVelocity: return Frame_.VelTex.Get();
    case Resource::SceneShadingNormal: return Frame_.ShadingNormalTex.Get();
    case Resource::SceneSurfaceIdentity: return Frame_.SurfaceIdentityTex.Get();
    case Resource::SceneDepth: return Frame_.DepthTex.Get();
    case Resource::FrameTex: return Frame_.FrameTex.Get();

    case Resource::Surface: return Frame_.HostSurface;

    case Resource::ClusterSphere:
    case Resource::ClusterIndex:
    case Resource::ClusterJobs:
    case Resource::ClusterBatches:
    case Resource::ClusterKept:
    case Resource::ClusterSlot:
    case Resource::DrawIndex:
    case Resource::DrawArguments: return nullptr;
    case Resource::TransmittanceLut: return Frame_.TransmittanceLut.Get();
    case Resource::MultiScatterLut: return Frame_.MultiScatterLut.Get();
    case Resource::SkyViewLut: return Frame_.SkyViewLut.Get();
    case Resource::ShadowAtlas: return Frame_.ShadowAtlas.Get();
    case Resource::LinearSampler:
    case Resource::LutSampler:
    case Resource::AtmosphereUniform:
    case Resource::CascadeUniform:
    case Resource::VegetationTable:
    case Resource::IrradianceBuffer:
    case Resource::Meter:
    case Resource::DepthPyramid:
    case Resource::AoBuffer: return nullptr;

    case Resource::SceneLinear: return Frame_.LinearTex[Frame_.LinearAt].Get();
    case Resource::kCount: return nullptr;
  }
  return nullptr;
}

SDL_GPUBuffer *SceneRenderer::BufferFor(Resource resource) const {
  const SubjectResidency &resident = Subjects_.Resident();
  switch (resource) {
    case Resource::ClusterSphere:
      return resident.Buffer(SubjectResidency::Stream::ClusterSpheres).Get();
    case Resource::ClusterIndex: return resident.Buffer(SubjectResidency::Stream::Index).Get();
    case Resource::ClusterJobs: return resident.Buffer(SubjectResidency::Stream::ClusterJobs).Get();
    case Resource::ClusterBatches:
      return resident.Buffer(SubjectResidency::Stream::ClusterBatches).Get();
    case Resource::ClusterKept: return resident.Buffer(SubjectResidency::Stream::ClusterKept).Get();
    case Resource::ClusterSlot: return resident.Buffer(SubjectResidency::Stream::ClusterSlot).Get();
    case Resource::DrawIndex: return resident.Buffer(SubjectResidency::Stream::DrawIndex).Get();
    case Resource::DrawArguments:
      return resident.Buffer(SubjectResidency::Stream::DrawArguments).Get();
    case Resource::IrradianceBuffer: return Frame_.IrradianceBuffer.Get();
    case Resource::DepthPyramid: return Frame_.Pyramid.Get();
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
  out.WidthPx = static_cast<double>(Frame_.Width);
  out.HeightPx = static_cast<double>(Frame_.Height);
  if (RegionW_ > 0 && RegionH_ > 0) {
    out.LeftPx = RegionX_ * static_cast<double>(Frame_.Width);
    out.TopPx = RegionY_ * static_cast<double>(Frame_.Height);
    out.WidthPx = RegionW_ * static_cast<double>(Frame_.Width);
    out.HeightPx = RegionH_ * static_cast<double>(Frame_.Height);
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

SDL_GPUTexture *SceneRenderer::DisplaySource() const {
  const auto input =
      Plan_->Holds(Stage::TemporalResolve) ? Resource::SceneAerial : Resource::SceneLinear;
  return Target(Plan_->Bound(input));
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
  if (seat->Configure == nullptr) { return true; }
  return (this->*seat->Configure)(error);
}

bool SceneRenderer::ConfigureSubjects(std::string &error) {
  Subjects_.SetSeparateTransmission(DrawsGlass_);
  return Subjects_.Configure(Frame_.Handles, error);
}

bool SceneRenderer::ConfigureGlass(std::string &error) {
  Glass_.Shares(Subjects_.Owned());
  Glass_.SeeThroughTo(Frame_.HdrTex.Get(), Frame_.Samp.Get());
  return Glass_.Configure(Frame_.Handles, error);
}

bool SceneRenderer::ConfigureCompositeTransmission(std::string &error) {
  return Frame_.CompositeTransmission.Configure(
      Frame_.Handles,
      {.Opaque = Frame_.HdrTex.Get(),
       .Transmissive = Frame_.TransmissiveTex.Get(),
       .Exact = Frame_.Samp.Get(),
       .Target = FormatOf(Plan_->Format(Resource::SceneComposited))},
      error);
}

bool SceneRenderer::ConfigureOverlay(std::string &error) {
  return Overlay_.EnsureAtlas(Frame_.Handles, error) &&
         Frame_.OverlayPipe.Configure(
             Frame_.Handles, Frame_.Samp.Get(), FormatOf(Plan_->Format(Resource::FrameTex)), error);
}

bool SceneRenderer::ConfigurePresent(std::string &error) {
  return Frame_.Present.Configure(Frame_.Handles, Frame_.FrameTex.Get(), Frame_.Samp.Get(), error);
}

bool SceneRenderer::ConfigureTonemap(std::string &error) {
  return Frame_.Tonemap.Configure(Frame_.Handles,
                                  {.Scene = DisplaySource(),
                                   .Depth = Frame_.DepthTex.Get(),
                                   .Exact = Frame_.Samp.Get(),
                                   .Linear = FormatOf(Plan_->Format(Resource::SceneLinear))},
                                  Display(),
                                  error);
}

bool SceneRenderer::ConfigureMediumTransmittance(std::string &error) {
  return Frame_.MediumTransmittance.Configure(Frame_.Handles, Frame_.TransmittanceLut.Get(), error);
}

bool SceneRenderer::ConfigureMediumMultiScatter(std::string &error) {
  return Frame_.MultiScatter.Configure(Frame_.Handles,
                                       Frame_.TransmittanceLut.Get(),
                                       Frame_.LutSamp.Get(),
                                       Frame_.MultiScatterLut.Get(),
                                       error);
}

bool SceneRenderer::ConfigureMediumRadiance(std::string &error) {
  return Frame_.Radiance.Configure(Frame_.Handles,
                                   Frame_.TransmittanceLut.Get(),
                                   Frame_.MultiScatterLut.Get(),
                                   Frame_.LutSamp.Get(),
                                   Frame_.SkyViewLut.Get(),
                                   error);
}

bool SceneRenderer::ConfigureSky(std::string &error) {
  return Frame_.Sky.Configure(Frame_.Handles,
                              {.SkyView = Frame_.SkyViewLut.Get(),
                               .Transmittance = Frame_.TransmittanceLut.Get(),
                               .Lut = Frame_.LutSamp.Get()},
                              error);
}

bool SceneRenderer::ConfigureAerialPerspective(std::string &error) {
  return Frame_.Aerial.Configure(Frame_.Handles,
                                 {.Scene = Target(Plan_->Bound(Resource::SceneComposited)),
                                  .Depth = Frame_.DepthTex.Get(),
                                  .SkyView = Frame_.SkyViewLut.Get(),
                                  .Transmittance = Frame_.TransmittanceLut.Get(),
                                  .Exact = Frame_.Samp.Get(),
                                  .Lut = Frame_.LutSamp.Get()},
                                 FormatOf(Plan_->Format(Resource::SceneAerial)),
                                 error);
}

bool SceneRenderer::ConfigureLightVisibility(std::string &error) {
  return Frame_.Shadow.Configure(Subjects_, Frame_.Handles, error);
}

void SceneRenderer::Picture(bool picture, const PassRecording &into) {
  SDL_GPUViewport where{};
  const Placed rect = PictureRect();
  where.x = picture ? static_cast<float>(rect.LeftPx) : 0.0f;
  where.y = picture ? static_cast<float>(rect.TopPx) : 0.0f;
  where.w = picture ? static_cast<float>(rect.WidthPx) : static_cast<float>(Frame_.Width);
  where.h = picture ? static_cast<float>(rect.HeightPx) : static_cast<float>(Frame_.Height);
  where.min_depth = 0.0f;
  where.max_depth = 1.0f;
  if (into.Pass != nullptr) { SDL_SetGPUViewport(into.Pass, &where); }
}

FrameContext SceneRenderer::Framing() const {
  FrameContext ctx{};
  if (OrthoM_ > 0) {
    for (int axis = 0; axis < 3; ++axis) {
      ctx.ViewPosition[axis] = static_cast<float>(-Camera_.Forward[axis]);
    }
    ctx.ViewPosition[3] = 0;
  }
  for (int axis = 0; axis < 3; axis++) { ctx.PreViewTranslation[axis] = -Camera_.EyeM[axis]; }

  ctx.Mvp = MvpCamRel(Camera_, Through());
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
  (this->*seat->Encode)(ctx, into);
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
    spent.Triangles += drew.Ground().Triangles();
    spent.Draws += drew.Ground().Drawn() > 0 ? 1u : 0u;
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
  Frame_.CompositeTransmission.Encode(ctx, into);
}

void SceneRenderer::EncodeTonemap(const FrameContext &ctx, const PassRecording &into) {
  Frame_.Tonemap.Bind(DisplaySource());
  const Vec2f delta = {{Jitter_[0] - PrevJitter_[0], Jitter_[1] - PrevJitter_[1]}};
  Frame_.Tonemap.BindTemporal(
      {.History = Frame_.LinearTex[1 - Frame_.LinearAt].Get(), .Velocity = Frame_.VelTex.Get()},
      Extent{.WidthPx = Frame_.Width, .HeightPx = Frame_.Height},
      delta,
      Frame_.HistoryHeld);
  Picture(true, into);
  Frame_.Tonemap.Encode(ctx, into);
}

void SceneRenderer::EncodeOverlay(const FrameContext &ctx, const PassRecording &into) {
  Picture(false, into);
  Frame_.OverlayPipe.Bind(Extent{.WidthPx = Frame_.Width, .HeightPx = Frame_.Height});
  Frame_.OverlayPipe.Encode(Overlay_, ctx, into);
}

void SceneRenderer::EncodePresent(const FrameContext &ctx, const PassRecording &into) {
  {
    std::string why;
    if (!Frame_.Present.For(Frame_.Handles, SurfaceFormat(), why)) {
      Log::Error(LogTag::Render, "present_not_built", {{"msg", why}});
      return;
    }
  }
  Picture(false, into);
  Frame_.Present.Encode(ctx, into);
}

void SceneRenderer::EncodeMediumTransmittance(const FrameContext &ctx, const PassRecording &into) {
  (void)ctx;
  Frame_.MediumTransmittance.Encode(into);
}

void SceneRenderer::EncodeMediumMultiScatter(const FrameContext &ctx, const PassRecording &into) {
  (void)ctx;
  Frame_.MultiScatter.Encode(into);
}

void SceneRenderer::EncodeMediumRadiance(const FrameContext &ctx, const PassRecording &into) {
  (void)ctx;
  Frame_.Radiance.Encode(into);
}

bool SceneRenderer::SetGroundClasses(std::span<const uint32_t> classes,
                                     std::span<const float> palette,
                                     std::string &error) {
  const auto uploaded =
      GroundStorage_.Replace(Frame_.Handles.Device, classes, palette, Submission_);
  if (!uploaded) {
    error = uploaded.error();
    return false;
  }
  Subjects_.GroundFrom({.Classes = GroundStorage_.Classes(), .Palette = GroundStorage_.Palette()});
  Glass_.GroundFrom({.Classes = GroundStorage_.Classes(), .Palette = GroundStorage_.Palette()});
  return true;
}

bool SceneRenderer::ConfigureIrradiance(std::string &error) {
  Subjects_.SkyFrom(Frame_.IrradianceBuffer.Get());
  if (DrawsGlass_) { Glass_.SkyFrom(Frame_.IrradianceBuffer.Get()); }
  if (!GroundStorage_.Ready() && !SetGroundClasses({}, {}, error)) { return false; }
  return Frame_.SkyIrradianceStage.Configure(Frame_.Handles,
                                             Frame_.TransmittanceLut.Get(),
                                             Frame_.MultiScatterLut.Get(),
                                             Frame_.LutSamp.Get(),
                                             Frame_.IrradianceBuffer.Get(),
                                             error);
}

void SceneRenderer::EncodeIrradiance(const FrameContext &ctx, const PassRecording &into) {
  (void)ctx;
  Frame_.SkyIrradianceStage.Encode(into);
}

bool SceneRenderer::ConfigureDepthPyramid(std::string &error) {
  return Frame_.PyramidStage.Configure(Frame_.Handles,
                                       Frame_.DepthTex.Get(),
                                       Frame_.Samp.Get(),
                                       Frame_.Pyramid.Get(),
                                       {.WidthPx = Frame_.Width, .HeightPx = Frame_.Height},
                                       error);
}

void SceneRenderer::EncodeDepthPyramid(const FrameContext &ctx, const PassRecording &into) {
  (void)ctx;
  Frame_.PyramidStage.Encode(into);
}

bool SceneRenderer::ConfigureSubjectCull(std::string &error) {
  Frame_.Cull.PyramidFrom(Frame_.Pyramid.Get(),
                          PyramidOver({.WidthPx = static_cast<uint32_t>(Frame_.Width),
                                       .HeightPx = static_cast<uint32_t>(Frame_.Height)}));
  return Frame_.Cull.Configure(Subjects_, Frame_.Handles, error);
}

void SceneRenderer::EncodeSubjectCull(const FrameContext &ctx, const PassRecording &into) {
  Frame_.Cull.Projects(static_cast<float>(Frame_.Height));
  Frame_.Cull.EncodeCull(ctx, into);
}

void SceneRenderer::EncodeSubjectScan(const FrameContext &ctx, const PassRecording &into) {
  Frame_.Cull.EncodeScan(ctx, into);
}

void SceneRenderer::EncodeSubjectCompact(const FrameContext &ctx, const PassRecording &into) {
  Frame_.Cull.EncodeCompact(ctx, into);
}

void SceneRenderer::EncodeLightVisibility(const FrameContext &ctx, const PassRecording &into) {
  Frame_.Shadow.Encode(ctx, into);
  Subjects_.ShadowedBy(
      Frame_.ShadowAtlas.Get(), Frame_.LutSamp.Get(), Frame_.Shadow.LightFromWorld());
}

bool SceneRenderer::Settle(std::string &error) {
  if (Device_.Get() == nullptr) {
    error = Says::kRendererNotReady;
    return false;
  }
  if (!Submission_.WaitIdle(Submission_.Context, Device_.Get())) {
    error = std::string(Says::kGpuWaitFailed) + SDL_GetError();
    return false;
  }
  error.clear();
  return true;
}

void SceneRenderer::SettleShadow() {
  Frame_.Shadow.Prepare(Framing());
  Touched_[static_cast<size_t>(Resource::ShadowAtlas)] = Frame_.Shadow.Cached();
}

EyeBasis SceneRenderer::Eye() const {
  assert(OrthoM_ > 0.0f || FovDeg_ > 0.0f);
  EyeBasis eye;
  eye.TanHalfHeight =
      OrthoM_ > 0.0f ? 0.0f : std::tan(static_cast<float>(FovDeg_ * kDeg2Rad) * 0.5f);
  eye.TanHalfWidth =
      eye.TanHalfHeight * (PictureH() > 0.0 ? static_cast<float>(PictureW() / PictureH()) : 1.0f);
  for (int axis = 0; axis < 3; ++axis) {
    eye.Right[axis] = static_cast<float>(Camera_.Right[axis]);
    eye.Up[axis] = static_cast<float>(Camera_.Up[axis]);
    eye.Forward[axis] = static_cast<float>(Camera_.Forward[axis]);
  }
  return eye;
}

void SceneRenderer::EncodeAerialPerspective(const FrameContext &ctx, const PassRecording &into) {
  Picture(true, into);
  Frame_.Aerial.SetBasis(Eye());
  const Mat4f projection = Through().Projection();
  Frame_.Aerial.SetDepthReconstruction(
      {{projection[14], projection[15], projection[10], -projection[11]}});
  Frame_.Aerial.Encode(ctx, into);
}

void SceneRenderer::EncodeSky(const FrameContext &ctx, const PassRecording &into) {
  Picture(true, into);
  Frame_.Sky.SetBasis(Eye());
  Frame_.Sky.Encode(ctx, into);
}

namespace {

template <int Base> float RadicalInverse(int index) {
  float result = 0.0f;
  float weight = 1.0f / static_cast<float>(Base);
  int at = index + 1;
  while (at > 0) {
    result += weight * static_cast<float>(at % Base);
    at /= Base;
    weight /= static_cast<float>(Base);
  }
  return result;
}

Vec2f HaltonJitter(int at) {
  return {{RadicalInverse<2>(at) - 0.5f, RadicalInverse<3>(at) - 0.5f}};
}
}

void SceneRenderer::EncodePass(SDL_GPUCommandBuffer *commands,
                               size_t pass,
                               StageSubmission &submission) {
  const Compiled::Pass &declared = Plan_->Passes()[pass];
  if (declared.Kind == PassKind::Compute) {
    EncodeComputePass(commands, declared, submission);
  } else {
    EncodeGraphicsPass(commands, declared, submission);
  }
}

void SceneRenderer::EncodeComputePass(SDL_GPUCommandBuffer *commands,
                                      const Compiled::Pass &declared,
                                      StageSubmission &submission) {
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
  const PassRecording into{.Commands = commands,
                           .Pass = nullptr,
                           .Dispatch = SDL_BeginGPUComputePass(
                               commands, written.data(), writtenCount, tables.data(), tableCount),
                           .Submission = submission};
  for (size_t at = 0; at < declared.Count; ++at) {
    EncodeStage(Plan_->Order()[declared.First + at], into);
  }
  SDL_EndGPUComputePass(into.Dispatch);
}

void SceneRenderer::EncodeGraphicsPass(SDL_GPUCommandBuffer *commands,
                                       const Compiled::Pass &declared,
                                       StageSubmission &submission) {
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
      .Dispatch = nullptr,
      .Submission = submission};
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
  Frame_.LinearAt = 0;
  Frame_.HistoryHeld = false;
}

std::expected<void, std::string> SceneRenderer::PrepareFrame() {
  if (!Ready_) { return std::unexpected(WhyNot_.empty() ? Says::kRendererNotReady : WhyNot_); }
  if (!CameraFull_) { return std::unexpected(Says::kCameraNotConfigured); }

  Subjects_.CastsNoShadow();
  for (bool &touched : Touched_) { touched = false; }
  SettleShadow();
  {
    std::string why;
    if (!Subjects_.HandTables(why) || !Subjects_.HandPlacements(false, why) ||
        (DrawsGlass_ && !Glass_.HandTables(why))) {
      return std::unexpected(std::move(why));
    }
    if (!Subjects_.HandDrawArguments(true, why)) { return std::unexpected(std::move(why)); }
  }
  return {};
}

std::expected<void, std::string> SceneRenderer::RenderFrame() {
  auto prepared = PrepareFrame();
  if (!prepared) { return prepared; }
  SDL_GPUCommandBuffer *commands = Submission_.Acquire(Submission_.Context, Device_.Get());
  if (commands == nullptr) { return std::unexpected(SDL_GetError()); }
  std::string uploadError;
  if (!Subjects_.FlushCrossings(commands, uploadError) ||
      (DrawsGlass_ && !Glass_.FlushCrossings(commands, uploadError))) {
    SDL_CancelGPUCommandBuffer(commands);
    return std::unexpected(std::move(uploadError));
  }

  SDL_GPUTexture *swapchain = nullptr;
  if (Showing_ != nullptr) {
    Uint32 gotW = 0;
    Uint32 gotH = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, Showing_, &swapchain, &gotW, &gotH)) {
      std::string error = SDL_GetError();
      SDL_CancelGPUCommandBuffer(commands);
      return std::unexpected(std::move(error));
    }
    if (swapchain == nullptr) {
      if (!SDL_CancelGPUCommandBuffer(commands)) { return std::unexpected(SDL_GetError()); }
      return {};
    }
    Frame_.Shown.WidthPx = static_cast<int>(gotW);
    Frame_.Shown.HeightPx = static_cast<int>(gotH);
    Frame_.HostSurface = swapchain;
  }

  const auto previousJitter = Jitter_;
  const auto previousPrevJitter = PrevJitter_;
  const auto previousJitterAt = JitterAt_;
  const auto previousHistoryStarted = HistoryStarted_;
  const auto previousHistoryHeld = Frame_.HistoryHeld;
  const auto previousLinearAt = Frame_.LinearAt;
  if (Plan_->Holds(Stage::TemporalResolve)) {
    PrevJitter_ = Jitter_;
    JitterAt_ = (JitterAt_ + 1) % kJitterPeriod;
    Jitter_ = HaltonJitter(JitterAt_);

    Frame_.HistoryHeld = HistoryStarted_;
    HistoryStarted_ = true;
    Frame_.LinearAt = 1 - Frame_.LinearAt;
  }
  StageSubmission stageSubmission;
  const auto restoreTemporalState = [&] {
    Jitter_ = previousJitter;
    PrevJitter_ = previousPrevJitter;
    JitterAt_ = previousJitterAt;
    HistoryStarted_ = previousHistoryStarted;
    Frame_.HistoryHeld = previousHistoryHeld;
    Frame_.LinearAt = previousLinearAt;
  };

  if (!Subjects_.Ground().Cull(Framing(), Subjects_.AnchorM(), commands, uploadError)) {
    SDL_CancelGPUCommandBuffer(commands);
    restoreTemporalState();
    return std::unexpected(std::move(uploadError));
  }

  for (size_t pass = 0; pass < Plan_->Passes().size(); ++pass) {
    EncodePass(commands, pass, stageSubmission);
  }

  if (Landed_[LandedAt_] != nullptr) {
    if (!Submission_.WaitFence(Submission_.Context, Device_.Get(), &Landed_[LandedAt_], 1)) {
      const std::string error = SDL_GetError();
      SDL_CancelGPUCommandBuffer(commands);
      restoreTemporalState();
      return std::unexpected(error);
    }
    SDL_ReleaseGPUFence(Device_.Get(), Landed_[LandedAt_]);
    Landed_[LandedAt_] = nullptr;
  }
  Landed_[LandedAt_] = Submission_.Submit(Submission_.Context, commands);
  if (swapchain != nullptr) { Frame_.HostSurface = Frame_.Offscreen.Get(); }
  if (Landed_[LandedAt_] == nullptr) {
    std::string error = SDL_GetError();
    restoreTemporalState();
    return std::unexpected(std::move(error));
  }
  Subjects_.CommitCrossings();
  if (DrawsGlass_) { Glass_.CommitCrossings(); }
  stageSubmission.Commit();
  LandedAt_ = (LandedAt_ + 1) % kFramesInFlight;
  for (int axis = 0; axis < 3; axis++) { PrevEye_[axis] = Camera_.EyeM[axis]; }
  Subjects_.CarryFrame();
  Glass_.CarryFrame();

  PrevMvp_ = MvpCamRel(Camera_, Through());
  Submitted_ = true;
  return {};
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

ReadState SceneRenderer::ReadPixels(std::vector<uint8_t> &rgba) {
  WhyNot_.clear();
  if (!Ready_ || !Submitted_) {
    WhyNot_ = Says::kNoReadbackFrame;
    return ReadState::Failed;
  }
  if (!Plan_) {
    WhyNot_ = Says::kNoColourOutput;
    return ReadState::Failed;
  }
  const auto source = Plan_->Holds(Resource::FrameTex) ? Resource::FrameTex : Resource::Surface;
  if (!Plan_->Holds(source)) {
    WhyNot_ = Says::kNoColourOutput;
    return ReadState::Failed;
  }
  SDL_GPUTexture *const held =
      source == Resource::FrameTex ? Frame_.FrameTex.Get() : Frame_.Offscreen.Get();
  if (held == nullptr) {
    WhyNot_ = Says::kNoColourTexture;
    return ReadState::Failed;
  }
  Readback read;
  if (read.FromTexture(
          Device_.Get(), held, {.WidthPx = Frame_.Width, .HeightPx = Frame_.Height}, 4u) !=
      ReadState::Ready) {
    WhyNot_ = std::string(Says::kColourReadbackFailed) + SDL_GetError();
    return ReadState::Failed;
  }
  rgba.resize(static_cast<size_t>(Frame_.Width) * static_cast<size_t>(Frame_.Height) * 4u);
  std::memcpy(rgba.data(), read.Rows(), rgba.size());
  const auto format = FormatOf(Plan_->Format(source));
  if (format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM ||
      format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB) {
    for (size_t at = 0; at + 3 < rgba.size(); at += 4) { std::swap(rgba[at], rgba[at + 2]); }
  }
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadDepth(std::vector<float> &depth) {
  if (!Ready_ || !Frame_.DepthTex) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.Get(),
                       Frame_.DepthTex.Get(),
                       {.WidthPx = Frame_.Width, .HeightPx = Frame_.Height},
                       4u) != ReadState::Ready) {
    return ReadState::Failed;
  }
  depth.resize(static_cast<size_t>(Frame_.Width) * static_cast<size_t>(Frame_.Height));
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
                       {.WidthPx = Frame_.Width, .HeightPx = Frame_.Height},
                       wide ? 16u : 8u) != ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components =
      static_cast<size_t>(Frame_.Width) * static_cast<size_t>(Frame_.Height) * 4u;
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
  if (!Ready_ || !Frame_.ShadowAtlas || !Frame_.Shadow.HasSubmittedData()) {
    return ReadState::Failed;
  }
  Readback read;
  if (read.FromTexture(Device_.Get(),
                       Frame_.ShadowAtlas.Get(),
                       {.WidthPx = kShadowAtlasPx, .HeightPx = kShadowAtlasPx},
                       4u) != ReadState::Ready) {
    return ReadState::Failed;
  }
  depth.resize(static_cast<size_t>(kShadowAtlasPx) * static_cast<size_t>(kShadowAtlasPx));
  std::memcpy(depth.data(), read.Rows(), depth.size() * sizeof(float));
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadKeptIndices(KeptDraws &into) {
  into = {};
  const SubjectResidency &resident = Subjects_.Resident();
  SDL_GPUBuffer *const args = resident.Buffer(SubjectResidency::Stream::DrawArguments).Get();
  const uint32_t rows = Subjects_.ClusterBatchRows();
  if (!Ready_ || args == nullptr || rows == 0) { return ReadState::Failed; }
  Readback read;
  const uint32_t bytes = rows * 5u * static_cast<uint32_t>(sizeof(uint32_t));
  if (read.FromBuffer(Device_.Get(), args, bytes) != ReadState::Ready) { return ReadState::Failed; }
  const auto *const held = reinterpret_cast<const uint32_t *>(read.Rows());
  for (uint32_t at = 0; at < rows; ++at) {
    into.Indices += held[static_cast<size_t>(at) * 5];
    into.Batches += held[static_cast<size_t>(at) * 5] > 0u ? 1u : 0u;
  }
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadPyramid(PyramidDepths &into) {
  into = {};
  if (!Ready_ || !Frame_.Pyramid) { return ReadState::Failed; }
  const PyramidShape shape = PyramidOver({.WidthPx = static_cast<uint32_t>(Frame_.Width),
                                          .HeightPx = static_cast<uint32_t>(Frame_.Height)});
  const uint32_t texels = shape.Wide[0] * shape.High[0];
  if (texels == 0) { return ReadState::Failed; }
  const ReadState landed = Frame_.PyramidRead.Poll();
  if (landed == ReadState::Failed) {
    return Frame_.PyramidRead.Enqueue(Device_.Get(),
                                      Frame_.Pyramid.Get(),
                                      texels * static_cast<uint32_t>(sizeof(float))) ==
                   ReadState::Failed
               ? ReadState::Failed
               : ReadState::Pending;
  }
  if (landed == ReadState::Pending) { return ReadState::Pending; }
  const auto *const held = reinterpret_cast<const float *>(Frame_.PyramidRead.Rows());
  double summed = 0.0;
  into.Nearest = held[0];
  into.Farthest = held[0];
  for (uint32_t at = 0; at < texels; ++at) {
    into.Nearest = std::max(held[at], into.Nearest);
    into.Farthest = std::min(held[at], into.Farthest);
    summed += static_cast<double>(held[at]);
  }
  into.Mean = static_cast<float>(summed / static_cast<double>(texels));
  Frame_.PyramidRead.Release();
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadSkyIrradiance(std::span<float, kIrradianceFloats> out) {
  if (!Ready_ || !Frame_.IrradianceBuffer || !Frame_.SkyIrradianceStage.Settled()) {
    return ReadState::Failed;
  }
  Readback read;
  if (read.FromBuffer(Device_.Get(),
                      Frame_.IrradianceBuffer.Get(),
                      kIrradianceFloats * static_cast<uint32_t>(sizeof(float))) !=
      ReadState::Ready) {
    return ReadState::Failed;
  }
  std::memcpy(out.data(), read.Rows(), kIrradianceFloats * sizeof(float));
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadShadingNormal(std::vector<float> &xyz) {
  SDL_GPUTexture *source = Frame_.ShadingNormalTex.Get();
  if (!Ready_ || (source == nullptr)) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(
          Device_.Get(), source, {.WidthPx = Frame_.Width, .HeightPx = Frame_.Height}, 8u) !=
      ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components =
      static_cast<size_t>(Frame_.Width) * static_cast<size_t>(Frame_.Height) * 4u;
  xyz.resize(components);
  for (size_t component = 0; component < components; ++component) {
    uint16_t bits = 0;
    std::memcpy(&bits, read.Rows() + component * sizeof(uint16_t), sizeof bits);
    xyz[component] = HalfToFloat(bits);
  }
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadSceneVelocity(std::vector<float> &xy) {
  SDL_GPUTexture *source = Frame_.VelTex.Get();
  if (!Ready_ || (source == nullptr)) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(
          Device_.Get(), source, {.WidthPx = Frame_.Width, .HeightPx = Frame_.Height}, 4u) !=
      ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components =
      static_cast<size_t>(Frame_.Width) * static_cast<size_t>(Frame_.Height) * 2u;
  xy.resize(components);
  for (size_t component = 0; component < components; ++component) {
    uint16_t bits = 0;
    std::memcpy(&bits, read.Rows() + component * sizeof(uint16_t), sizeof bits);
    xy[component] = HalfToFloat(bits);
  }
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadSurfaceIdentity(std::vector<float> &slot) {
  SDL_GPUTexture *source = Frame_.SurfaceIdentityTex.Get();
  if (!Ready_ || (source == nullptr)) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(
          Device_.Get(), source, {.WidthPx = Frame_.Width, .HeightPx = Frame_.Height}, 16u) !=
      ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components =
      static_cast<size_t>(Frame_.Width) * static_cast<size_t>(Frame_.Height) * 4u;
  slot.resize(components);
  std::memcpy(slot.data(), read.Rows(), components * sizeof(float));
  return ReadState::Ready;
}

void SceneRenderer::StopShowing() {
  Frame_.Offscreen.Reset();
  Frame_.HostSurface = nullptr;
  Frame_.Shown = {};
  Submitted_ = false;
  if (Showing_ == nullptr) { return; }
  SDL_ReleaseWindowFromGPUDevice(Device_.Get(), Showing_);
  Showing_ = nullptr;
}

std::expected<SDL_GPUPresentMode, std::string> SceneRenderer::ClaimWindow(SDL_Window *window) {
  if (!SDL_ClaimWindowForGPUDevice(Device_.Get(), window)) {
    WhyNot_ = std::string(Says::kWindowClaimFailed) + SDL_GetError();
    return std::unexpected(WhyNot_);
  }
  constexpr auto composition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR;
  if (!SDL_WindowSupportsGPUSwapchainComposition(Device_.Get(), window, composition)) {
    WhyNot_ = Says::kUnsupportedTransfer;
    SDL_ReleaseWindowFromGPUDevice(Device_.Get(), window);
    return std::unexpected(WhyNot_);
  }
  constexpr std::array modes = {
      SDL_GPU_PRESENTMODE_MAILBOX, SDL_GPU_PRESENTMODE_IMMEDIATE, SDL_GPU_PRESENTMODE_VSYNC};
  WhyNot_ = Says::kNoPresentMode;
  for (const SDL_GPUPresentMode mode : modes) {
    if (!SDL_WindowSupportsGPUPresentMode(Device_.Get(), window, mode)) { continue; }
    if (SDL_SetGPUSwapchainParameters(Device_.Get(), window, composition, mode)) { return mode; }
    WhyNot_ = std::string(Says::kPresentModeFailed) + SDL_GetError();
  }
  SDL_ReleaseWindowFromGPUDevice(Device_.Get(), window);
  return std::unexpected(WhyNot_);
}

std::expected<void, std::string>
SceneRenderer::DrawsInto(int widthPx, int heightPx, SDL_Window *presents) {
  if (widthPx <= 0 || heightPx <= 0) { return std::unexpected(Says::kInvalidTargetExtent); }
  if (!Stands()) { return std::unexpected(WhyNot_); }

  OwnedTexture texture;
  auto mode = Presenting_;
  if (presents != nullptr && Showing_ != presents) {
    const auto claimed = ClaimWindow(presents);
    if (!claimed) { return std::unexpected(claimed.error()); }
    mode = *claimed;
  } else if (presents == nullptr) {
    auto made = MakeOffscreen({.WidthPx = widthPx, .HeightPx = heightPx});
    if (!made) { return std::unexpected(made.error()); }
    texture = std::move(*made);
  }

  if (Showing_ != nullptr && Showing_ != presents) {
    SDL_ReleaseWindowFromGPUDevice(Device_.Get(), Showing_);
  }
  Showing_ = presents;
  Presenting_ = mode;
  Frame_.Offscreen = std::move(texture);
  Frame_.HostSurface = Frame_.Offscreen.Get();
  Frame_.Width = widthPx;
  Frame_.Height = heightPx;
  Frame_.Shown = {};
  Submitted_ = false;
  WhyNot_.clear();
  return {};
}

std::expected<std::optional<SceneRenderer::Shown>, std::string_view>
SceneRenderer::Presented() const {
  if (Showing_ == nullptr) {
    return std::unexpected(
        "no window is being shown on: a frame is presented to a surface the caller declared, "
        "and `DrawsInto` names one");
  }
  if (Frame_.Shown.WidthPx == 0) { return std::optional<Shown>(); }
  return std::optional<Shown>(Frame_.Shown);
}

}
