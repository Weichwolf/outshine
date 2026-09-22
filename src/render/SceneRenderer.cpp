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
  ActiveState().Camera = basis;
  ActiveState().FovDeg = lens.FovDeg;
  ActiveState().OrthoWidthM = lens.OrthoWidthM;
  ActiveState().OrthoM = lens.OrthoM;
  ActiveState().NearM = lens.NearM;
  ActiveState().FarM = lens.FarM;
  ActiveState().CameraFull = true;
}

Lens SceneRenderer::Through() const {
  return {.WidePx = PictureW(),
          .HighPx = PictureH(),
          .FovDeg = ActiveState().FovDeg,
          .OrthoWidthM = ActiveState().OrthoWidthM,
          .OrthoM = ActiveState().OrthoM,
          .NearM = ActiveState().NearM,
          .FarM = ActiveState().FarM,
          .Jitter = ActiveState().Jitter};
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
    ActiveState().WhyNot =
        "SDL's video subsystem is not running: outshine renders through SDL3 and the CLIENT owns "
        "the process, so the client calls SDL_Init(SDL_INIT_VIDEO) before it declares a scenario";
    return false;
  }
  SDL_GPUDevice *device =
      SDL_CreateGPUDevice(SDL_ShaderCross_GetSPIRVShaderFormats(), kGpuValidation, nullptr);
  if (device == nullptr) {
    Log::Error(LogTag::Render, "no_device", {{"msg", SDL_GetError()}});
    ActiveState().WhyNot = std::string("no gpu device: ") + SDL_GetError();
    return false;
  }
  Device_ = OwnedDevice(device);
  return true;
}

std::expected<OwnedTexture, std::string> SceneRenderer::MakeOffscreen(const Compiled *plan,
                                                                      Extent frame) {
  if (plan == nullptr || !plan->Holds(Resource::Surface)) { return OwnedTexture{}; }
  SDL_GPUTextureCreateInfo wanted{};
  wanted.type = SDL_GPU_TEXTURETYPE_2D;
  wanted.format = FormatOf(plan->Format(Resource::Surface));
  wanted.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
  wanted.width = static_cast<Uint32>(frame.WidthPx);
  wanted.height = static_cast<Uint32>(frame.HeightPx);
  wanted.layer_count_or_depth = 1;
  wanted.num_levels = 1;
  OwnedTexture texture(Device_.Get(), SDL_CreateGPUTexture(Device_.Get(), &wanted));
  if (!texture) {
    ActiveState().WhyNot = std::string(Says::kTargetTextureFailed) + SDL_GetError();
    return std::unexpected(ActiveState().WhyNot);
  }
  return texture;
}

std::expected<void, std::string>
SceneRenderer::StandsOffscreen(FrameResources &frame, const Compiled *plan, bool presents) {
  if (presents || frame.Offscreen || frame.Width <= 0 || frame.Height <= 0) { return {}; }
  auto texture = MakeOffscreen(plan, {.WidthPx = frame.Width, .HeightPx = frame.Height});
  if (!texture) { return std::unexpected(texture.error()); }
  frame.Offscreen = std::move(*texture);
  frame.HostSurface = frame.Offscreen.Get();
  return {};
}

std::expected<void, std::string> SceneRenderer::Init(Extent frame,
                                                     std::shared_ptr<const Compiled> plan) {
  return InitForTarget(frame, std::move(plan), Showing_ != nullptr);
}

bool SceneRenderer::BeginsWorldCandidate(std::string &error) {
  if (Candidate_) {
    error = "a renderer already builds a world candidate, so a second candidate cannot borrow its "
            "temporary GPU state";
    return false;
  }
  Candidate_.emplace();
  if (!Candidate_->Content.Resources.CopySourcesFrom(State_.Content.Resources, error)) {
    Candidate_.reset();
    return false;
  }
  error.clear();
  return true;
}

bool SceneRenderer::PublishesWorldCandidate(std::string &error) {
  if (!Candidate_) {
    error = "a renderer cannot publish a world candidate it did not begin";
    return false;
  }
  if (Candidate_->Frame) { State_.Frame = std::move(*Candidate_->Frame); }
  static_cast<SceneStateCore &>(State_) = std::move(static_cast<SceneStateCore &>(*Candidate_));
  Candidate_.reset();
  ApplyWorldDeclarations();
  BindFrameResources();
  ActiveFrame().Cull.Invalidate();
  BeginTemporalRun();
  error.clear();
  return true;
}

void SceneRenderer::AbandonsWorldCandidate() noexcept {
  Candidate_.reset();
}

std::expected<void, std::string> SceneRenderer::ValidateExecutablePlan(const Compiled &plan) {
  for (const Stage stage : plan.Order()) {
    if (Executable(stage)) { continue; }
    Log::Error(LogTag::Render, "stage_not_executed", {{"stage", Row(stage).Name}});
    ActiveState().WhyNot = std::string("this device layer does not execute the stage '") +
                           Row(stage).Name +
                           "', which the catalogue offers and the consumer declared";
    return std::unexpected(ActiveState().WhyNot);
  }
  return {};
}

std::expected<bool, std::string> SceneRenderer::ReuseFrameResources(Extent frame,
                                                                    const Compiled &plan) {
  const bool reusesFrame =
      Candidate_ && State_.Ready && State_.Plan != nullptr && State_.Frame.Width == frame.WidthPx &&
      State_.Frame.Height == frame.HeightPx && State_.Plan->Specification() == plan.Specification();
  if (!reusesFrame) { return false; }
  ActiveState().Plan = State_.Plan;
  ActiveState().Content.DrawsGlass = plan.Holds(Stage::SubjectsTransmissive);
  ActiveState().Content.Subjects.AttachPipelines(State_.Frame.SubjectPipelines,
                                                 State_.Frame.Handles);
  if (!ActiveState().Content.Subjects.PrepareGroundStorage(State_.Frame.Handles,
                                                           ActiveState().WhyNot)) {
    return std::unexpected(ActiveState().WhyNot);
  }
  ActiveState().Content.Glass.AttachPipelines(State_.Frame.GlassPipelines, State_.Frame.Handles);
  ActiveState().Content.Glass.Shares(ActiveState().Content.Subjects.Owned());
  const auto ground =
      ActiveState().Content.Ground.Replace(State_.Frame.Handles.Device, {}, {}, Submission_);
  if (!ground) {
    ActiveState().WhyNot = ground.error();
    return std::unexpected(ActiveState().WhyNot);
  }
  ActiveState().Content.Subjects.GroundFrom({.Classes = ActiveState().Content.Ground.Classes(),
                                             .Palette = ActiveState().Content.Ground.Palette()});
  ActiveState().Content.Glass.GroundFrom({.Classes = ActiveState().Content.Ground.Classes(),
                                          .Palette = ActiveState().Content.Ground.Palette()});
  ActiveState().Content.Subjects.SkyFrom(State_.Frame.IrradianceBuffer.Get());
  if (ActiveState().Content.DrawsGlass) {
    ActiveState().Content.Glass.SkyFrom(State_.Frame.IrradianceBuffer.Get());
  }
  ActiveState().Submitted = false;
  ActiveState().Ready = true;
  return true;
}

std::expected<SceneRenderer::FrameResources, std::string>
SceneRenderer::BuildFrameResources(Extent frame, const Compiled &plan, bool presents) {
  SDL_GPUDevice *const device = Device_.Get();
  FrameResources candidate;
  candidate.Width = frame.WidthPx;
  candidate.Height = frame.HeightPx;
  candidate.Handles.Device = device;
  candidate.Handles.HdrFormat = FormatOf(plan.Format(Resource::SceneHdr));
  candidate.Handles.SurfaceFormat = FormatOf(plan.Format(Resource::FrameTex));
  candidate.Handles.Width = candidate.Width;
  candidate.Handles.Height = candidate.Height;

  for (const Compiled::Pass &pass : plan.Passes()) {
    if (pass.Kind == PassKind::Compute || pass.Depth == kNoEdge) { continue; }
    candidate.Handles.SceneColours = pass.Targets;
    break;
  }
  for (size_t r = 0; r < kResourceCount; ++r) {
    const auto id = static_cast<Resource>(r);
    if (!plan.Holds(id)) { continue; }
    Create(candidate, plan, id);
    if (Created(candidate, id)) { continue; }
    ActiveState().WhyNot =
        std::string("could not create render resource '") + Row(id).Name + "': " + SDL_GetError();
    return std::unexpected(ActiveState().WhyNot);
  }
  if (const auto stood = StandsOffscreen(candidate, &plan, presents); !stood) {
    return std::unexpected(stood.error());
  }
  if (!ConfigurePlanStages(candidate, plan, plan.Holds(Stage::SubjectsTransmissive))) {
    return std::unexpected(ActiveState().WhyNot);
  }
  if (Device_ && !Settle(ActiveState().WhyNot)) { return std::unexpected(ActiveState().WhyNot); }
  return candidate;
}

void SceneRenderer::PublishFrameResources(FrameResources frame,
                                          std::shared_ptr<const Compiled> plan) {
  ActiveState().Ready = false;
  ActiveState().Submitted = false;
  if (Candidate_) {
    Candidate_->Frame.emplace(std::move(frame));
  } else {
    State_.Frame = std::move(frame);
  }
  ActiveState().Plan = std::move(plan);
  ActiveState().Content.DrawsGlass = ActiveState().Plan->Holds(Stage::SubjectsTransmissive);
  if (!Candidate_) { ApplyWorldDeclarations(); }
  BindFrameResources();
  if (!Candidate_) { BeginTemporalRun(); }
  ActiveState().Ready = true;
}

void SceneRenderer::LogReadyPlan(SDL_GPUDevice *device) const {
  Log::Info(LogTag::Render,
            "device_ready",
            {{"width", ActiveFrame().Width},
             {"height", ActiveFrame().Height},
             {"driver", SDL_GetGPUDeviceDriver(device)},
             {"plan", ActiveState().Plan->Digest()},
             {"passes", ActiveState().Plan->PassCount()},
             {"stages", static_cast<int>(ActiveState().Plan->Order().size())}});
  for (size_t at = 0; at < kStageCount; ++at) {
    const auto stage = static_cast<Stage>(at);
    if (Executable(stage)) { continue; }
    Log::Info(LogTag::Render, "stage_without_a_body", {{"stage", Row(stage).Name}});
  }
  for (const std::string &merge : ActiveState().Plan->Merges()) {
    Log::Info(LogTag::Render, "plan_merge", {{"merge", merge}});
  }
  for (const std::string &alias : ActiveState().Plan->Aliases()) {
    Log::Info(LogTag::Render, "plan_alias", {{"alias", alias}});
  }
}

std::expected<void, std::string>
SceneRenderer::InitForTarget(Extent frame, std::shared_ptr<const Compiled> plan, bool presents) {
  ActiveState().WhyNot.clear();
  if (const auto valid = ValidateExecutablePlan(*plan); !valid) { return valid; }
  if (!Stands()) { return std::unexpected(ActiveState().WhyNot); }
  const auto reused = ReuseFrameResources(frame, *plan);
  if (!reused) { return std::unexpected(reused.error()); }
  if (*reused) { return {}; }
  auto candidate = BuildFrameResources(frame, *plan, presents);
  if (!candidate) { return std::unexpected(candidate.error()); }
  SDL_GPUDevice *const device = Device_.Get();
  PublishFrameResources(std::move(*candidate), std::move(plan));
  LogReadyPlan(device);
  return {};
}

AttachmentSet
SceneRenderer::ColoursForStage(const FrameResources &frame, const Compiled &plan, Stage wanted) {
  for (const Compiled::Pass &pass : plan.Passes()) {
    for (size_t at = pass.First; at < pass.First + pass.Count; ++at) {
      if (plan.Order()[at] == wanted) { return pass.Targets; }
    }
  }
  return frame.Handles.SceneColours;
}

bool SceneRenderer::ConfigurePlanStages(FrameResources &frame,
                                        const Compiled &plan,
                                        bool drawsGlass) {
  for (const Stage stage : plan.Order()) {
    std::string why;
    frame.Handles.SceneColours = ColoursForStage(frame, plan, stage);
    if (Configure(stage, frame, plan, drawsGlass, why)) { continue; }
    Log::Error(LogTag::Render, "stage_not_configured", {{"stage", Row(stage).Name}, {"msg", why}});
    ActiveState().WhyNot =
        std::string("the stage '") + Row(stage).Name + "' did not configure: " + why;
    return false;
  }
  return true;
}

void SceneRenderer::Create(FrameResources &frame, const Compiled &plan, Resource resource) {
  const auto target = [&](Resource of, SDL_GPUTextureUsageFlags usage) {
    SDL_GPUTextureCreateInfo wanted{};
    wanted.type = SDL_GPU_TEXTURETYPE_2D;
    wanted.format = FormatOf(plan.Format(of));
    wanted.usage = usage;
    wanted.width = static_cast<uint32_t>(frame.Width);
    wanted.height = static_cast<uint32_t>(frame.Height);
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
      frame.Samp = OwnedSampler(Device_.Get(), SDL_CreateGPUSampler(Device_.Get(), &wanted));
      return;
    }

    case Resource::OverlayAtlas: return;
    case Resource::SceneHdr: frame.HdrTex = target(resource, colour); return;
    case Resource::SceneTransmissive: frame.TransmissiveTex = target(resource, colour); return;
    case Resource::SceneComposited: frame.CompositedTex = target(resource, colour); return;
    case Resource::SceneAerial: frame.AerialTex = target(resource, colour); return;
    case Resource::SceneVelocity: frame.VelTex = target(resource, colour); return;
    case Resource::SceneShadingNormal: frame.ShadingNormalTex = target(resource, colour); return;
    case Resource::SceneSurfaceIdentity:
      frame.SurfaceIdentityTex = target(resource, colour);
      return;
    case Resource::SceneDepth:

      frame.DepthTex = target(
          resource, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER);
      return;
    case Resource::FrameTex: frame.FrameTex = target(resource, colour); return;

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
      wanted.format = FormatOf(plan.Format(resource));
      wanted.usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_SAMPLER;

      struct LutShape {
        uint32_t WidthPx;
        uint32_t HeightPx;
        OwnedTexture *Into;
      };

      const LutShape shape = [&frame, resource] -> LutShape {
        switch (resource) {
          case Resource::MultiScatterLut:
            return {.WidthPx = kMultiScatterLutSize,
                    .HeightPx = kMultiScatterLutSize,
                    .Into = &frame.MultiScatterLut};
          case Resource::SkyViewLut:
            return {.WidthPx = kSkyViewLutWidth,
                    .HeightPx = kSkyViewLutHeight,
                    .Into = &frame.SkyViewLut};
          default:
            return {.WidthPx = kTransmittanceLutWidth,
                    .HeightPx = kTransmittanceLutHeight,
                    .Into = &frame.TransmittanceLut};
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
      frame.LutSamp = OwnedSampler(Device_.Get(), SDL_CreateGPUSampler(Device_.Get(), &wanted));
      return;
    }
    case Resource::AtmosphereUniform:
    case Resource::CascadeUniform:
    case Resource::IrradianceBuffer: {
      SDL_GPUBufferCreateInfo wanted{};
      wanted.usage =
          SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
      wanted.size = kIrradianceFloats * static_cast<uint32_t>(sizeof(float));
      frame.IrradianceBuffer =
          OwnedBuffer(frame.Handles.Device, SDL_CreateGPUBuffer(frame.Handles.Device, &wanted));
      return;
    }
    case Resource::DepthPyramid: {
      SDL_GPUBufferCreateInfo wanted{};
      wanted.usage =
          SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
      wanted.size = PyramidOver({.WidthPx = static_cast<uint32_t>(frame.Width),
                                 .HeightPx = static_cast<uint32_t>(frame.Height)})
                        .Texels *
                    static_cast<uint32_t>(sizeof(float));
      frame.Pyramid =
          OwnedBuffer(frame.Handles.Device, SDL_CreateGPUBuffer(frame.Handles.Device, &wanted));
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
      frame.ShadowAtlas = OwnedTexture(Device_.Get(), SDL_CreateGPUTexture(Device_.Get(), &wanted));
      return;
    }
    case Resource::AoBuffer: return;

    case Resource::SceneLinear:

      frame.LinearTex[0] = target(resource, colour);
      frame.LinearTex[1] = target(resource, colour);
      frame.LinearAt = 0;
      frame.HistoryHeld = false;
      return;
    case Resource::kCount: return;
  }
}

bool SceneRenderer::Created(const FrameResources &frame, Resource resource) {
  switch (resource) {
    case Resource::LinearSampler: return static_cast<bool>(frame.Samp);
    case Resource::SceneHdr: return static_cast<bool>(frame.HdrTex);
    case Resource::SceneTransmissive: return static_cast<bool>(frame.TransmissiveTex);
    case Resource::SceneComposited: return static_cast<bool>(frame.CompositedTex);
    case Resource::SceneAerial: return static_cast<bool>(frame.AerialTex);
    case Resource::SceneVelocity: return static_cast<bool>(frame.VelTex);
    case Resource::SceneShadingNormal: return static_cast<bool>(frame.ShadingNormalTex);
    case Resource::SceneSurfaceIdentity: return static_cast<bool>(frame.SurfaceIdentityTex);
    case Resource::SceneDepth: return static_cast<bool>(frame.DepthTex);
    case Resource::FrameTex: return static_cast<bool>(frame.FrameTex);
    case Resource::TransmittanceLut: return static_cast<bool>(frame.TransmittanceLut);
    case Resource::MultiScatterLut: return static_cast<bool>(frame.MultiScatterLut);
    case Resource::SkyViewLut: return static_cast<bool>(frame.SkyViewLut);
    case Resource::LutSampler: return static_cast<bool>(frame.LutSamp);
    case Resource::AtmosphereUniform:
    case Resource::CascadeUniform:
    case Resource::IrradianceBuffer: return static_cast<bool>(frame.IrradianceBuffer);
    case Resource::DepthPyramid: return static_cast<bool>(frame.Pyramid);
    case Resource::VegetationTable:
    case Resource::Meter:
    case Resource::ShadowAtlas: return static_cast<bool>(frame.ShadowAtlas);
    case Resource::SceneLinear: return frame.LinearTex[0] && frame.LinearTex[1];
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
  return Target(ActiveFrame(), resource);
}

SDL_GPUTexture *SceneRenderer::Target(const FrameResources &frame, Resource resource) {
  switch (resource) {
    case Resource::OverlayAtlas: return nullptr;
    case Resource::SceneHdr: return frame.HdrTex.Get();
    case Resource::SceneTransmissive: return frame.TransmissiveTex.Get();
    case Resource::SceneComposited: return frame.CompositedTex.Get();
    case Resource::SceneAerial: return frame.AerialTex.Get();
    case Resource::SceneVelocity: return frame.VelTex.Get();
    case Resource::SceneShadingNormal: return frame.ShadingNormalTex.Get();
    case Resource::SceneSurfaceIdentity: return frame.SurfaceIdentityTex.Get();
    case Resource::SceneDepth: return frame.DepthTex.Get();
    case Resource::FrameTex: return frame.FrameTex.Get();

    case Resource::Surface: return frame.HostSurface;

    case Resource::ClusterSphere:
    case Resource::ClusterIndex:
    case Resource::ClusterJobs:
    case Resource::ClusterBatches:
    case Resource::ClusterKept:
    case Resource::ClusterSlot:
    case Resource::DrawIndex:
    case Resource::DrawArguments: return nullptr;
    case Resource::TransmittanceLut: return frame.TransmittanceLut.Get();
    case Resource::MultiScatterLut: return frame.MultiScatterLut.Get();
    case Resource::SkyViewLut: return frame.SkyViewLut.Get();
    case Resource::ShadowAtlas: return frame.ShadowAtlas.Get();
    case Resource::LinearSampler:
    case Resource::LutSampler:
    case Resource::AtmosphereUniform:
    case Resource::CascadeUniform:
    case Resource::VegetationTable:
    case Resource::IrradianceBuffer:
    case Resource::Meter:
    case Resource::DepthPyramid:
    case Resource::AoBuffer: return nullptr;

    case Resource::SceneLinear: return frame.LinearTex[frame.LinearAt].Get();
    case Resource::kCount: return nullptr;
  }
  return nullptr;
}

SDL_GPUBuffer *SceneRenderer::BufferFor(Resource resource) const {
  const SubjectResidency &resident = ActiveState().Content.Subjects.Resident();
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
    case Resource::IrradianceBuffer: return ActiveFrame().IrradianceBuffer.Get();
    case Resource::DepthPyramid: return ActiveFrame().Pyramid.Get();
    default: return nullptr;
  }
}

DisplayOptions SceneRenderer::Display() const {
  DisplayOptions options;
  options.Exposure = ActiveState().Plan->Exposure();
  options.Curve = ActiveState().Plan->Display();

  options.Temporal = ActiveState().Plan->Holds(Stage::TemporalResolve);
  return options;
}

SceneRenderer::Placed SceneRenderer::PictureRect() const {
  Placed out;
  out.LeftPx = 0;
  out.TopPx = 0;
  out.WidthPx = static_cast<double>(ActiveFrame().Width);
  out.HeightPx = static_cast<double>(ActiveFrame().Height);
  if (ActiveState().RegionW > 0 && ActiveState().RegionH > 0) {
    out.LeftPx = ActiveState().RegionX * static_cast<double>(ActiveFrame().Width);
    out.TopPx = ActiveState().RegionY * static_cast<double>(ActiveFrame().Height);
    out.WidthPx = ActiveState().RegionW * static_cast<double>(ActiveFrame().Width);
    out.HeightPx = ActiveState().RegionH * static_cast<double>(ActiveFrame().Height);
  }
  if (ActiveState().RegionAspect > 0 && out.WidthPx > 0 && out.HeightPx > 0) {
    const double fitted = out.WidthPx / out.HeightPx > ActiveState().RegionAspect
                              ? out.HeightPx * ActiveState().RegionAspect
                              : out.WidthPx;
    const double tall = fitted / ActiveState().RegionAspect;
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
  return ActiveState().Plan ? FormatOf(ActiveState().Plan->Format(Resource::Surface))
                            : SDL_GPU_TEXTUREFORMAT_INVALID;
}

SDL_GPUTexture *SceneRenderer::DisplaySource() const {
  const auto input = ActiveState().Plan->Holds(Stage::TemporalResolve) ? Resource::SceneAerial
                                                                       : Resource::SceneLinear;
  return Target(ActiveState().Plan->Bound(input));
}

SDL_GPUTexture *SceneRenderer::LinearSource() const {
  return Target(ActiveState().Plan->Bound(Resource::SceneLinear));
}

bool SceneRenderer::Configure(
    Stage stage, FrameResources &frame, const Compiled &plan, bool drawsGlass, std::string &error) {
  const Executor *seat = ExecutorOf(stage);
  if (seat == nullptr) {
    error = "this device layer does not execute the stage";
    return false;
  }
  if (seat->Configure == nullptr) { return true; }
  return seat->Configure(*this, frame, plan, drawsGlass, error);
}

bool SceneRenderer::ConfigureSubjects(SceneRenderer &renderer,
                                      FrameResources &frame,
                                      const Compiled &plan,
                                      bool drawsGlass,
                                      std::string &error) {
  (void)plan;
  return renderer.ActiveState().Content.Subjects.Configure(
      frame.SubjectPipelines, frame.Handles, nullptr, nullptr, drawsGlass, error);
}

bool SceneRenderer::ConfigureGlass(SceneRenderer &renderer,
                                   FrameResources &frame,
                                   const Compiled &plan,
                                   bool drawsGlass,
                                   std::string &error) {
  (void)plan;
  (void)drawsGlass;
  return renderer.ActiveState().Content.Glass.Configure(
      frame.GlassPipelines, frame.Handles, frame.HdrTex.Get(), frame.Samp.Get(), false, error);
}

bool SceneRenderer::ConfigureCompositeTransmission(SceneRenderer &renderer,
                                                   FrameResources &frame,
                                                   const Compiled &plan,
                                                   bool drawsGlass,
                                                   std::string &error) {
  static_cast<void>(renderer);
  (void)drawsGlass;
  return frame.CompositeTransmission.Configure(
      frame.Handles,
      {.Opaque = frame.HdrTex.Get(),
       .Transmissive = frame.TransmissiveTex.Get(),
       .Exact = frame.Samp.Get(),
       .Target = FormatOf(plan.Format(Resource::SceneComposited))},
      error);
}

bool SceneRenderer::ConfigureOverlay(SceneRenderer &renderer,
                                     FrameResources &frame,
                                     const Compiled &plan,
                                     bool drawsGlass,
                                     std::string &error) {
  (void)drawsGlass;
  static_cast<void>(renderer);
  return frame.OverlayPipe.Configure(
      frame.Handles, frame.Samp.Get(), FormatOf(plan.Format(Resource::FrameTex)), error);
}

bool SceneRenderer::ConfigurePresent(SceneRenderer &renderer,
                                     FrameResources &frame,
                                     const Compiled &plan,
                                     bool drawsGlass,
                                     std::string &error) {
  static_cast<void>(renderer);
  (void)plan;
  (void)drawsGlass;
  return frame.Present.Configure(frame.Handles, frame.FrameTex.Get(), frame.Samp.Get(), error);
}

bool SceneRenderer::ConfigureTonemap(SceneRenderer &renderer,
                                     FrameResources &frame,
                                     const Compiled &plan,
                                     bool drawsGlass,
                                     std::string &error) {
  static_cast<void>(renderer);
  (void)drawsGlass;
  const Resource input =
      plan.Holds(Stage::TemporalResolve) ? Resource::SceneAerial : Resource::SceneLinear;
  DisplayOptions display;
  display.Exposure = plan.Exposure();
  display.Curve = plan.Display();
  display.Temporal = plan.Holds(Stage::TemporalResolve);
  return frame.Tonemap.Configure(frame.Handles,
                                 {.Scene = Target(frame, plan.Bound(input)),
                                  .Depth = frame.DepthTex.Get(),
                                  .Exact = frame.Samp.Get(),
                                  .Linear = FormatOf(plan.Format(Resource::SceneLinear))},
                                 display,
                                 error);
}

bool SceneRenderer::ConfigureMediumTransmittance(SceneRenderer &renderer,
                                                 FrameResources &frame,
                                                 const Compiled &plan,
                                                 bool drawsGlass,
                                                 std::string &error) {
  static_cast<void>(renderer);
  (void)plan;
  (void)drawsGlass;
  return frame.MediumTransmittance.Configure(frame.Handles, frame.TransmittanceLut.Get(), error);
}

bool SceneRenderer::ConfigureMediumMultiScatter(SceneRenderer &renderer,
                                                FrameResources &frame,
                                                const Compiled &plan,
                                                bool drawsGlass,
                                                std::string &error) {
  static_cast<void>(renderer);
  (void)plan;
  (void)drawsGlass;
  return frame.MultiScatter.Configure(frame.Handles,
                                      frame.TransmittanceLut.Get(),
                                      frame.LutSamp.Get(),
                                      frame.MultiScatterLut.Get(),
                                      error);
}

bool SceneRenderer::ConfigureMediumRadiance(SceneRenderer &renderer,
                                            FrameResources &frame,
                                            const Compiled &plan,
                                            bool drawsGlass,
                                            std::string &error) {
  static_cast<void>(renderer);
  (void)plan;
  (void)drawsGlass;
  return frame.Radiance.Configure(frame.Handles,
                                  frame.TransmittanceLut.Get(),
                                  frame.MultiScatterLut.Get(),
                                  frame.LutSamp.Get(),
                                  frame.SkyViewLut.Get(),
                                  error);
}

bool SceneRenderer::ConfigureSky(SceneRenderer &renderer,
                                 FrameResources &frame,
                                 const Compiled &plan,
                                 bool drawsGlass,
                                 std::string &error) {
  static_cast<void>(renderer);
  (void)plan;
  (void)drawsGlass;
  return frame.Sky.Configure(frame.Handles,
                             {.SkyView = frame.SkyViewLut.Get(),
                              .Transmittance = frame.TransmittanceLut.Get(),
                              .Lut = frame.LutSamp.Get()},
                             error);
}

bool SceneRenderer::ConfigureAerialPerspective(SceneRenderer &renderer,
                                               FrameResources &frame,
                                               const Compiled &plan,
                                               bool drawsGlass,
                                               std::string &error) {
  static_cast<void>(renderer);
  (void)drawsGlass;
  return frame.Aerial.Configure(frame.Handles,
                                {.Scene = Target(frame, plan.Bound(Resource::SceneComposited)),
                                 .Depth = frame.DepthTex.Get(),
                                 .SkyView = frame.SkyViewLut.Get(),
                                 .Transmittance = frame.TransmittanceLut.Get(),
                                 .Exact = frame.Samp.Get(),
                                 .Lut = frame.LutSamp.Get()},
                                FormatOf(plan.Format(Resource::SceneAerial)),
                                error);
}

bool SceneRenderer::ConfigureLightVisibility(SceneRenderer &renderer,
                                             FrameResources &frame,
                                             const Compiled &plan,
                                             bool drawsGlass,
                                             std::string &error) {
  (void)plan;
  (void)drawsGlass;
  return frame.Shadow.Configure(renderer.ActiveState().Content.Subjects, frame.Handles, error);
}

void SceneRenderer::ApplyWorldDeclarations() {
  if (ActiveState().MediumDeclared) {
    ActiveFrame().MediumTransmittance.Declare(ActiveState().Medium);
    ActiveFrame().MultiScatter.Declare(ActiveState().Medium);
    ActiveFrame().Radiance.Declare(
        ActiveState().Medium, ActiveState().CosSunZenith, ActiveState().EyeHeightM);
    ActiveFrame().SkyIrradianceStage.Declare(ActiveState().Medium, ActiveState().CosSunZenith);
  }
  if (ActiveState().SkyDeclared) {
    const SkyStanding stands = {.SunDir = ActiveState().SkyToSun,
                                .WorldUp = ActiveState().SkyUp,
                                .IlluminanceLux = ActiveState().SkyIlluminanceLux,
                                .EyeHeightM = ActiveState().EyeHeightM};
    ActiveFrame().Sky.Declare(ActiveState().Medium, stands);
    ActiveFrame().Aerial.Declare(ActiveState().Medium, stands);
  }
  if (ActiveState().ShadowDeclared) {
    ActiveFrame().Shadow.Declare({.ToSun = ActiveState().ShadowToSun, .Up = ActiveState().ShadowUp},
                                 ActiveState().ShadowRadiusM);
  }
}

void SceneRenderer::BindFrameResources() {
  ActiveState().Content.Subjects.UsePipelines(ActiveFrame().SubjectPipelines);
  ActiveState().Content.Glass.UsePipelines(ActiveFrame().GlassPipelines);
  ActiveState().Content.Glass.Shares(ActiveState().Content.Subjects.Owned());
  ActiveFrame().Shadow.Binds(ActiveState().Content.Subjects);
  ActiveFrame().Cull.Binds(ActiveState().Content.Subjects);
  ActiveState().Content.Subjects.SkyFrom(ActiveFrame().IrradianceBuffer.Get());
  if (ActiveState().Content.DrawsGlass) {
    ActiveState().Content.Glass.SkyFrom(ActiveFrame().IrradianceBuffer.Get());
  }
}

void SceneRenderer::Picture(bool picture, const PassRecording &into) {
  SDL_GPUViewport where{};
  const Placed rect = PictureRect();
  where.x = picture ? static_cast<float>(rect.LeftPx) : 0.0f;
  where.y = picture ? static_cast<float>(rect.TopPx) : 0.0f;
  where.w = picture ? static_cast<float>(rect.WidthPx) : static_cast<float>(ActiveFrame().Width);
  where.h = picture ? static_cast<float>(rect.HeightPx) : static_cast<float>(ActiveFrame().Height);
  where.min_depth = 0.0f;
  where.max_depth = 1.0f;
  if (into.Pass != nullptr) { SDL_SetGPUViewport(into.Pass, &where); }
}

FrameContext SceneRenderer::Framing() const {
  FrameContext ctx{};
  if (ActiveState().OrthoM > 0) {
    for (int axis = 0; axis < 3; ++axis) {
      ctx.ViewPosition[axis] = static_cast<float>(-ActiveState().Camera.Forward[axis]);
    }
    ctx.ViewPosition[3] = 0;
  }
  for (int axis = 0; axis < 3; axis++) {
    ctx.PreViewTranslation[axis] = -ActiveState().Camera.EyeM[axis];
  }

  ctx.Mvp = Through().ViewProjection(ActiveState().Camera);
  for (int axis = 0; axis < 3; axis++) {
    ctx.PrevPreViewTranslation[axis] =
        ActiveState().Submitted ? -ActiveState().PrevEye[axis] : ctx.PreViewTranslation[axis];
  }
  for (int at = 0; at < 16; at++) {
    ctx.PrevMvp[at] = ActiveState().Submitted ? ActiveState().PrevMvp[at] : ctx.Mvp[at];
  }
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
    const SubjectDraw &drew =
        stage == Stage::Subjects ? ActiveState().Content.Subjects : ActiveState().Content.Glass;
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
  ActiveState().Content.Subjects.Encode(ctx, into);
}

void SceneRenderer::EncodeGlass(const FrameContext &ctx, const PassRecording &into) {
  Picture(true, into);
  ActiveState().Content.Glass.Encode(ctx, into);
}

void SceneRenderer::EncodeCompositeTransmission(const FrameContext &ctx,
                                                const PassRecording &into) {
  Picture(true, into);
  ActiveFrame().CompositeTransmission.Encode(ctx, into);
}

void SceneRenderer::EncodeTonemap(const FrameContext &ctx, const PassRecording &into) {
  ActiveFrame().Tonemap.Bind(DisplaySource());
  const Vec2f delta = {{ActiveState().Jitter[0] - ActiveState().PrevJitter[0],
                        ActiveState().Jitter[1] - ActiveState().PrevJitter[1]}};
  ActiveFrame().Tonemap.BindTemporal(
      {.History = ActiveFrame().LinearTex[1 - ActiveFrame().LinearAt].Get(),
       .Velocity = ActiveFrame().VelTex.Get()},
      Extent{.WidthPx = ActiveFrame().Width, .HeightPx = ActiveFrame().Height},
      delta,
      ActiveFrame().HistoryHeld);
  Picture(true, into);
  ActiveFrame().Tonemap.Encode(ctx, into);
}

void SceneRenderer::EncodeOverlay(const FrameContext &ctx, const PassRecording &into) {
  Picture(false, into);
  ActiveFrame().OverlayPipe.Bind(
      Extent{.WidthPx = ActiveFrame().Width, .HeightPx = ActiveFrame().Height});
  ActiveFrame().OverlayPipe.Encode(ActiveState().Content.Overlay, ctx, into);
}

void SceneRenderer::EncodePresent(const FrameContext &ctx, const PassRecording &into) {
  {
    std::string why;
    if (!ActiveFrame().Present.For(ActiveFrame().Handles, SurfaceFormat(), why)) {
      Log::Error(LogTag::Render, "present_not_built", {{"msg", why}});
      return;
    }
  }
  Picture(false, into);
  ActiveFrame().Present.Encode(ctx, into);
}

void SceneRenderer::EncodeMediumTransmittance(const FrameContext &ctx, const PassRecording &into) {
  (void)ctx;
  ActiveFrame().MediumTransmittance.Encode(into);
}

void SceneRenderer::EncodeMediumMultiScatter(const FrameContext &ctx, const PassRecording &into) {
  (void)ctx;
  ActiveFrame().MultiScatter.Encode(into);
}

void SceneRenderer::EncodeMediumRadiance(const FrameContext &ctx, const PassRecording &into) {
  (void)ctx;
  ActiveFrame().Radiance.Encode(into);
}

bool SceneRenderer::SetGroundClasses(std::span<const uint32_t> classes,
                                     std::span<const float> palette,
                                     std::string &error) {
  const auto uploaded = ActiveState().Content.Ground.Replace(
      ActiveFrame().Handles.Device, classes, palette, Submission_);
  if (!uploaded) {
    error = uploaded.error();
    return false;
  }
  ActiveState().Content.Subjects.GroundFrom({.Classes = ActiveState().Content.Ground.Classes(),
                                             .Palette = ActiveState().Content.Ground.Palette()});
  ActiveState().Content.Glass.GroundFrom({.Classes = ActiveState().Content.Ground.Classes(),
                                          .Palette = ActiveState().Content.Ground.Palette()});
  ActiveState().Content.Resources.SetGroundClassification(classes, palette);
  return true;
}

bool SceneRenderer::ConfigureIrradiance(SceneRenderer &renderer,
                                        FrameResources &frame,
                                        const Compiled &plan,
                                        bool drawsGlass,
                                        std::string &error) {
  (void)plan;
  (void)drawsGlass;
  if (!renderer.ActiveState().Content.Ground.Ready()) {
    const auto uploaded = renderer.ActiveState().Content.Ground.Replace(
        frame.Handles.Device, {}, {}, renderer.Submission_);
    if (!uploaded) {
      error = uploaded.error();
      return false;
    }
    renderer.ActiveState().Content.Subjects.GroundFrom(
        {.Classes = renderer.ActiveState().Content.Ground.Classes(),
         .Palette = renderer.ActiveState().Content.Ground.Palette()});
    renderer.ActiveState().Content.Glass.GroundFrom(
        {.Classes = renderer.ActiveState().Content.Ground.Classes(),
         .Palette = renderer.ActiveState().Content.Ground.Palette()});
  }
  return frame.SkyIrradianceStage.Configure(frame.Handles,
                                            frame.TransmittanceLut.Get(),
                                            frame.MultiScatterLut.Get(),
                                            frame.LutSamp.Get(),
                                            frame.IrradianceBuffer.Get(),
                                            error);
}

void SceneRenderer::EncodeIrradiance(const FrameContext &ctx, const PassRecording &into) {
  (void)ctx;
  ActiveFrame().SkyIrradianceStage.Encode(into);
}

bool SceneRenderer::ConfigureDepthPyramid(SceneRenderer &renderer,
                                          FrameResources &frame,
                                          const Compiled &plan,
                                          bool drawsGlass,
                                          std::string &error) {
  static_cast<void>(renderer);
  (void)plan;
  (void)drawsGlass;
  return frame.PyramidStage.Configure(frame.Handles,
                                      frame.DepthTex.Get(),
                                      frame.Samp.Get(),
                                      frame.Pyramid.Get(),
                                      {.WidthPx = frame.Width, .HeightPx = frame.Height},
                                      error);
}

void SceneRenderer::EncodeDepthPyramid(const FrameContext &ctx, const PassRecording &into) {
  (void)ctx;
  ActiveFrame().PyramidStage.Encode(into);
}

bool SceneRenderer::ConfigureSubjectCull(SceneRenderer &renderer,
                                         FrameResources &frame,
                                         const Compiled &plan,
                                         bool drawsGlass,
                                         std::string &error) {
  (void)plan;
  (void)drawsGlass;
  frame.Cull.PyramidFrom(frame.Pyramid.Get(),
                         PyramidOver({.WidthPx = static_cast<uint32_t>(frame.Width),
                                      .HeightPx = static_cast<uint32_t>(frame.Height)}));
  return frame.Cull.Configure(renderer.ActiveState().Content.Subjects, frame.Handles, error);
}

void SceneRenderer::EncodeSubjectCull(const FrameContext &ctx, const PassRecording &into) {
  ActiveFrame().Cull.Projects(static_cast<float>(ActiveFrame().Height));
  ActiveFrame().Cull.EncodeCull(ctx, into);
}

void SceneRenderer::EncodeSubjectScan(const FrameContext &ctx, const PassRecording &into) {
  ActiveFrame().Cull.EncodeScan(ctx, into);
}

void SceneRenderer::EncodeSubjectCompact(const FrameContext &ctx, const PassRecording &into) {
  ActiveFrame().Cull.EncodeCompact(ctx, into);
}

void SceneRenderer::EncodeLightVisibility(const FrameContext &ctx, const PassRecording &into) {
  ActiveFrame().Shadow.Encode(ctx, into);
  ActiveState().Content.Subjects.ShadowedBy(ActiveFrame().ShadowAtlas.Get(),
                                            ActiveFrame().LutSamp.Get(),
                                            ActiveFrame().Shadow.LightFromWorld());
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
  ActiveFrame().Shadow.Prepare(Framing());
  Touched_[static_cast<size_t>(Resource::ShadowAtlas)] = ActiveFrame().Shadow.Cached();
}

EyeBasis SceneRenderer::Eye() const {
  assert(ActiveState().OrthoM > 0.0f || ActiveState().FovDeg > 0.0f);
  EyeBasis eye;
  eye.TanHalfHeight = ActiveState().OrthoM > 0.0f
                          ? 0.0f
                          : std::tan(static_cast<float>(ActiveState().FovDeg * kDeg2Rad) * 0.5f);
  eye.TanHalfWidth =
      eye.TanHalfHeight * (PictureH() > 0.0 ? static_cast<float>(PictureW() / PictureH()) : 1.0f);
  for (int axis = 0; axis < 3; ++axis) {
    eye.Right[axis] = static_cast<float>(ActiveState().Camera.Right[axis]);
    eye.Up[axis] = static_cast<float>(ActiveState().Camera.Up[axis]);
    eye.Forward[axis] = static_cast<float>(ActiveState().Camera.Forward[axis]);
  }
  return eye;
}

void SceneRenderer::EncodeAerialPerspective(const FrameContext &ctx, const PassRecording &into) {
  Picture(true, into);
  ActiveFrame().Aerial.SetBasis(Eye());
  const Mat4f projection = Through().Projection();
  ActiveFrame().Aerial.SetDepthReconstruction(
      {{projection[14], projection[15], projection[10], -projection[11]}});
  ActiveFrame().Aerial.Encode(ctx, into);
}

void SceneRenderer::EncodeSky(const FrameContext &ctx, const PassRecording &into) {
  Picture(true, into);
  ActiveFrame().Sky.SetBasis(Eye());
  ActiveFrame().Sky.Encode(ctx, into);
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
  const Compiled::Pass &declared = ActiveState().Plan->Passes()[pass];
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
    EncodeStage(ActiveState().Plan->Order()[declared.First + at], into);
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
    attachment.store_op =
        ActiveState().Plan->Stored(wanted) ? SDL_GPU_STOREOP_STORE : SDL_GPU_STOREOP_DONT_CARE;

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
    depth.store_op = ActiveState().Plan->Stored(declared.Depth) ? SDL_GPU_STOREOP_STORE
                                                                : SDL_GPU_STOREOP_DONT_CARE;
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
    EncodeStage(ActiveState().Plan->Order()[declared.First + at], into);
  }
  SDL_EndGPURenderPass(into.Pass);
}

void SceneRenderer::BeginTemporalRun() {
  ActiveState().HistoryStarted = false;
  ActiveState().JitterAt = 0;
  ActiveState().Jitter[0] = 0.0f;
  ActiveState().Jitter[1] = 0.0f;
  ActiveState().PrevJitter[0] = 0.0f;
  ActiveState().PrevJitter[1] = 0.0f;
  ActiveFrame().LinearAt = 0;
  ActiveFrame().HistoryHeld = false;
}

std::expected<void, std::string> SceneRenderer::PrepareFrame() {
  if (!ActiveState().Ready) {
    return std::unexpected(ActiveState().WhyNot.empty() ? Says::kRendererNotReady
                                                        : ActiveState().WhyNot);
  }
  if (!ActiveState().CameraFull) { return std::unexpected(Says::kCameraNotConfigured); }

  ActiveState().Content.Subjects.CastsNoShadow();
  for (bool &touched : Touched_) { touched = false; }
  SettleShadow();
  {
    std::string why;
    if (!ActiveState().Content.Subjects.HandTables(why) ||
        !ActiveState().Content.Subjects.HandPlacements(false, why) ||
        (ActiveState().Content.DrawsGlass && !ActiveState().Content.Glass.HandTables(why))) {
      return std::unexpected(std::move(why));
    }
  }
  return {};
}

std::expected<void, std::string> SceneRenderer::RenderFrame() {
  const auto published = PublishedWorld();
  return RenderPublishedFrame();
}

std::expected<void, std::string> SceneRenderer::RenderPublishedFrame() {
  auto prepared = PrepareFrame();
  if (!prepared) { return prepared; }
  SDL_GPUCommandBuffer *commands = Submission_.Acquire(Submission_.Context, Device_.Get());
  if (commands == nullptr) { return std::unexpected(SDL_GetError()); }
  std::string uploadError;
  if (!ActiveState().Content.Subjects.FlushCrossings(commands, uploadError) ||
      (ActiveState().Content.DrawsGlass &&
       !ActiveState().Content.Glass.FlushCrossings(commands, uploadError))) {
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
    ActiveFrame().Shown.WidthPx = static_cast<int>(gotW);
    ActiveFrame().Shown.HeightPx = static_cast<int>(gotH);
    ActiveFrame().HostSurface = swapchain;
  }

  const auto previousJitter = ActiveState().Jitter;
  const auto previousPrevJitter = ActiveState().PrevJitter;
  const auto previousJitterAt = ActiveState().JitterAt;
  const auto previousHistoryStarted = ActiveState().HistoryStarted;
  const auto previousHistoryHeld = ActiveFrame().HistoryHeld;
  const auto previousLinearAt = ActiveFrame().LinearAt;
  if (ActiveState().Plan->Holds(Stage::TemporalResolve)) {
    ActiveState().PrevJitter = ActiveState().Jitter;
    ActiveState().JitterAt = (ActiveState().JitterAt + 1) % kJitterPeriod;
    ActiveState().Jitter = HaltonJitter(ActiveState().JitterAt);

    ActiveFrame().HistoryHeld = ActiveState().HistoryStarted;
    ActiveState().HistoryStarted = true;
    ActiveFrame().LinearAt = 1 - ActiveFrame().LinearAt;
  }
  StageSubmission stageSubmission;
  const auto restoreTemporalState = [&] {
    ActiveState().Jitter = previousJitter;
    ActiveState().PrevJitter = previousPrevJitter;
    ActiveState().JitterAt = previousJitterAt;
    ActiveState().HistoryStarted = previousHistoryStarted;
    ActiveFrame().HistoryHeld = previousHistoryHeld;
    ActiveFrame().LinearAt = previousLinearAt;
  };

  if (!ActiveState().Content.Subjects.Ground().Cull(
          Framing(), ActiveState().Content.Subjects.AnchorM(), commands, uploadError)) {
    SDL_CancelGPUCommandBuffer(commands);
    restoreTemporalState();
    return std::unexpected(std::move(uploadError));
  }

  for (size_t pass = 0; pass < ActiveState().Plan->Passes().size(); ++pass) {
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
  if (swapchain != nullptr) { ActiveFrame().HostSurface = ActiveFrame().Offscreen.Get(); }
  if (Landed_[LandedAt_] == nullptr) {
    std::string error = SDL_GetError();
    restoreTemporalState();
    return std::unexpected(std::move(error));
  }
  ActiveState().Content.Subjects.CommitCrossings();
  if (ActiveState().Content.DrawsGlass) { ActiveState().Content.Glass.CommitCrossings(); }
  stageSubmission.Commit();
  LandedAt_ = (LandedAt_ + 1) % kFramesInFlight;
  for (int axis = 0; axis < 3; axis++) {
    ActiveState().PrevEye[axis] = ActiveState().Camera.EyeM[axis];
  }
  ActiveState().Content.Subjects.CarryFrame();
  ActiveState().Content.Glass.CarryFrame();

  ActiveState().PrevMvp = Through().ViewProjection(ActiveState().Camera);
  ActiveState().Submitted = true;
  return {};
}

void SceneRenderer::WaitForGpu() {
  if (!ActiveState().Ready) { return; }
  SDL_WaitForGPUIdle(Device_.Get());

  for (SDL_GPUFence *&held : Landed_) {
    if (held == nullptr) { continue; }
    SDL_ReleaseGPUFence(Device_.Get(), held);
    held = nullptr;
  }
}

ReadState SceneRenderer::ReadPixels(std::vector<uint8_t> &rgba) {
  ActiveState().WhyNot.clear();
  if (!ActiveState().Ready || !ActiveState().Submitted) {
    ActiveState().WhyNot = Says::kNoReadbackFrame;
    return ReadState::Failed;
  }
  if (!ActiveState().Plan) {
    ActiveState().WhyNot = Says::kNoColourOutput;
    return ReadState::Failed;
  }
  const auto source =
      ActiveState().Plan->Holds(Resource::FrameTex) ? Resource::FrameTex : Resource::Surface;
  if (!ActiveState().Plan->Holds(source)) {
    ActiveState().WhyNot = Says::kNoColourOutput;
    return ReadState::Failed;
  }
  SDL_GPUTexture *const held =
      source == Resource::FrameTex ? ActiveFrame().FrameTex.Get() : ActiveFrame().Offscreen.Get();
  if (held == nullptr) {
    ActiveState().WhyNot = Says::kNoColourTexture;
    return ReadState::Failed;
  }
  Readback read;
  if (read.FromTexture(Device_.Get(),
                       held,
                       {.WidthPx = ActiveFrame().Width, .HeightPx = ActiveFrame().Height},
                       4u) != ReadState::Ready) {
    ActiveState().WhyNot = std::string(Says::kColourReadbackFailed) + SDL_GetError();
    return ReadState::Failed;
  }
  rgba.resize(static_cast<size_t>(ActiveFrame().Width) * static_cast<size_t>(ActiveFrame().Height) *
              4u);
  std::memcpy(rgba.data(), read.Rows(), rgba.size());
  const auto format = FormatOf(ActiveState().Plan->Format(source));
  if (format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM ||
      format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB) {
    for (size_t at = 0; at + 3 < rgba.size(); at += 4) { std::swap(rgba[at], rgba[at + 2]); }
  }
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadDepth(std::vector<float> &depth) {
  if (!ActiveState().Ready || !ActiveFrame().DepthTex) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.Get(),
                       ActiveFrame().DepthTex.Get(),
                       {.WidthPx = ActiveFrame().Width, .HeightPx = ActiveFrame().Height},
                       4u) != ReadState::Ready) {
    return ReadState::Failed;
  }
  depth.resize(static_cast<size_t>(ActiveFrame().Width) *
               static_cast<size_t>(ActiveFrame().Height));
  std::memcpy(depth.data(), read.Rows(), depth.size() * sizeof(float));
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadSceneLinear(std::vector<float> &rgba) {
  SDL_GPUTexture *source = LinearSource();
  if (!ActiveState().Ready || (source == nullptr)) { return ReadState::Failed; }
  const bool wide = ActiveState().Plan->Format(Resource::SceneLinear) == TexelFormat::Rgba32Float;
  Readback read;
  if (read.FromTexture(Device_.Get(),
                       source,
                       {.WidthPx = ActiveFrame().Width, .HeightPx = ActiveFrame().Height},
                       wide ? 16u : 8u) != ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components =
      static_cast<size_t>(ActiveFrame().Width) * static_cast<size_t>(ActiveFrame().Height) * 4u;
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
  if (!ActiveState().Ready || !ActiveFrame().ShadowAtlas ||
      !ActiveFrame().Shadow.HasSubmittedData()) {
    return ReadState::Failed;
  }
  Readback read;
  if (read.FromTexture(Device_.Get(),
                       ActiveFrame().ShadowAtlas.Get(),
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
  const SubjectResidency &resident = ActiveState().Content.Subjects.Resident();
  SDL_GPUBuffer *const args = resident.Buffer(SubjectResidency::Stream::DrawArguments).Get();
  const uint32_t rows = ActiveState().Content.Subjects.ClusterBatchRows();
  if (!ActiveState().Ready || args == nullptr || rows == 0) { return ReadState::Failed; }
  Readback read;
  const uint32_t bytes = rows * 5u * static_cast<uint32_t>(sizeof(uint32_t));
  if (read.FromBuffer(Device_.Get(), args, bytes) != ReadState::Ready) { return ReadState::Failed; }
  const auto *const held = reinterpret_cast<const uint32_t *>(read.Rows());
  into.Arguments.assign(held, held + static_cast<size_t>(rows) * 5u);
  for (uint32_t at = 0; at < rows; ++at) {
    into.Indices += held[static_cast<size_t>(at) * 5];
    into.Batches += held[static_cast<size_t>(at) * 5] > 0u ? 1u : 0u;
  }
  const uint32_t jobs = ActiveState().Content.Subjects.ClusterJobs();
  SDL_GPUBuffer *const kept = resident.Buffer(SubjectResidency::Stream::ClusterKept).Get();
  if (jobs > 0 && kept != nullptr) {
    Readback visibility;
    const uint32_t keptBytes = jobs * static_cast<uint32_t>(sizeof(uint32_t));
    if (visibility.FromBuffer(Device_.Get(), kept, keptBytes) != ReadState::Ready) {
      into = {};
      return ReadState::Failed;
    }
    const auto *const values = reinterpret_cast<const uint32_t *>(visibility.Rows());
    into.Visibility.assign(values, values + jobs);
  }
  SDL_GPUBuffer *const index = resident.Buffer(SubjectResidency::Stream::DrawIndex).Get();
  if (index == nullptr || into.Indices == 0) { return ReadState::Ready; }
  Readback drawn;
  const uint32_t indexBytes = into.Indices * static_cast<uint32_t>(sizeof(uint32_t));
  if (drawn.FromBuffer(Device_.Get(), index, indexBytes) != ReadState::Ready) {
    into = {};
    return ReadState::Failed;
  }
  const auto *const indices = reinterpret_cast<const uint32_t *>(drawn.Rows());
  into.DrawIndex.assign(indices, indices + into.Indices);
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadPyramid(PyramidDepths &into) {
  into = {};
  if (!ActiveState().Ready || !ActiveFrame().Pyramid) { return ReadState::Failed; }
  const PyramidShape shape = PyramidOver({.WidthPx = static_cast<uint32_t>(ActiveFrame().Width),
                                          .HeightPx = static_cast<uint32_t>(ActiveFrame().Height)});
  const uint32_t texels = shape.Wide[0] * shape.High[0];
  if (texels == 0) { return ReadState::Failed; }
  const ReadState landed = ActiveFrame().PyramidRead.Poll();
  if (landed == ReadState::Failed) {
    return ActiveFrame().PyramidRead.Enqueue(Device_.Get(),
                                             ActiveFrame().Pyramid.Get(),
                                             texels * static_cast<uint32_t>(sizeof(float))) ==
                   ReadState::Failed
               ? ReadState::Failed
               : ReadState::Pending;
  }
  if (landed == ReadState::Pending) { return ReadState::Pending; }
  const auto *const held = reinterpret_cast<const float *>(ActiveFrame().PyramidRead.Rows());
  double summed = 0.0;
  into.Nearest = held[0];
  into.Farthest = held[0];
  for (uint32_t at = 0; at < texels; ++at) {
    into.Nearest = std::max(held[at], into.Nearest);
    into.Farthest = std::min(held[at], into.Farthest);
    summed += static_cast<double>(held[at]);
  }
  into.Mean = static_cast<float>(summed / static_cast<double>(texels));
  ActiveFrame().PyramidRead.Release();
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadSkyIrradiance(std::span<float, kIrradianceFloats> out) {
  if (!ActiveState().Ready || !ActiveFrame().IrradianceBuffer ||
      !ActiveFrame().SkyIrradianceStage.Settled()) {
    return ReadState::Failed;
  }
  Readback read;
  if (read.FromBuffer(Device_.Get(),
                      ActiveFrame().IrradianceBuffer.Get(),
                      kIrradianceFloats * static_cast<uint32_t>(sizeof(float))) !=
      ReadState::Ready) {
    return ReadState::Failed;
  }
  std::memcpy(out.data(), read.Rows(), kIrradianceFloats * sizeof(float));
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadShadingNormal(std::vector<float> &xyz) {
  SDL_GPUTexture *source = ActiveFrame().ShadingNormalTex.Get();
  if (!ActiveState().Ready || (source == nullptr)) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.Get(),
                       source,
                       {.WidthPx = ActiveFrame().Width, .HeightPx = ActiveFrame().Height},
                       8u) != ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components =
      static_cast<size_t>(ActiveFrame().Width) * static_cast<size_t>(ActiveFrame().Height) * 4u;
  xyz.resize(components);
  for (size_t component = 0; component < components; ++component) {
    uint16_t bits = 0;
    std::memcpy(&bits, read.Rows() + component * sizeof(uint16_t), sizeof bits);
    xyz[component] = HalfToFloat(bits);
  }
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadSceneVelocity(std::vector<float> &xy) {
  SDL_GPUTexture *source = ActiveFrame().VelTex.Get();
  if (!ActiveState().Ready || (source == nullptr)) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.Get(),
                       source,
                       {.WidthPx = ActiveFrame().Width, .HeightPx = ActiveFrame().Height},
                       4u) != ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components =
      static_cast<size_t>(ActiveFrame().Width) * static_cast<size_t>(ActiveFrame().Height) * 2u;
  xy.resize(components);
  for (size_t component = 0; component < components; ++component) {
    uint16_t bits = 0;
    std::memcpy(&bits, read.Rows() + component * sizeof(uint16_t), sizeof bits);
    xy[component] = HalfToFloat(bits);
  }
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadSurfaceIdentity(std::vector<float> &slot) {
  SDL_GPUTexture *source = ActiveFrame().SurfaceIdentityTex.Get();
  if (!ActiveState().Ready || (source == nullptr)) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.Get(),
                       source,
                       {.WidthPx = ActiveFrame().Width, .HeightPx = ActiveFrame().Height},
                       16u) != ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components =
      static_cast<size_t>(ActiveFrame().Width) * static_cast<size_t>(ActiveFrame().Height) * 4u;
  slot.resize(components);
  std::memcpy(slot.data(), read.Rows(), components * sizeof(float));
  return ReadState::Ready;
}

void SceneRenderer::StopShowing() {
  ActiveFrame().Offscreen.Reset();
  ActiveFrame().HostSurface = nullptr;
  ActiveFrame().Shown = {};
  ActiveState().Submitted = false;
  if (Showing_ == nullptr) { return; }
  SDL_ReleaseWindowFromGPUDevice(Device_.Get(), Showing_);
  Showing_ = nullptr;
}

std::expected<SDL_GPUPresentMode, std::string> SceneRenderer::ClaimWindow(SDL_Window *window) {
  if (!SDL_ClaimWindowForGPUDevice(Device_.Get(), window)) {
    ActiveState().WhyNot = std::string(Says::kWindowClaimFailed) + SDL_GetError();
    return std::unexpected(ActiveState().WhyNot);
  }
  constexpr auto composition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR;
  if (!SDL_WindowSupportsGPUSwapchainComposition(Device_.Get(), window, composition)) {
    ActiveState().WhyNot = Says::kUnsupportedTransfer;
    SDL_ReleaseWindowFromGPUDevice(Device_.Get(), window);
    return std::unexpected(ActiveState().WhyNot);
  }
  constexpr std::array modes = {
      SDL_GPU_PRESENTMODE_MAILBOX, SDL_GPU_PRESENTMODE_IMMEDIATE, SDL_GPU_PRESENTMODE_VSYNC};
  ActiveState().WhyNot = Says::kNoPresentMode;
  for (const SDL_GPUPresentMode mode : modes) {
    if (!SDL_WindowSupportsGPUPresentMode(Device_.Get(), window, mode)) { continue; }
    if (SDL_SetGPUSwapchainParameters(Device_.Get(), window, composition, mode)) { return mode; }
    ActiveState().WhyNot = std::string(Says::kPresentModeFailed) + SDL_GetError();
  }
  SDL_ReleaseWindowFromGPUDevice(Device_.Get(), window);
  return std::unexpected(ActiveState().WhyNot);
}

std::expected<void, std::string>
SceneRenderer::DrawsInto(int widthPx, int heightPx, SDL_Window *presents) {
  if (widthPx <= 0 || heightPx <= 0) { return std::unexpected(Says::kInvalidTargetExtent); }
  if (!Stands()) { return std::unexpected(ActiveState().WhyNot); }

  const bool changedWindow = presents != nullptr && Showing_ != presents;
  auto mode = Presenting_;
  if (changedWindow) {
    const auto claimed = ClaimWindow(presents);
    if (!claimed) { return std::unexpected(claimed.error()); }
    mode = *claimed;
  }

  if (ActiveState().Plan != nullptr) {
    const auto rebuilt = InitForTarget(
        {.WidthPx = widthPx, .HeightPx = heightPx}, ActiveState().Plan, presents != nullptr);
    if (!rebuilt) {
      if (changedWindow) { SDL_ReleaseWindowFromGPUDevice(Device_.Get(), presents); }
      return std::unexpected(rebuilt.error());
    }
  } else if (presents == nullptr) {
    auto made = MakeOffscreen(nullptr, {.WidthPx = widthPx, .HeightPx = heightPx});
    if (!made) { return std::unexpected(made.error()); }
    ActiveFrame().Offscreen = std::move(*made);
    ActiveFrame().HostSurface = ActiveFrame().Offscreen.Get();
    ActiveFrame().Width = widthPx;
    ActiveFrame().Height = heightPx;
  }

  if (Showing_ != nullptr && Showing_ != presents) {
    SDL_ReleaseWindowFromGPUDevice(Device_.Get(), Showing_);
  }
  Showing_ = presents;
  Presenting_ = mode;
  ActiveFrame().Shown = {};
  ActiveState().Submitted = false;
  ActiveState().WhyNot.clear();
  return {};
}

std::expected<std::optional<SceneRenderer::Shown>, std::string_view>
SceneRenderer::Presented() const {
  if (Showing_ == nullptr) {
    return std::unexpected(
        "no window is being shown on: a frame is presented to a surface the caller declared, "
        "and `DrawsInto` names one");
  }
  if (ActiveFrame().Shown.WidthPx == 0) { return std::optional<Shown>(); }
  return std::optional<Shown>(ActiveFrame().Shown);
}
}
