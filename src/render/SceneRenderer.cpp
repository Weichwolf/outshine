#include "MediumView.h"
#include "EnvironmentSpecularLayout.h"
#include "Units.h"
#include "Mat4.h"
#include "Vec2.h"
#include "Heap.h"
#include <algorithm>
#include <cassert>
#include <span>
#include <array>
#include <chrono>

#include "SceneRenderer.h"
#include "Vec3.h"

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
#include "visibility/DepthPyramid.h"
#include "stages/SceneTargets.h"

namespace outshine::Render {
namespace {
bool DepthPyramidNeeded(const ExecutionPlan &plan) {
  return !plan.Holds(Stage::TemporalResolve) ||
         std::ranges::contains(plan.Specification().Outputs, Resource::DepthPyramid) ||
         std::ranges::contains(plan.Specification().Content, Stage::DepthPyramid);
}
}

void SceneRenderer::SetContentSelection(const ContentSelection &selection) {
  auto &content = ActiveState().Content;
  content.Geometry().Ground().SetDrawEnabled(selection.HeightLattice);
  content.Transmission().Ground().SetDrawEnabled(selection.HeightLattice);
  content.Geometry().SetPiecesVisible(selection.Instances);
  content.Transmission().SetPiecesVisible(selection.Instances);
}

namespace Says {
constexpr auto kRendererNotReady = "GPU renderer is not initialized";
constexpr auto kCameraNotConfigured = "render camera is not configured";
constexpr auto kInvalidTargetExtent = "render target dimensions must be positive";
}

namespace {

#ifdef OUTSHINE_GPU_VALIDATION
constexpr bool kGpuValidation = true;
#else
constexpr bool kGpuValidation = false;
#endif

struct FrameAcquisition {
  const GpuSubmission &Submission;
  Native::Device *Device = nullptr;
  std::optional<std::chrono::steady_clock::time_point> Previous = std::nullopt;
  std::chrono::steady_clock::duration QueueWait{};

  [[nodiscard]] Native::Result<std::unique_ptr<Native::Commands>> operator()() {
    if (Previous) { QueueWait += std::chrono::steady_clock::now() - *Previous; }
    auto commands = Submission.Acquire(Submission.Context, Device);
    if (!commands && commands.error().Kind == Native::ErrorKind::Busy) {
      Previous = std::chrono::steady_clock::now();
    }
    return commands;
  }
};
}

namespace {

GeometryCullStage::Source CullSource(const GeometryRenderer &draw) {
  return {.Resident = &draw.Resident(),
          .AnchorM = draw.AnchorM(),
          .Jobs = draw.ClusterJobs(),
          .BatchRows = draw.ClusterBatchRows(),
          .Generation = draw.Generation(),
          .GroundGeneration = draw.Ground().Generation()};
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
          .Jitter = ActiveState().History.Sequence().Jitter()};
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
        {.Named = Stage::EnvironmentSpecular,
         .Configure = &SceneRenderer::ConfigureEnvironmentSpecular,
         .Encode = &SceneRenderer::EncodeEnvironmentSpecular},
        {.Named = Stage::Irradiance,
         .Configure = &SceneRenderer::ConfigureIrradiance,
         .Encode = &SceneRenderer::EncodeIrradiance},
        {.Named = Stage::GeometryCull,
         .Configure = &SceneRenderer::ConfigureGeometryCull,
         .Encode = &SceneRenderer::EncodeGeometryCull},
        {.Named = Stage::GeometryScan,
         .Configure = &SceneRenderer::ConfigureGeometryCull,
         .Encode = &SceneRenderer::EncodeGeometryScan},
        {.Named = Stage::GeometryCompact,
         .Configure = &SceneRenderer::ConfigureGeometryCull,
         .Encode = &SceneRenderer::EncodeGeometryCompact},
        {.Named = Stage::LightVisibility,
         .Configure = &SceneRenderer::ConfigureLightVisibility,
         .Encode = &SceneRenderer::EncodeLightVisibility},
        {.Named = Stage::Sky,
         .Configure = &SceneRenderer::ConfigureSky,
         .Encode = &SceneRenderer::EncodeSky},
        {.Named = Stage::OpaqueGeometry,
         .Configure = &SceneRenderer::ConfigureGeometry,
         .Encode = &SceneRenderer::EncodeGeometry},
        {.Named = Stage::GroundPyramid,
         .Configure = &SceneRenderer::ConfigureGroundPyramid,
         .Encode = &SceneRenderer::EncodeGroundPyramid},
        {.Named = Stage::GroundSelection,
         .Configure = &SceneRenderer::ConfigureGroundSelection,
         .Encode = &SceneRenderer::EncodeGroundSelection},
        {.Named = Stage::Ground, .Configure = nullptr, .Encode = &SceneRenderer::EncodeGround},
        {.Named = Stage::TransmissiveGeometry,
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
  auto made =
      Submission_.Create(Submission_.Context,
                         {.Validation = kGpuValidation,
                          .GpuTiming = outshine::Render::Native::GpuTimingMode::Submission});
  if (!made) {
    ActiveState().WhyNot = made.error().Message;
    return false;
  }
  Device_ = std::move(*made);
  Uploads_ = std::make_unique<UploadQueue>(
      *Device_,
      UploadBudget_,
      [](void *context) { static_cast<SceneRenderer *>(context)->CollectCompletedFrameTimes(); },
      this);
  return true;
}

std::expected<std::unique_ptr<Native::Texture>, std::string>
SceneRenderer::MakeOffscreen(const ExecutionPlan *plan, Extent frame) {
  if (plan == nullptr || !plan->Holds(Resource::Surface)) {
    return std::unique_ptr<Native::Texture>{};
  }
  auto made =
      Device_->CreateTexture({.Width = static_cast<uint32_t>(frame.WidthPx),
                              .Height = static_cast<uint32_t>(frame.HeightPx),
                              .Texels = TextureFormatOf(plan->Format(Resource::Surface)),
                              .Use = Native::TextureUse::ColourAttachment |
                                     Native::TextureUse::Sampled | Native::TextureUse::CopySource});
  if (!made) { return std::unexpected(made.error().Message); }
  return std::move(*made);
}

std::expected<void, std::string>
SceneRenderer::StandsOffscreen(FrameResources &frame, const ExecutionPlan *plan, bool presents) {
  if (presents || frame.Offscreen || frame.Width <= 0 || frame.Height <= 0) { return {}; }
  auto texture = MakeOffscreen(plan, {.WidthPx = frame.Width, .HeightPx = frame.Height});
  if (!texture) { return std::unexpected(texture.error()); }
  frame.Offscreen = std::move(*texture);
  frame.HostSurface = frame.Offscreen.get();
  return {};
}

std::expected<void, std::string> SceneRenderer::Init(Extent frame,
                                                     std::shared_ptr<const ExecutionPlan> plan) {
  return InitForTarget(frame, std::move(plan), Showing_ != nullptr);
}

bool SceneRenderer::BeginsWorldCandidate(std::string &error,
                                         SceneResources::PieceSources pieces,
                                         GroundSources ground) {
  if (Candidate_) {
    error = "a renderer already builds a world candidate, so a second candidate cannot borrow its "
            "temporary GPU state";
    return false;
  }
  Candidate_.emplace();
  Candidate_->GroundInputs = ground;
  Candidate_->Content.Scene->Resources().CopySourcesFrom(State_.Content.Scene->Resources(), pieces);
  error.clear();
  return true;
}

bool SceneRenderer::PublishesWorldCandidate(std::string &error, OverlaySource overlay) {
  if (!Candidate_) {
    error = "a renderer cannot publish a world candidate it did not begin";
    return false;
  }
  if (overlay == OverlaySource::Published) { Candidate_->Overlay = std::move(State_.Overlay); }
  if (Candidate_->Frame) { State_.Frame = std::move(*Candidate_->Frame); }
  static_cast<SceneStateCore &>(State_) = std::move(static_cast<SceneStateCore &>(*Candidate_));
  Candidate_.reset();
  ApplyWorldDeclarations();
  BindFrameResources();
  ActiveFrame().Shadow.Invalidate();
  ActiveFrame().Cull.Invalidate();
  BeginTemporalRun();
  error.clear();
  return true;
}

void SceneRenderer::AbandonsWorldCandidate() noexcept {
  Candidate_.reset();
}

std::expected<void, std::string> SceneRenderer::ValidateExecutablePlan(const ExecutionPlan &plan) {
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
                                                                    const ExecutionPlan &plan) {
  if (!Candidate_) { return false; }
  const bool reusesFrame =
      State_.Ready && State_.Plan != nullptr && State_.Frame.Width == frame.WidthPx &&
      State_.Frame.Height == frame.HeightPx && State_.Plan->Specification() == plan.Specification();
  if (!reusesFrame) { return false; }
  const bool reusesGround = Candidate_->GroundInputs == GroundSources::Published;
  Candidate_->Frame.reset();
  ActiveState().Plan = State_.Plan;
  ActiveState().Content.DrawsGlass = plan.Holds(Stage::TransmissiveGeometry);
  if (!Accepted(ActiveState().Content.Geometry().AttachPipelines(State_.Frame.GeometryPipelines,
                                                                 State_.Frame.Handles),
                ActiveState().WhyNot)) {
    return std::unexpected(ActiveState().WhyNot);
  }
  if (reusesGround && !Accepted(ActiveState().Content.Geometry().Ground().ReuseInputsFrom(
                                    State_.Content.Geometry().Ground(), *Device_),
                                ActiveState().WhyNot)) {
    return std::unexpected(ActiveState().WhyNot);
  }
  if (!Accepted(
          ActiveState().Content.Geometry().PrepareTerrainMaterialBuffers(State_.Frame.Handles),
          ActiveState().WhyNot)) {
    return std::unexpected(ActiveState().WhyNot);
  }
  if (!Accepted(ActiveState().Content.Transmission().AttachPipelines(State_.Frame.GlassPipelines,
                                                                     State_.Frame.Handles),
                ActiveState().WhyNot)) {
    return std::unexpected(ActiveState().WhyNot);
  }
  if (reusesGround && !Accepted(ActiveState().Content.Scene->GroundMaterials().ReuseInputsFrom(
                                    State_.Content.Scene->GroundMaterials(), *Device_),
                                ActiveState().WhyNot)) {
    return std::unexpected(ActiveState().WhyNot);
  }
  if (!ActiveState().Content.Scene->GroundMaterials().Ready()) {
    const auto ground = ActiveState().Content.Scene->GroundMaterials().Replace(
        State_.Frame.Handles.Device, {}, {}, std::chrono::seconds(5));
    if (!ground) {
      ActiveState().WhyNot = ground.error().Message;
      return std::unexpected(ActiveState().WhyNot);
    }
  }
  ActiveState().Content.Geometry().GroundFrom(
      {.Classes = ActiveState().Content.Scene->GroundMaterials().Classes(),
       .Palette = ActiveState().Content.Scene->GroundMaterials().Palette()});
  ActiveState().Content.Transmission().GroundFrom(
      {.Classes = ActiveState().Content.Scene->GroundMaterials().Classes(),
       .Palette = ActiveState().Content.Scene->GroundMaterials().Palette()});
  ActiveState().History.InvalidateView();
  ActiveState().Ready = true;
  return true;
}

std::expected<SceneRenderer::FrameResources, std::string>
SceneRenderer::BuildFrameResources(Extent frame, const ExecutionPlan &plan, bool presents) {
  Native::Device *const device = Device_.get();
  FrameResources candidate;
  candidate.Width = frame.WidthPx;
  candidate.Height = frame.HeightPx;
  candidate.Handles.Device = device;
  candidate.Handles.Uploads = Uploads_.get();
  candidate.Handles.HdrFormat = TextureFormatOf(plan.Format(Resource::SceneHdr));
  candidate.Handles.SurfaceFormat = TextureFormatOf(plan.Format(Resource::FrameTex));
  candidate.Handles.Width = candidate.Width;
  candidate.Handles.Height = candidate.Height;

  for (const ExecutionPlan::Pass &pass : plan.Passes()) {
    if (pass.Kind == PassKind::Compute || pass.Depth == kNoEdge) { continue; }
    candidate.Handles.SceneColours = pass.Targets;
    break;
  }
  for (size_t r = 0; r < kResourceCount; ++r) {
    const auto id = static_cast<Resource>(r);
    if (!plan.Holds(id)) { continue; }
    const auto created = Create(candidate, plan, id);
    if (!created) { return std::unexpected(created.error().Message); }
    if (Created(candidate, id)) { continue; }
    ActiveState().WhyNot = std::string("could not create render resource '") + Row(id).Name + "'";
    return std::unexpected(ActiveState().WhyNot);
  }
  if (const auto stood = StandsOffscreen(candidate, &plan, presents); !stood) {
    return std::unexpected(stood.error());
  }
  if (!ConfigurePlanStages(candidate, plan, plan.Holds(Stage::TransmissiveGeometry))) {
    return std::unexpected(ActiveState().WhyNot);
  }
  if (Device_ && !Settle(ActiveState().WhyNot)) { return std::unexpected(ActiveState().WhyNot); }
  return candidate;
}

void SceneRenderer::PublishFrameResources(FrameResources frame,
                                          std::shared_ptr<const ExecutionPlan> plan) {
  ActiveState().Ready = false;
  ActiveState().History.InvalidateView();
  if (Candidate_) {
    Candidate_->Frame.emplace(std::move(frame));
  } else {
    State_.Frame = std::move(frame);
  }
  ActiveState().Plan = std::move(plan);
  ActiveState().Content.DrawsGlass = ActiveState().Plan->Holds(Stage::TransmissiveGeometry);
  if (!Candidate_) { ApplyWorldDeclarations(); }
  BindFrameResources();
  if (!Candidate_) { BeginTemporalRun(); }
  ActiveState().Ready = true;
}

void SceneRenderer::LogReadyPlan(Native::Device *device) const {
  Log::Info(LogTag::Render,
            "device_ready",
            {{"width", ActiveFrame().Width},
             {"height", ActiveFrame().Height},
             {"driver", device->Limits().Name},
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

std::expected<void, std::string> SceneRenderer::InitForTarget(
    Extent frame, std::shared_ptr<const ExecutionPlan> plan, bool presents) {
  const auto offline = OfflineUploads(std::chrono::seconds(5));
  ActiveState().WhyNot.clear();
  if (const auto valid = ValidateExecutablePlan(*plan); !valid) { return valid; }
  if (!Stands()) { return std::unexpected(ActiveState().WhyNot); }
  ActiveState().Content.Scene->GroundMaterials().UploadsFrom(Uploads_.get());
  const auto reused = ReuseFrameResources(frame, *plan);
  if (!reused) { return std::unexpected(reused.error()); }
  if (*reused) { return {}; }
  auto candidate = BuildFrameResources(frame, *plan, presents);
  if (!candidate) { return std::unexpected(candidate.error()); }
  Native::Device *const device = Device_.get();
  PublishFrameResources(std::move(*candidate), std::move(plan));
  LogReadyPlan(device);
  return {};
}

AttachmentSet SceneRenderer::ColoursForStage(const FrameResources &frame,
                                             const ExecutionPlan &plan,
                                             Stage wanted) {
  for (const ExecutionPlan::Pass &pass : plan.Passes()) {
    for (size_t at = pass.First; at < pass.First + pass.Count; ++at) {
      if (plan.Order()[at] == wanted) { return pass.Targets; }
    }
  }
  return frame.Handles.SceneColours;
}

bool SceneRenderer::ConfigurePlanStages(FrameResources &frame,
                                        const ExecutionPlan &plan,
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

SceneRenderer::FrameGraphAllocations SceneRenderer::FrameGraphAllocationCounts() const noexcept {
  FrameGraphAllocations counts;
  const auto count = [&counts](const FrameResources &frame) {
    counts.Textures += frame.Targets.Count();
    const auto atmosphere = frame.Atmosphere.Allocations();
    counts.Textures += atmosphere.Textures;
    counts.Buffers += atmosphere.Buffers;
    counts.Samplers += atmosphere.Samplers;
    for (const auto *texture : {frame.Offscreen.get(), frame.Shadow.Atlas()}) {
      if (texture != nullptr) { ++counts.Textures; }
    }
    for (const auto *buffer : {frame.DepthPyramid.Buffer(), frame.GroundPyramid.Buffer()}) {
      if (buffer != nullptr) { ++counts.Buffers; }
    }
    if (frame.Samp) { ++counts.Samplers; }
  };
  count(State_.Frame);
  if (Candidate_ && Candidate_->Frame) { count(*Candidate_->Frame); }
  return counts;
}

bool SceneRenderer::Created(const FrameResources &frame, Resource resource) {
  if (AtmospherePasses::Owns(resource)) { return frame.Atmosphere.Has(resource); }
  switch (resource) {
    case Resource::LinearSampler: return static_cast<bool>(frame.Samp);
    case Resource::DepthPyramid: return frame.DepthPyramid.Buffer() != nullptr;
    case Resource::GroundPyramid: return frame.GroundPyramid.Buffer() != nullptr;
    case Resource::GroundSelection: return true;
    case Resource::ShadowAtlas: return frame.Shadow.Atlas() != nullptr;
    case Resource::AtmosphereUniform:
    case Resource::CascadeUniform:
    case Resource::VegetationTable:
    case Resource::Meter:
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
    default: return frame.Targets.Get(resource) != nullptr;
  }
}

Native::Texture *SceneRenderer::Target(Resource resource) const {
  return Target(ActiveFrame(), resource, ActiveState().History.Sequence().OutputIndex());
}

Native::Texture *
SceneRenderer::Target(const FrameResources &frame, Resource resource, size_t linearIndex) {
  if (AtmospherePasses::Owns(resource)) { return frame.Atmosphere.Texture(resource); }
  switch (resource) {
    case Resource::OverlayAtlas: return nullptr;

    case Resource::Surface: return frame.Offscreen.get();

    case Resource::ClusterSphere:
    case Resource::ClusterIndex:
    case Resource::ClusterJobs:
    case Resource::ClusterBatches:
    case Resource::ClusterKept:
    case Resource::ClusterSlot:
    case Resource::DrawIndex:
    case Resource::DrawArguments: return nullptr;
    case Resource::ShadowAtlas: return frame.Shadow.Atlas();
    case Resource::LinearSampler:
    case Resource::LutSampler:
    case Resource::AtmosphereUniform:
    case Resource::CascadeUniform:
    case Resource::VegetationTable:
    case Resource::IrradianceBuffer:
    case Resource::Meter:
    case Resource::GroundPyramid:
    case Resource::GroundSelection:
    case Resource::DepthPyramid:
    case Resource::AoBuffer:
    case Resource::kCount: return nullptr;
    default: return frame.Targets.Get(resource, linearIndex);
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

Native::Format SceneRenderer::SurfaceFormat() const {
  if (Showing_ != nullptr && ActiveFrame().HostSurface != nullptr) {
    return ActiveFrame().HostSurface->Description().Texels;
  }
  return ActiveState().Plan ? TextureFormatOf(ActiveState().Plan->Format(Resource::Surface))
                            : Native::Format::Count;
}

Native::Texture *SceneRenderer::DisplaySource() const {
  const auto input = ActiveState().Plan->Holds(Stage::TemporalResolve) ? Resource::SceneAerial
                                                                       : Resource::SceneLinear;
  return Target(ActiveState().Plan->Bound(input));
}

Native::Texture *SceneRenderer::LinearSource() const {
  return Target(ActiveState().Plan->Bound(Resource::SceneLinear));
}

bool SceneRenderer::Configure(Stage stage,
                              FrameResources &frame,
                              const ExecutionPlan &plan,
                              bool drawsGlass,
                              std::string &error) {
  const Executor *seat = ExecutorOf(stage);
  if (seat == nullptr) {
    error = "this device layer does not execute the stage";
    return false;
  }
  if (seat->Configure == nullptr) { return true; }
  return seat->Configure(*this, frame, plan, drawsGlass, error);
}

bool SceneRenderer::PrepareGroundMaterialBuffers(FrameResources &frame, std::string &error) {
  auto &content = ActiveState().Content;
  if (content.Scene->GroundMaterials().Ready()) { return true; }
  const auto uploaded = content.Scene->GroundMaterials().Replace(
      frame.Handles.Device, {}, {}, std::chrono::seconds(5));
  if (!uploaded) {
    error = uploaded.error().Message;
    return false;
  }
  const GeometryRenderer::GroundBuffers buffers{
      .Classes = content.Scene->GroundMaterials().Classes(),
      .Palette = content.Scene->GroundMaterials().Palette()};
  content.Geometry().GroundFrom(buffers);
  content.Transmission().GroundFrom(buffers);
  return true;
}

bool SceneRenderer::ConfigureGeometry(SceneRenderer &renderer,
                                      FrameResources &frame,
                                      const ExecutionPlan &plan,
                                      bool drawsGlass,
                                      std::string &error) {
  (void)plan;
  if (!renderer.PrepareGroundMaterialBuffers(frame, error)) { return false; }
  return Accepted(renderer.ActiveState().Content.Geometry().Configure(
                      frame.GeometryPipelines, frame.Handles, nullptr, nullptr, drawsGlass),
                  error);
}

bool SceneRenderer::ConfigureGlass(SceneRenderer &renderer,
                                   FrameResources &frame,
                                   const ExecutionPlan &plan,
                                   bool drawsGlass,
                                   std::string &error) {
  (void)plan;
  (void)drawsGlass;
  if (!renderer.PrepareGroundMaterialBuffers(frame, error)) { return false; }
  return Accepted(
      renderer.ActiveState().Content.Transmission().Configure(frame.GlassPipelines,
                                                              frame.Handles,
                                                              frame.Targets.Get(Resource::SceneHdr),
                                                              frame.Samp.get(),
                                                              false),
      error);
}

bool SceneRenderer::ConfigureCompositeTransmission(SceneRenderer &renderer,
                                                   FrameResources &frame,
                                                   const ExecutionPlan &plan,
                                                   bool drawsGlass,
                                                   std::string &error) {
  static_cast<void>(renderer);
  (void)drawsGlass;
  return Accepted(frame.CompositeTransmission.Configure(
                      frame.Handles,
                      {.Opaque = frame.Targets.Get(Resource::SceneHdr),
                       .Transmissive = frame.Targets.Get(Resource::SceneTransmissive),
                       .Exact = frame.Samp.get(),
                       .Target = TextureFormatOf(plan.Format(Resource::SceneComposited))}),
                  error);
}

bool SceneRenderer::ConfigureOverlay(SceneRenderer &renderer,
                                     FrameResources &frame,
                                     const ExecutionPlan &plan,
                                     bool drawsGlass,
                                     std::string &error) {
  (void)drawsGlass;
  static_cast<void>(renderer);
  return Accepted(frame.OverlayPipe.Configure(frame.Handles,
                                              frame.Samp.get(),
                                              TextureFormatOf(plan.Format(Resource::FrameTex))),
                  error);
}

bool SceneRenderer::ConfigurePresent(SceneRenderer &renderer,
                                     FrameResources &frame,
                                     const ExecutionPlan &plan,
                                     bool drawsGlass,
                                     std::string &error) {
  static_cast<void>(renderer);
  (void)plan;
  (void)drawsGlass;
  if (!Accepted(frame.Present.Configure(frame.Targets.Get(Resource::FrameTex), frame.Samp.get()),
                error)) {
    return false;
  }
  constexpr std::array formats{Native::Format::Rgba8Unorm,
                               Native::Format::Rgba8Srgb,
                               Native::Format::Bgra8Unorm,
                               Native::Format::Bgra8Srgb};
  for (const auto format : formats) {
    if (!frame.Handles.Device->Supports(format, Native::TextureUse::ColourAttachment, false)) {
      continue;
    }
    if (!Accepted(frame.Present.For(*frame.Handles.Device, format), error)) { return false; }
  }
  return true;
}

bool SceneRenderer::ConfigureTonemap(SceneRenderer &renderer,
                                     FrameResources &frame,
                                     const ExecutionPlan &plan,
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
  return Accepted(
      frame.Tonemap.Configure(frame.Handles,
                              {.Scene = Target(frame, plan.Bound(input), 0),
                               .Depth = frame.Targets.Get(Resource::SceneDepth),
                               .Exact = frame.Samp.get(),
                               .Linear = TextureFormatOf(plan.Format(Resource::SceneLinear))},
                              display),
      error);
}

bool SceneRenderer::ConfigureMediumTransmittance(SceneRenderer &renderer,
                                                 FrameResources &frame,
                                                 const ExecutionPlan &plan,
                                                 bool drawsGlass,
                                                 std::string &error) {
  (void)renderer;
  (void)plan;
  (void)drawsGlass;
  return Accepted(frame.Atmosphere.Configure(Stage::MediumTransmittance, frame.Handles), error);
}

bool SceneRenderer::ConfigureMediumMultiScatter(SceneRenderer &renderer,
                                                FrameResources &frame,
                                                const ExecutionPlan &plan,
                                                bool drawsGlass,
                                                std::string &error) {
  (void)renderer;
  (void)plan;
  (void)drawsGlass;
  return Accepted(frame.Atmosphere.Configure(Stage::MediumMultiScatter, frame.Handles), error);
}

bool SceneRenderer::ConfigureMediumRadiance(SceneRenderer &renderer,
                                            FrameResources &frame,
                                            const ExecutionPlan &plan,
                                            bool drawsGlass,
                                            std::string &error) {
  (void)renderer;
  (void)plan;
  (void)drawsGlass;
  return Accepted(frame.Atmosphere.Configure(Stage::MediumRadiance, frame.Handles), error);
}

bool SceneRenderer::ConfigureSky(SceneRenderer &renderer,
                                 FrameResources &frame,
                                 const ExecutionPlan &plan,
                                 bool drawsGlass,
                                 std::string &error) {
  static_cast<void>(renderer);
  (void)plan;
  (void)drawsGlass;
  return Accepted(
      frame.Sky.Configure(frame.Handles,
                          {.SkyView = frame.Atmosphere.Texture(Resource::SkyViewLut),
                           .Transmittance = frame.Atmosphere.Texture(Resource::TransmittanceLut),
                           .Lut = frame.Atmosphere.Sampler()}),
      error);
}

bool SceneRenderer::ConfigureAerialPerspective(SceneRenderer &renderer,
                                               FrameResources &frame,
                                               const ExecutionPlan &plan,
                                               bool drawsGlass,
                                               std::string &error) {
  static_cast<void>(renderer);
  (void)drawsGlass;
  return Accepted(
      frame.Aerial.Configure(frame.Handles,
                             {.Scene = Target(frame, plan.Bound(Resource::SceneComposited), 0),
                              .Depth = frame.Targets.Get(Resource::SceneDepth),
                              .SkyView = frame.Atmosphere.Texture(Resource::SkyViewLut),
                              .Transmittance = frame.Atmosphere.Texture(Resource::TransmittanceLut),
                              .Exact = frame.Samp.get(),
                              .Lut = frame.Atmosphere.Sampler()},
                             TextureFormatOf(plan.Format(Resource::SceneAerial))),
      error);
}

bool SceneRenderer::ConfigureLightVisibility(SceneRenderer &renderer,
                                             FrameResources &frame,
                                             const ExecutionPlan &plan,
                                             bool drawsGlass,
                                             std::string &error) {
  (void)plan;
  (void)drawsGlass;
  return Accepted(frame.Shadow.Configure(renderer.ActiveState().Content.Geometry(), frame.Handles),
                  error);
}

void SceneRenderer::ApplyWorldDeclarations() {
  ActiveFrame().Atmosphere.Declare(
      {.Air = ActiveState().Medium,
       .CosSunZenith = ActiveState().CosSunZenith,
       .EyeHeightM = ActiveState().EyeHeightM,
       .GroundAlbedo = ActiveState().Content.Geometry().Environment().GroundAlbedo});
  if (ActiveState().SkyDeclared) {
    const SkyParameters parameters = {.SunDir = ActiveState().SkyToSun,
                                      .WorldUp = ActiveState().SkyUp,
                                      .IlluminanceLux = ActiveState().SkyIlluminanceLux,
                                      .EyeHeightM = ActiveState().EyeHeightM};
    ActiveFrame().Sky.Declare(ActiveState().Medium, parameters);
  }
  const GeometryRenderer::Reflections reflections{
      .Atlas = ActiveState().MediumDeclared && ActiveState().SkyDeclared
                   ? ActiveFrame().Atmosphere.Texture(Resource::EnvironmentSpecular)
                   : nullptr,
      .Sampler = ActiveFrame().Atmosphere.Sampler(),
      .Up = ActiveState().SkyUp,
      .ToSun = ActiveState().SkyToSun};
  ActiveState().Content.Geometry().ReflectionsFrom(reflections);
  ActiveState().Content.Transmission().ReflectionsFrom(reflections);
  if (ActiveState().ShadowDeclared) {
    ActiveFrame().Shadow.Declare({.ToSun = ActiveState().ShadowToSun, .Up = ActiveState().ShadowUp},
                                 ActiveState().ShadowRadiusM,
                                 ActiveState().ShadowCameraCentred);
  }
}

void SceneRenderer::BindFrameResources() {
  ActiveState().Content.Geometry().UsePipelines(ActiveFrame().GeometryPipelines);
  ActiveState().Content.Transmission().UsePipelines(ActiveFrame().GlassPipelines);
  ActiveFrame().Shadow.Binds(ActiveState().Content.Geometry());
  ActiveFrame().Cull.Binds(CullSource(ActiveState().Content.Geometry()));
}

PassRecording SceneRenderer::Picture(bool picture, const PassRecording &into) const {
  if (!picture) { return into; }
  auto result = into;
  const Placed rect = PictureRect();
  result.Area = {.X = static_cast<float>(rect.LeftPx),
                 .Y = static_cast<float>(rect.TopPx),
                 .Width = static_cast<float>(rect.WidthPx),
                 .Height = static_cast<float>(rect.HeightPx)};
  return result;
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
  ActiveState().History.ApplyPrevious(ctx);
  return ctx;
}

Native::Result<void>
SceneRenderer::EncodeStage(Stage stage, const FrameContext &ctx, const PassRecording &into) {
  const Executor *seat = ExecutorOf(stage);
  if (seat == nullptr || seat->Encode == nullptr) { return {}; }

  const outshine::Heap::Tagged encoding(Row(stage).Name);
  const auto began = std::chrono::steady_clock::now();
  if constexpr (kGpuValidation) { into.Commands->BeginLabel(Row(stage).Name); }
  auto result = (this->*seat->Encode)(ctx, into);
  if constexpr (kGpuValidation) { into.Commands->EndLabel(); }
  Effort &spent = PendingSpent_[static_cast<size_t>(stage)];
  spent.TookMs +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  spent.Draws = 0;
  spent.DirectTriangles = 0;
  spent.IndirectInputTriangles = 0;
  spent.IndirectRecords = 0;
  spent.Surfaces = 0;
  spent.Placements = 0;
  if (stage == Stage::OpaqueGeometry || stage == Stage::TransmissiveGeometry) {
    const GeometryRenderer &drew = stage == Stage::OpaqueGeometry
                                       ? ActiveState().Content.Geometry()
                                       : ActiveState().Content.Transmission();
    const auto &work = drew.Work();
    spent.Draws = work.DrawRequests;
    spent.DirectTriangles = work.DirectTriangles;
    spent.IndirectInputTriangles = work.IndirectInputTriangles;
    spent.IndirectRecords = work.IndirectRecords;
    spent.Surfaces = work.SurfaceSlotLimit;
    spent.Placements = work.PlacementSlotLimit;
    spent.Textured = drew.Textured();
    spent.Palettes = drew.ColourImages();
    spent.DeviceBytes = drew.HeldBytes();
    spent.Layouts = drew.Layouts();
  }
  return result;
}

Native::Result<void> SceneRenderer::EncodeGeometry(const FrameContext &ctx,
                                                   const PassRecording &into) {
  return ActiveState().Content.Geometry().Encode(ctx, Picture(true, into));
}

Native::Result<void> SceneRenderer::EncodeGlass(const FrameContext &ctx,
                                                const PassRecording &into) {
  return ActiveState().Content.Transmission().Encode(ctx, Picture(true, into));
}

Native::Result<void> SceneRenderer::EncodeCompositeTransmission(const FrameContext &ctx,
                                                                const PassRecording &into) {
  return ActiveFrame().CompositeTransmission.Encode(ctx, Picture(true, into));
}

Native::Result<void> SceneRenderer::EncodeTonemap(const FrameContext &ctx,
                                                  const PassRecording &into) {
  ActiveFrame().Tonemap.Bind(DisplaySource());
  ActiveFrame().Tonemap.BindTemporal(
      {.History = ActiveFrame().Targets.Get(Resource::SceneLinear,
                                            ActiveState().History.Sequence().HistoryIndex()),
       .Velocity = ActiveFrame().Targets.Get(Resource::SceneVelocity)},
      Extent{.WidthPx = ActiveFrame().Width, .HeightPx = ActiveFrame().Height},
      ActiveState().History.Sequence().JitterDelta(),
      ActiveState().History.Sequence().HasHistory());
  return ActiveFrame().Tonemap.Encode(ctx, Picture(true, into));
}

Native::Result<void> SceneRenderer::EncodeOverlay(const FrameContext &ctx,
                                                  const PassRecording &into) {
  ActiveFrame().OverlayPipe.Bind(
      Extent{.WidthPx = ActiveFrame().Width, .HeightPx = ActiveFrame().Height});
  return ActiveFrame().OverlayPipe.Encode(ActiveState().Overlay, ctx, Picture(false, into));
}

Native::Result<void> SceneRenderer::EncodePresent([[maybe_unused]] const FrameContext &ctx,
                                                  const PassRecording &into) {
  auto configured = ActiveFrame().Present.For(*Device_, SurfaceFormat());
  if (!configured) { return configured; }
  return ActiveFrame().Present.Encode(Picture(false, into));
}

Native::Result<void> SceneRenderer::EncodeMediumTransmittance(const FrameContext &ctx,
                                                              const PassRecording &into) {
  (void)ctx;
  return ActiveFrame().Atmosphere.Encode(Stage::MediumTransmittance, into);
}

Native::Result<void> SceneRenderer::EncodeMediumMultiScatter(const FrameContext &ctx,
                                                             const PassRecording &into) {
  (void)ctx;
  return ActiveFrame().Atmosphere.Encode(Stage::MediumMultiScatter, into);
}

Native::Result<void> SceneRenderer::EncodeMediumRadiance(const FrameContext &ctx,
                                                         const PassRecording &into) {
  (void)ctx;
  return ActiveFrame().Atmosphere.Encode(Stage::MediumRadiance, into);
}

bool SceneRenderer::SetGroundClasses(std::span<const uint32_t> classes,
                                     std::span<const float> palette,
                                     std::string &error,
                                     GroundClassUploadMetrics *metrics) {
  if (!UploadGroundClasses(classes, palette, error, metrics)) { return false; }
  const auto copyAt = std::chrono::steady_clock::now();
  ActiveState().Content.Scene->Resources().SetGroundClassification(classes, palette);
  if (metrics != nullptr) {
    metrics->RestoreSourceMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - copyAt)
            .count();
  }
  return true;
}

bool SceneRenderer::SetGroundClasses(GroundClassificationSource source,
                                     std::string &error,
                                     GroundClassUploadMetrics *metrics) {
  if ((source.ClassWords != 0 && !source.Classes) ||
      (source.PaletteFloats != 0 && !source.Palette)) {
    error = "ground classification source has no owner for its bytes";
    return false;
  }
  if (!UploadGroundClasses(source.ClassSpan(), source.PaletteSpan(), error, metrics)) {
    return false;
  }
  const auto retainAt = std::chrono::steady_clock::now();
  ActiveState().Content.Scene->Resources().SetGroundClassification(std::move(source));
  if (metrics != nullptr) {
    metrics->RestoreSourceMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - retainAt)
            .count();
  }
  return true;
}

bool SceneRenderer::BeginGroundClasses(GroundClassificationSource source,
                                       std::string &error,
                                       GroundClassUploadMetrics *metrics) {
  if (metrics != nullptr) { *metrics = {}; }
  WorldContent &content = ActiveState().Content;
  if (content.PendingGroundClasses || (source.ClassWords != 0 && !source.Classes) ||
      (source.PaletteFloats != 0 && !source.Palette)) {
    error = "ground classification upload has an active or unowned source";
    return false;
  }
  content.PendingGroundClasses.emplace(std::move(source));
  auto begun = content.Scene->GroundMaterials().BeginReplace(
      ActiveFrame().Handles.Device,
      content.PendingGroundClasses->ClassSpan(),
      content.PendingGroundClasses->PaletteSpan(),
      metrics != nullptr ? &metrics->Storage : nullptr);
  if (!begun) {
    error = std::move(begun.error().Message);
    content.PendingGroundClasses.reset();
    return false;
  }
  return true;
}

std::expected<bool, std::string>
SceneRenderer::AdvanceGroundClasses(size_t bytesMost, GroundClassUploadMetrics *metrics) {
  if (metrics != nullptr) { *metrics = {}; }
  WorldContent &content = ActiveState().Content;
  if (!content.PendingGroundClasses) {
    return std::unexpected("ground classification upload has no source");
  }
  auto advanced = content.Scene->GroundMaterials().AdvanceReplace(
      bytesMost, metrics != nullptr ? &metrics->Storage : nullptr);
  if (!advanced) {
    content.PendingGroundClasses.reset();
    return std::unexpected(std::move(advanced.error().Message));
  }
  if (!*advanced) { return false; }
  content.Geometry().GroundFrom({.Classes = content.Scene->GroundMaterials().Classes(),
                                 .Palette = content.Scene->GroundMaterials().Palette()});
  content.Transmission().GroundFrom({.Classes = content.Scene->GroundMaterials().Classes(),
                                     .Palette = content.Scene->GroundMaterials().Palette()});
  const auto retainAt = std::chrono::steady_clock::now();
  content.Scene->Resources().SetGroundClassification(std::move(*content.PendingGroundClasses));
  content.PendingGroundClasses.reset();
  if (metrics != nullptr) {
    metrics->RestoreSourceMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - retainAt)
            .count();
  }
  return true;
}

bool SceneRenderer::UploadGroundClasses(std::span<const uint32_t> classes,
                                        std::span<const float> palette,
                                        std::string &error,
                                        GroundClassUploadMetrics *metrics) {
  if (metrics != nullptr) { *metrics = {}; }
  const auto uploaded = ActiveState().Content.Scene->GroundMaterials().Replace(
      ActiveFrame().Handles.Device,
      classes,
      palette,
      std::chrono::seconds(5),
      metrics != nullptr ? &metrics->Storage : nullptr);
  if (!uploaded) {
    error = uploaded.error().Message;
    return false;
  }
  ActiveState().Content.Geometry().GroundFrom(
      {.Classes = ActiveState().Content.Scene->GroundMaterials().Classes(),
       .Palette = ActiveState().Content.Scene->GroundMaterials().Palette()});
  ActiveState().Content.Transmission().GroundFrom(
      {.Classes = ActiveState().Content.Scene->GroundMaterials().Classes(),
       .Palette = ActiveState().Content.Scene->GroundMaterials().Palette()});
  return true;
}

bool SceneRenderer::ConfigureEnvironmentSpecular(SceneRenderer &renderer,
                                                 FrameResources &frame,
                                                 const ExecutionPlan &plan,
                                                 bool drawsGlass,
                                                 std::string &error) {
  (void)renderer;
  (void)plan;
  (void)drawsGlass;
  return Accepted(frame.Atmosphere.Configure(Stage::EnvironmentSpecular, frame.Handles), error);
}

Native::Result<void> SceneRenderer::EncodeEnvironmentSpecular(const FrameContext &ctx,
                                                              const PassRecording &into) {
  (void)ctx;
  return ActiveFrame().Atmosphere.Encode(Stage::EnvironmentSpecular, into);
}

bool SceneRenderer::ConfigureIrradiance(SceneRenderer &renderer,
                                        FrameResources &frame,
                                        const ExecutionPlan &plan,
                                        bool drawsGlass,
                                        std::string &error) {
  (void)renderer;
  (void)plan;
  (void)drawsGlass;
  return Accepted(frame.Atmosphere.Configure(Stage::Irradiance, frame.Handles), error);
}

Native::Result<void> SceneRenderer::EncodeIrradiance(const FrameContext &ctx,
                                                     const PassRecording &into) {
  (void)ctx;
  return ActiveFrame().Atmosphere.Encode(Stage::Irradiance, into);
}

bool SceneRenderer::ConfigureDepthPyramid(SceneRenderer &renderer,
                                          FrameResources &frame,
                                          const ExecutionPlan &plan,
                                          bool drawsGlass,
                                          std::string &error) {
  static_cast<void>(renderer);
  (void)plan;
  (void)drawsGlass;
  return Accepted(
      frame.DepthPyramid.Configure(frame.Targets.Get(Resource::SceneDepth), frame.Samp.get()),
      error);
}

Native::Result<void> SceneRenderer::EncodeDepthPyramid(const FrameContext &ctx,
                                                       const PassRecording &into) {
  (void)ctx;
  if (!DepthPyramidNeeded(*ActiveState().Plan)) { return {}; }
  return ActiveFrame().DepthPyramid.Encode(into);
}

bool SceneRenderer::ConfigureGeometryCull(SceneRenderer &renderer,
                                          FrameResources &frame,
                                          const ExecutionPlan &plan,
                                          bool drawsGlass,
                                          std::string &error) {
  (void)plan;
  (void)drawsGlass;
  frame.Cull.PyramidFrom(frame.DepthPyramid.Buffer(), frame.DepthPyramid.Shape());
  return Accepted(
      frame.Cull.Configure(CullSource(renderer.ActiveState().Content.Geometry()), frame.Handles),
      error);
}

Native::Result<void> SceneRenderer::EncodeGeometryCull(const FrameContext &ctx,
                                                       const PassRecording &into) {
  ActiveFrame().Cull.Binds(CullSource(ActiveState().Content.Geometry()));
  ActiveFrame().Cull.Projects(static_cast<float>(ActiveFrame().Height));
  auto lens = Through();
  const bool temporal = ActiveState().Plan->Holds(Stage::TemporalResolve);
  lens.Jitter = {};
  const GeometryCullStage::SelectionProjection projection{
      .Clip = temporal ? lens.ViewProjection(ActiveState().Camera) : ctx.Mvp,
      .GuardNdc = temporal ? Vec2f{{1.0f / static_cast<float>(lens.WidePx),
                                    1.0f / static_cast<float>(lens.HighPx)}}
                           : Vec2f{}};
  return ActiveFrame().Cull.EncodeCull(ctx, into, projection);
}

Native::Result<void> SceneRenderer::EncodeGeometryScan([[maybe_unused]] const FrameContext &ctx,
                                                       const PassRecording &into) {
  return ActiveFrame().Cull.EncodeScan(into);
}

Native::Result<void> SceneRenderer::EncodeGeometryCompact([[maybe_unused]] const FrameContext &ctx,
                                                          const PassRecording &into) {
  return ActiveFrame().Cull.EncodeCompact(into);
}

Native::Result<void> SceneRenderer::EncodeLightVisibility(const FrameContext &ctx,
                                                          const PassRecording &into) {
  auto encoded = ActiveFrame().Shadow.Encode(ctx, into);
  if (!encoded) { return encoded; }
  Spent_[static_cast<size_t>(Stage::LightVisibility)].DeviceBytes =
      static_cast<uint64_t>(kShadowAtlasPx) * kShadowAtlasPx * sizeof(float);
  ActiveState().Content.Geometry().ShadowedBy(ActiveFrame().Shadow.Atlas(),
                                              ActiveFrame().Atmosphere.Sampler(),
                                              ActiveState().ShadowLightIndex,
                                              ActiveFrame().Shadow.LightFromWorld(),
                                              ActiveFrame().Shadow.Regions(),
                                              ActiveFrame().Shadow.RegionCount());
  return {};
}

bool SceneRenderer::Settle(std::string &error) {
  const auto offline = OfflineUploads(std::chrono::seconds(5));
  if (!Device_) {
    error = Says::kRendererNotReady;
    return false;
  }
  auto commands =
      Uploads_->Begin([this] { return Submission_.Acquire(Submission_.Context, Device_.get()); });
  if (!commands) {
    error = commands.error().Message;
    return false;
  }
  auto completion = Submission_.Submit(Submission_.Context, Device_.get(), std::move(*commands));
  if (!completion) {
    error = completion.error().Message;
    return false;
  }
  Uploads_->Submitted(*completion);
  const auto status = Submission_.WaitFence(
      Submission_.Context, Device_.get(), *completion, std::chrono::seconds(5));
  if (!status) {
    error = status.error().Message;
    return false;
  }
  if (*status != Native::CompletionStatus::Ready) {
    error = "GPU completion exceeds the explicit settle budget";
    return false;
  }
  CollectCompletedFrameTimes();
  error.clear();
  return true;
}

void SceneRenderer::SettleShadow() {
  ActiveFrame().Shadow.Prepare(Framing());
  Touched_[static_cast<size_t>(Resource::ShadowAtlas)] =
      ActiveFrame().Shadow.Cached() || ActiveFrame().Shadow.PreservesAtlas();
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

Native::Result<void>
SceneRenderer::EncodeAerialPerspective([[maybe_unused]] const FrameContext &ctx,
                                       const PassRecording &into) {
  if (!ActiveState().SkyDeclared) { return {}; }
  const PassRecording picture = Picture(true, into);
  const SkyParameters sky{.SunDir = ActiveState().SkyToSun,
                          .WorldUp = ActiveState().SkyUp,
                          .IlluminanceLux = ActiveState().SkyIlluminanceLux,
                          .EyeHeightM = ActiveState().EyeHeightM};
  const auto view = MakeMediumViewParameters(
      Through(), picture.Area, ActiveState().Camera, sky, ActiveState().Medium);
  if (!view) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "aerial medium camera frame is invalid"});
  }
  return ActiveFrame().Aerial.Encode(*view, picture);
}

Native::Result<void> SceneRenderer::EncodeSky(const FrameContext &ctx, const PassRecording &into) {
  ActiveFrame().Sky.SetBasis(Eye());
  return ActiveFrame().Sky.Encode(ctx, Picture(true, into));
}

Native::Result<void> SceneRenderer::EncodePass(Native::Commands *commands,
                                               size_t pass,
                                               const FrameContext &ctx,
                                               StageSubmission &submission) {
  const auto &declared = ActiveState().Plan->Passes()[pass];
  if (declared.Count == 1 &&
      ActiveState().Plan->Order()[declared.First] == Stage::LightVisibility &&
      ActiveFrame().Shadow.Cached()) {
    return EncodeStage(Stage::LightVisibility,
                       ctx,
                       {.Commands = commands, .Submission = submission, .Area = {}, .Clip = {}});
  }
  return declared.Kind == PassKind::Compute
             ? EncodeComputePass(commands, declared, ctx, submission)
             : EncodeGraphicsPass(commands, declared, ctx, submission);
}

Native::Result<void> SceneRenderer::EncodeComputePass(Native::Commands *commands,
                                                      const ExecutionPlan::Pass &declared,
                                                      const FrameContext &ctx,
                                                      StageSubmission &submission) {
  const PassRecording into{.Commands = commands, .Submission = submission, .Area = {}, .Clip = {}};
  for (size_t at = 0; at < declared.Count; ++at) {
    auto encoded = EncodeStage(ActiveState().Plan->Order()[declared.First + at], ctx, into);
    if (!encoded) { return encoded; }
  }
  return {};
}

Native::Result<void> SceneRenderer::EncodeGraphicsPass(Native::Commands *commands,
                                                       const ExecutionPlan::Pass &declared,
                                                       const FrameContext &ctx,
                                                       StageSubmission &submission) {
  std::array<Native::ColourAttachment, kMaxColourAttachments> colours{};
  size_t count = 0;
  for (const auto wanted : declared.Targets) {
    auto &colour = colours[count++];
    colour.Image.Image = wanted == Resource::Surface ? ActiveFrame().HostSurface : Target(wanted);
    colour.Begin = Touched_[static_cast<size_t>(wanted)] ? Native::Load::Load : Native::Load::Clear;
    Touched_[static_cast<size_t>(wanted)] = true;
    colour.End = ActiveState().Plan->Stored(wanted) ? Native::Store::Store : Native::Store::Discard;
    const bool coverage = wanted == Resource::SceneHdr || wanted == Resource::SceneComposited ||
                          wanted == Resource::SceneTransmissive || wanted == Resource::SceneLinear;
    colour.Clear = wanted == Resource::SceneVelocity
                       ? std::array{kVelocityStatic, kVelocityStatic, 0.0f, 0.0f}
                       : std::array{0.0f, 0.0f, 0.0f, coverage ? 0.0f : 1.0f};
  }
  std::optional<Native::DepthAttachment> depth;
  if (declared.Depth != kNoEdge) {
    depth.emplace(Native::DepthAttachment{
        .Image = {.Image = Target(declared.Depth)},
        .Begin = Touched_[static_cast<size_t>(declared.Depth)] ? Native::Load::Load
                                                               : Native::Load::Clear,
        .End = ActiveState().Plan->Stored(declared.Depth) ? Native::Store::Store
                                                          : Native::Store::Discard});
    Touched_[static_cast<size_t>(declared.Depth)] = true;
  }
  Native::TextureView view;
  if (depth) { view = depth->Image; }
  if (count > 0) { view = colours.front().Image; }
  if (view.Image == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "raster pass has no target extent"});
  }
  const auto &image = view.Image->Description();
  const uint32_t width = std::max(1u, image.Width >> view.FirstMip);
  const uint32_t height = std::max(1u, image.Height >> view.FirstMip);
  auto begun = commands->BeginRenderPass(
      {.Colours = std::span(colours).first(count), .Depth = depth, .Label = declared.Name});
  if (!begun) { return begun; }
  const PassRecording into{
      .Commands = commands,
      .Submission = submission,
      .Area = {.Width = static_cast<float>(width), .Height = static_cast<float>(height)},
      .Clip = {.Width = width, .Height = height}};
  for (size_t at = 0; at < declared.Count; ++at) {
    auto encoded = EncodeStage(ActiveState().Plan->Order()[declared.First + at], ctx, into);
    if (!encoded) { return encoded; }
  }
  return commands->EndRenderPass();
}

void SceneRenderer::BeginTemporalRun() {
  ActiveState().History.ResetAccumulation();
}

std::expected<void, std::string> SceneRenderer::PrepareFrame() {
  if (!ActiveState().Ready) {
    return std::unexpected(ActiveState().WhyNot.empty() ? Says::kRendererNotReady
                                                        : ActiveState().WhyNot);
  }
  if (!ActiveState().CameraFull) { return std::unexpected(Says::kCameraNotConfigured); }

  ActiveState().Content.Geometry().CastsNoShadow();
  for (bool &touched : Touched_) { touched = false; }
  {
    std::string why;
    if (!ActiveState().Content.Geometry().HandTables(why) ||
        !ActiveState().Content.Geometry().HandPlacements(true, why) ||
        (ActiveState().Content.DrawsGlass &&
         !ActiveState().Content.Transmission().HandTables(why))) {
      return std::unexpected(std::move(why));
    }
  }
  SettleShadow();
  return {};
}

Native::Result<void> SceneRenderer::FlushCrossings(Native::Commands &commands) {
  return ActiveState().Content.Scene->Meshes().FlushCrossings(&commands);
}

std::expected<Native::Completion, std::string> SceneRenderer::PrepareWorldResources() {
  const auto published = PublishedWorld();
  if (const auto prepared = PrepareFrame(); !prepared) { return std::unexpected(prepared.error()); }
  auto commands =
      Uploads_->Begin([this] { return Submission_.Acquire(Submission_.Context, Device_.get()); });
  if (!commands) { return std::unexpected(commands.error().Message); }
  const auto flushed = FlushCrossings(**commands);
  if (!flushed) { return std::unexpected(flushed.error().Message); }
  const FrameContext ctx = Framing();
  StageSubmission preparation;
  const auto world = [](Stage stage) { return Row(stage).Phase == StagePhase::WorldPreparation; };
  for (size_t pass = 0; pass < ActiveState().Plan->Passes().size(); ++pass) {
    const auto &declared = ActiveState().Plan->Passes()[pass];
    const auto stages =
        std::span(ActiveState().Plan->Order()).subspan(declared.First, declared.Count);
    if (!std::ranges::any_of(stages, world)) { continue; }
    if (!std::ranges::all_of(stages, world)) {
      return std::unexpected("world resource preparation shares a pass with frame-only work");
    }
    auto encoded = EncodePass(commands->get(), pass, ctx, preparation);
    if (!encoded) { return std::unexpected(encoded.error().Message); }
  }
  auto completion = Submission_.Submit(Submission_.Context, Device_.get(), std::move(*commands));
  if (!completion) { return std::unexpected(completion.error().Message); }
  ActiveState().Content.Scene->Meshes().CommitCrossings();
  preparation.Commit();
  Uploads_->Submitted(*completion);
  return *completion;
}

bool SceneRenderer::WorldResourcesComplete(const Native::Completion &completion) {
  if (!Device_) { return false; }
  const auto status = Submission_.QueryFence(Submission_.Context, Device_.get(), completion);
  if (!status || *status != Native::CompletionStatus::Ready) { return false; }
  ActiveState().Content.Scene->Meshes().ReleaseUploadScratch();
  return true;
}

std::expected<void, std::string> SceneRenderer::RenderFrame() {
  const auto published = PublishedWorld();
  return RenderPublishedFrame();
}

void SceneRenderer::CommitSubmittedFrame(StageSubmission &stageSubmission,
                                         const CameraBasis &camera) {
  ActiveState().Content.Scene->Meshes().CommitCrossings();
  stageSubmission.Commit();
  ActiveFrame().Cull.DepthSubmitted(ActiveState().Plan->Holds(Stage::DepthPyramid) &&
                                    DepthPyramidNeeded(*ActiveState().Plan));
  ActiveState().Content.Geometry().CarryFrame();
  ActiveState().Content.Transmission().CarryFrame();

  LastSubmittedCamera_.Basis = camera;
  LastSubmittedCamera_.GeometryAnchorM = ActiveState().Content.Geometry().AnchorM();
  ++LastSubmittedCamera_.Serial;
}

std::expected<void, std::string> SceneRenderer::RenderPublishedFrame() {
  const auto nonblocking = OfflineUploads(std::chrono::nanoseconds::zero());
  const auto began = std::chrono::steady_clock::now();
  auto phaseBegan = began;
  RenderFrameTiming timing;
  const auto record = [&timing, &phaseBegan](RenderFramePhase phase) {
    const auto now = std::chrono::steady_clock::now();
    timing.PhaseMs[static_cast<size_t>(phase)] =
        std::chrono::duration<double, std::milli>(now - phaseBegan).count();
    phaseBegan = now;
  };
  const auto slot = WaitForFrameSlot();
  if (!slot) { return std::unexpected(slot.error().Message); }
  record(RenderFramePhase::FenceWait);
  auto prepared = PrepareFrame();
  if (!prepared) { return prepared; }
  record(RenderFramePhase::Prepare);
  FrameAcquisition acquire{.Submission = Submission_, .Device = Device_.get()};
  auto commands = [&] {
    const auto waiting = OfflineUploads(std::chrono::seconds(5));
    return Uploads_->Begin([&acquire] { return acquire(); });
  }();
  if (!commands) { return std::unexpected(commands.error().Message); }
  timing.PhaseMs[static_cast<size_t>(RenderFramePhase::FenceWait)] +=
      std::chrono::duration<double, std::milli>(acquire.QueueWait).count();
  phaseBegan += acquire.QueueWait;
  record(RenderFramePhase::Acquire);
  const auto uploaded = FlushCrossings(**commands);
  if (!uploaded) { return std::unexpected(uploaded.error().Message); }
  record(RenderFramePhase::Upload);
  std::unique_ptr<Native::PresentFrame> presenting;
  if (Showing_ != nullptr) {
    auto acquired = Device_->AcquirePresentFrame();
    if (!acquired) {
      if (acquired.error().Kind == Native::ErrorKind::Busy) { return {}; }
      return std::unexpected(acquired.error().Message);
    }
    presenting = std::move(*acquired);
    if (!presenting) { return {}; }
    ActiveFrame().HostSurface = &presenting->Image();
    const auto &image = presenting->Image().Description();
    ActiveFrame().Shown = {.WidthPx = static_cast<int>(image.Width),
                           .HeightPx = static_cast<int>(image.Height)};
  }
  record(RenderFramePhase::Swapchain);

  auto temporal =
      ActiveState().History.BeginFrame(ActiveState().Plan->Holds(Stage::TemporalResolve));
  const CameraBasis camera = ActiveState().Camera;
  const FrameContext ctx = Framing();
  StageSubmission stageSubmission;
  const auto restorePresentSurface = [&] {
    if (presenting) { ActiveFrame().HostSurface = ActiveFrame().Offscreen.get(); }
  };

  auto culled = ActiveState().Content.Geometry().Ground().Cull(
      ctx, ActiveState().Content.Geometry().AnchorM(), **commands);
  if (!culled) {
    restorePresentSurface();
    return std::unexpected(culled.error().Message);
  }
  record(RenderFramePhase::Cull);
  PendingSpent_ = {};
  for (size_t pass = 0; pass < ActiveState().Plan->Passes().size(); ++pass) {
    auto encoded = EncodePass(commands->get(), pass, ctx, stageSubmission);
    if (!encoded) {
      restorePresentSurface();
      return std::unexpected(encoded.error().Message);
    }
  }
  record(RenderFramePhase::Encode);
  if (presenting) {
    auto queued = (*commands)->Present(std::move(presenting));
    ActiveFrame().HostSurface = ActiveFrame().Offscreen.get();
    if (!queued) {
      restorePresentSurface();
      return std::unexpected(queued.error().Message);
    }
  }
  auto submitted = Submission_.Submit(Submission_.Context, Device_.get(), std::move(*commands));
  if (!submitted) {
    restorePresentSurface();
    return std::unexpected(submitted.error().Message);
  }
  temporal.Commit(ctx);
  Frames_.Submitted(*submitted);
  Uploads_->Submitted(*submitted);
  record(RenderFramePhase::Submit);
  CommitSubmittedFrame(stageSubmission, camera);
  record(RenderFramePhase::Finish);
  timing.TotalMs = std::chrono::duration<double, std::milli>(phaseBegan - began).count();
  CommitCostProfile(timing);
  LastRenderFrameTiming_ = timing;
  if (timing.TotalMs > SlowestRenderFrameTiming_.TotalMs) { SlowestRenderFrameTiming_ = timing; }
  return {};
}

void SceneRenderer::StopShowing() {
  if (Showing_ != nullptr && Device_) {
    auto stopped = Device_->SetPresentTarget(nullptr);
    if (!stopped) {
      Log::Error(LogTag::Render, "present_target_failed", {{"msg", stopped.error().Message}});
      return;
    }
  }
  ActiveFrame().HostSurface = nullptr;
  ActiveFrame().Shown = {};
  ActiveState().History.InvalidateView();
  Showing_ = nullptr;
}

std::expected<bool, std::string> SceneRenderer::ClaimWindow(SDL_Window *window) {
  auto changed = Device_->SetPresentTarget(window);
  if (!changed) { return std::unexpected(changed.error().Message); }
  return true;
}

std::expected<void, std::string>
SceneRenderer::DrawsInto(int widthPx, int heightPx, SDL_Window *presents) {
  if (widthPx <= 0 || heightPx <= 0) { return std::unexpected(Says::kInvalidTargetExtent); }
  if (!Stands()) { return std::unexpected(ActiveState().WhyNot); }
  ActiveState().Content.Scene->GroundMaterials().UploadsFrom(Uploads_.get());
  const bool changed = Showing_ != presents;
  if (changed) {
    auto claimed = ClaimWindow(presents);
    if (!claimed) { return std::unexpected(claimed.error()); }
  }
  if (ActiveState().Plan) {
    auto rebuilt = InitForTarget(
        {.WidthPx = widthPx, .HeightPx = heightPx}, ActiveState().Plan, presents != nullptr);
    if (!rebuilt) {
      if (changed) {
        auto restored = Device_->SetPresentTarget(Showing_);
        if (!restored) {
          return std::unexpected(rebuilt.error() +
                                 "; target rollback failed: " + restored.error().Message);
        }
      }
      return std::unexpected(rebuilt.error());
    }
  } else {
    ActiveFrame().Width = widthPx;
    ActiveFrame().Height = heightPx;
  }
  Showing_ = presents;
  Presenting_ = true;
  ActiveFrame().HostSurface = ActiveFrame().Offscreen.get();
  ActiveFrame().Shown = {};
  ActiveState().History.InvalidateView();
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
