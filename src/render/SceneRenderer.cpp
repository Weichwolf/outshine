#include "MediumView.h"
#include "EnvironmentSpecularLayout.h"
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

void SceneRenderer::SetContentSelection(const ContentSelection &selection) {
  auto &content = ActiveState().Content;
  content.Geometry.Ground().SetDrawEnabled(selection.HeightLattice);
  content.Glass.Ground().SetDrawEnabled(selection.HeightLattice);
  content.Geometry.SetPiecesVisible(selection.Instances);
  content.Glass.SetPiecesVisible(selection.Instances);
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
        {.Named = Stage::EnvironmentSpecular,
         .Configure = &SceneRenderer::ConfigureEnvironmentSpecular,
         .Encode = &SceneRenderer::EncodeEnvironmentSpecular},
        {.Named = Stage::Irradiance,
         .Configure = &SceneRenderer::ConfigureIrradiance,
         .Encode = &SceneRenderer::EncodeIrradiance},
        {.Named = Stage::SubjectCull,
         .Configure = &SceneRenderer::ConfigureGeometryCull,
         .Encode = &SceneRenderer::EncodeGeometryCull},
        {.Named = Stage::SubjectScan,
         .Configure = &SceneRenderer::ConfigureGeometryCull,
         .Encode = &SceneRenderer::EncodeGeometryScan},
        {.Named = Stage::SubjectCompact,
         .Configure = &SceneRenderer::ConfigureGeometryCull,
         .Encode = &SceneRenderer::EncodeGeometryCompact},
        {.Named = Stage::LightVisibility,
         .Configure = &SceneRenderer::ConfigureLightVisibility,
         .Encode = &SceneRenderer::EncodeLightVisibility},
        {.Named = Stage::Sky,
         .Configure = &SceneRenderer::ConfigureSky,
         .Encode = &SceneRenderer::EncodeSky},
        {.Named = Stage::Geometry,
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
      Submission_.Create(Submission_.Context, {.Validation = kGpuValidation, .GpuTiming = true});
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
SceneRenderer::MakeOffscreen(const Compiled *plan, Extent frame) {
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
SceneRenderer::StandsOffscreen(FrameResources &frame, const Compiled *plan, bool presents) {
  if (presents || frame.Offscreen || frame.Width <= 0 || frame.Height <= 0) { return {}; }
  auto texture = MakeOffscreen(plan, {.WidthPx = frame.Width, .HeightPx = frame.Height});
  if (!texture) { return std::unexpected(texture.error()); }
  frame.Offscreen = std::move(*texture);
  frame.HostSurface = frame.Offscreen.get();
  return {};
}

std::expected<void, std::string> SceneRenderer::Init(Extent frame,
                                                     std::shared_ptr<const Compiled> plan) {
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
  Candidate_->Content.Resources.CopySourcesFrom(State_.Content.Resources, pieces);
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
  ActiveFrame().Shadow.Invalidate();
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
  if (!Candidate_) { return false; }
  const bool reusesFrame =
      State_.Ready && State_.Plan != nullptr && State_.Frame.Width == frame.WidthPx &&
      State_.Frame.Height == frame.HeightPx && State_.Plan->Specification() == plan.Specification();
  if (!reusesFrame) { return false; }
  const bool reusesGround = Candidate_->GroundInputs == GroundSources::Published;
  Candidate_->Frame.reset();
  ActiveState().Plan = State_.Plan;
  ActiveState().Content.DrawsGlass = plan.Holds(Stage::TransmissiveGeometry);
  if (!Accepted(ActiveState().Content.Geometry.AttachPipelines(State_.Frame.GeometryPipelines,
                                                               State_.Frame.Handles),
                ActiveState().WhyNot)) {
    return std::unexpected(ActiveState().WhyNot);
  }
  if (reusesGround && !Accepted(ActiveState().Content.Geometry.Ground().ReuseInputsFrom(
                                    State_.Content.Geometry.Ground(), *Device_),
                                ActiveState().WhyNot)) {
    return std::unexpected(ActiveState().WhyNot);
  }
  if (!Accepted(ActiveState().Content.Geometry.PrepareTerrainMaterialBuffers(State_.Frame.Handles),
                ActiveState().WhyNot)) {
    return std::unexpected(ActiveState().WhyNot);
  }
  if (!Accepted(ActiveState().Content.Glass.AttachPipelines(State_.Frame.GlassPipelines,
                                                            State_.Frame.Handles),
                ActiveState().WhyNot)) {
    return std::unexpected(ActiveState().WhyNot);
  }
  ActiveState().Content.Glass.Shares(ActiveState().Content.Geometry.Owned());
  if (reusesGround &&
      !Accepted(ActiveState().Content.Ground.ReuseInputsFrom(State_.Content.Ground, *Device_),
                ActiveState().WhyNot)) {
    return std::unexpected(ActiveState().WhyNot);
  }
  if (!ActiveState().Content.Ground.Ready()) {
    const auto ground = ActiveState().Content.Ground.Replace(
        State_.Frame.Handles.Device, {}, {}, std::chrono::seconds(5));
    if (!ground) {
      ActiveState().WhyNot = ground.error().Message;
      return std::unexpected(ActiveState().WhyNot);
    }
  }
  ActiveState().Content.Geometry.GroundFrom({.Classes = ActiveState().Content.Ground.Classes(),
                                             .Palette = ActiveState().Content.Ground.Palette()});
  ActiveState().Content.Glass.GroundFrom({.Classes = ActiveState().Content.Ground.Classes(),
                                          .Palette = ActiveState().Content.Ground.Palette()});
  ActiveState().Submitted = false;
  ActiveState().Ready = true;
  return true;
}

std::expected<SceneRenderer::FrameResources, std::string>
SceneRenderer::BuildFrameResources(Extent frame, const Compiled &plan, bool presents) {
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

  for (const Compiled::Pass &pass : plan.Passes()) {
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
                                          std::shared_ptr<const Compiled> plan) {
  ActiveState().Ready = false;
  ActiveState().Submitted = false;
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

std::expected<void, std::string>
SceneRenderer::InitForTarget(Extent frame, std::shared_ptr<const Compiled> plan, bool presents) {
  const auto offline = OfflineUploads(std::chrono::seconds(5));
  ActiveState().WhyNot.clear();
  if (const auto valid = ValidateExecutablePlan(*plan); !valid) { return valid; }
  if (!Stands()) { return std::unexpected(ActiveState().WhyNot); }
  ActiveState().Content.Ground.UploadsFrom(Uploads_.get());
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

SceneRenderer::FrameGraphAllocations SceneRenderer::FrameGraphAllocationCounts() const noexcept {
  FrameGraphAllocations counts;
  const auto count = [&counts](const FrameResources &frame) {
    const std::array textures{&frame.Offscreen,
                              &frame.HdrTex,
                              &frame.VelTex,
                              &frame.DepthTex,
                              &frame.FrameTex,
                              &frame.TransmittanceLut,
                              &frame.MultiScatterLut,
                              &frame.SkyViewLut,
                              &frame.EnvironmentSpecular,
                              &frame.ShadowAtlas,
                              &frame.TransmissiveTex,
                              &frame.CompositedTex,
                              &frame.AerialTex,
                              &frame.ShadingNormalTex,
                              &frame.SurfaceIdentityTex,
                              &frame.SurfaceBaseTex,
                              &frame.SurfaceMetalRoughTex,
                              frame.LinearTex.data(),
                              &frame.LinearTex[1]};
    for (const std::unique_ptr<Native::Texture> *texture : textures) {
      counts.Textures += static_cast<size_t>(static_cast<bool>(*texture));
    }
    for (const std::unique_ptr<Native::Buffer> *buffer :
         {&frame.IrradianceBuffer, &frame.Pyramid, &frame.GroundPyramid}) {
      counts.Buffers += static_cast<size_t>(static_cast<bool>(*buffer));
    }
    for (const std::unique_ptr<Native::Sampler> *sampler : {&frame.Samp, &frame.LutSamp}) {
      counts.Samplers += static_cast<size_t>(static_cast<bool>(*sampler));
    }
  };
  count(State_.Frame);
  if (Candidate_ && Candidate_->Frame) { count(*Candidate_->Frame); }
  return counts;
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
    case Resource::SceneSurfaceBase: return static_cast<bool>(frame.SurfaceBaseTex);
    case Resource::SceneSurfaceMetalRough: return static_cast<bool>(frame.SurfaceMetalRoughTex);
    case Resource::SceneDepth: return static_cast<bool>(frame.DepthTex);
    case Resource::FrameTex: return static_cast<bool>(frame.FrameTex);
    case Resource::TransmittanceLut: return static_cast<bool>(frame.TransmittanceLut);
    case Resource::MultiScatterLut: return static_cast<bool>(frame.MultiScatterLut);
    case Resource::SkyViewLut: return static_cast<bool>(frame.SkyViewLut);
    case Resource::EnvironmentSpecular: return static_cast<bool>(frame.EnvironmentSpecular);
    case Resource::LutSampler: return static_cast<bool>(frame.LutSamp);
    case Resource::IrradianceBuffer: return static_cast<bool>(frame.IrradianceBuffer);
    case Resource::DepthPyramid: return static_cast<bool>(frame.Pyramid);
    case Resource::GroundPyramid: return static_cast<bool>(frame.GroundPyramid);
    case Resource::GroundSelection: return true;
    case Resource::ShadowAtlas: return static_cast<bool>(frame.ShadowAtlas);
    case Resource::SceneLinear: return frame.LinearTex[0] && frame.LinearTex[1];
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
  }
  return false;
}

Native::Texture *SceneRenderer::Target(Resource resource) const {
  return Target(ActiveFrame(), resource);
}

Native::Texture *SceneRenderer::Target(const FrameResources &frame, Resource resource) {
  switch (resource) {
    case Resource::OverlayAtlas: return nullptr;
    case Resource::SceneHdr: return frame.HdrTex.get();
    case Resource::SceneTransmissive: return frame.TransmissiveTex.get();
    case Resource::SceneComposited: return frame.CompositedTex.get();
    case Resource::SceneAerial: return frame.AerialTex.get();
    case Resource::SceneVelocity: return frame.VelTex.get();
    case Resource::SceneShadingNormal: return frame.ShadingNormalTex.get();
    case Resource::SceneSurfaceIdentity: return frame.SurfaceIdentityTex.get();
    case Resource::SceneSurfaceBase: return frame.SurfaceBaseTex.get();
    case Resource::SceneSurfaceMetalRough: return frame.SurfaceMetalRoughTex.get();
    case Resource::SceneDepth: return frame.DepthTex.get();
    case Resource::FrameTex: return frame.FrameTex.get();

    case Resource::Surface: return frame.Offscreen.get();

    case Resource::ClusterSphere:
    case Resource::ClusterIndex:
    case Resource::ClusterJobs:
    case Resource::ClusterBatches:
    case Resource::ClusterKept:
    case Resource::ClusterSlot:
    case Resource::DrawIndex:
    case Resource::DrawArguments: return nullptr;
    case Resource::TransmittanceLut: return frame.TransmittanceLut.get();
    case Resource::MultiScatterLut: return frame.MultiScatterLut.get();
    case Resource::SkyViewLut: return frame.SkyViewLut.get();
    case Resource::EnvironmentSpecular: return frame.EnvironmentSpecular.get();
    case Resource::ShadowAtlas: return frame.ShadowAtlas.get();
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
    case Resource::AoBuffer: return nullptr;

    case Resource::SceneLinear: return frame.LinearTex[frame.LinearAt].get();
    case Resource::kCount: return nullptr;
  }
  return nullptr;
}

Native::Buffer *SceneRenderer::BufferFor(Resource resource) const {
  const GeometryResidency &resident = ActiveState().Content.Geometry.Resident();
  switch (resource) {
    case Resource::ClusterSphere:
      return resident.Buffer(GeometryResidency::Stream::ClusterSpheres).get();
    case Resource::ClusterIndex: return resident.Buffer(GeometryResidency::Stream::Index).get();
    case Resource::ClusterJobs:
      return resident.Buffer(GeometryResidency::Stream::ClusterJobs).get();
    case Resource::ClusterBatches:
      return resident.Buffer(GeometryResidency::Stream::ClusterBatches).get();
    case Resource::ClusterKept:
      return resident.Buffer(GeometryResidency::Stream::ClusterKept).get();
    case Resource::ClusterSlot:
      return resident.Buffer(GeometryResidency::Stream::ClusterSlot).get();
    case Resource::DrawIndex: return resident.Buffer(GeometryResidency::Stream::DrawIndex).get();
    case Resource::DrawArguments:
      return resident.Buffer(GeometryResidency::Stream::DrawArguments).get();
    case Resource::IrradianceBuffer: return ActiveFrame().IrradianceBuffer.get();
    case Resource::DepthPyramid: return ActiveFrame().Pyramid.get();
    case Resource::GroundPyramid: return ActiveFrame().GroundPyramid.get();
    case Resource::GroundSelection:
      return ActiveState().Content.Geometry.Ground().SelectedPatches();
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

bool SceneRenderer::ConfigureGeometry(SceneRenderer &renderer,
                                      FrameResources &frame,
                                      const Compiled &plan,
                                      bool drawsGlass,
                                      std::string &error) {
  (void)plan;
  return Accepted(renderer.ActiveState().Content.Geometry.Configure(
                      frame.GeometryPipelines, frame.Handles, nullptr, nullptr, drawsGlass),
                  error);
}

bool SceneRenderer::ConfigureGlass(SceneRenderer &renderer,
                                   FrameResources &frame,
                                   const Compiled &plan,
                                   bool drawsGlass,
                                   std::string &error) {
  (void)plan;
  (void)drawsGlass;
  return Accepted(
      renderer.ActiveState().Content.Glass.Configure(
          frame.GlassPipelines, frame.Handles, frame.HdrTex.get(), frame.Samp.get(), false),
      error);
}

bool SceneRenderer::ConfigureCompositeTransmission(SceneRenderer &renderer,
                                                   FrameResources &frame,
                                                   const Compiled &plan,
                                                   bool drawsGlass,
                                                   std::string &error) {
  static_cast<void>(renderer);
  (void)drawsGlass;
  return Accepted(frame.CompositeTransmission.Configure(
                      frame.Handles,
                      {.Opaque = frame.HdrTex.get(),
                       .Transmissive = frame.TransmissiveTex.get(),
                       .Exact = frame.Samp.get(),
                       .Target = TextureFormatOf(plan.Format(Resource::SceneComposited))}),
                  error);
}

bool SceneRenderer::ConfigureOverlay(SceneRenderer &renderer,
                                     FrameResources &frame,
                                     const Compiled &plan,
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
                                     const Compiled &plan,
                                     bool drawsGlass,
                                     std::string &error) {
  static_cast<void>(renderer);
  (void)plan;
  (void)drawsGlass;
  if (!Accepted(frame.Present.Configure(frame.FrameTex.get(), frame.Samp.get()), error)) {
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
  return Accepted(
      frame.Tonemap.Configure(frame.Handles,
                              {.Scene = Target(frame, plan.Bound(input)),
                               .Depth = frame.DepthTex.get(),
                               .Exact = frame.Samp.get(),
                               .Linear = TextureFormatOf(plan.Format(Resource::SceneLinear))},
                              display),
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
  return Accepted(frame.MediumTransmittance.Configure(frame.Handles, frame.TransmittanceLut.get()),
                  error);
}

bool SceneRenderer::ConfigureMediumMultiScatter(SceneRenderer &renderer,
                                                FrameResources &frame,
                                                const Compiled &plan,
                                                bool drawsGlass,
                                                std::string &error) {
  static_cast<void>(renderer);
  (void)plan;
  (void)drawsGlass;
  return Accepted(frame.MultiScatter.Configure(frame.Handles,
                                               frame.TransmittanceLut.get(),
                                               frame.LutSamp.get(),
                                               frame.MultiScatterLut.get()),
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
  return Accepted(frame.Radiance.Configure(frame.Handles,
                                           frame.TransmittanceLut.get(),
                                           frame.MultiScatterLut.get(),
                                           frame.LutSamp.get(),
                                           frame.SkyViewLut.get()),
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
  return Accepted(frame.Sky.Configure(frame.Handles,
                                      {.SkyView = frame.SkyViewLut.get(),
                                       .Transmittance = frame.TransmittanceLut.get(),
                                       .Lut = frame.LutSamp.get()}),
                  error);
}

bool SceneRenderer::ConfigureAerialPerspective(SceneRenderer &renderer,
                                               FrameResources &frame,
                                               const Compiled &plan,
                                               bool drawsGlass,
                                               std::string &error) {
  static_cast<void>(renderer);
  (void)drawsGlass;
  return Accepted(
      frame.Aerial.Configure(frame.Handles,
                             {.Scene = Target(frame, plan.Bound(Resource::SceneComposited)),
                              .Depth = frame.DepthTex.get(),
                              .SkyView = frame.SkyViewLut.get(),
                              .Transmittance = frame.TransmittanceLut.get(),
                              .Exact = frame.Samp.get(),
                              .Lut = frame.LutSamp.get()},
                             TextureFormatOf(plan.Format(Resource::SceneAerial))),
      error);
}

bool SceneRenderer::ConfigureLightVisibility(SceneRenderer &renderer,
                                             FrameResources &frame,
                                             const Compiled &plan,
                                             bool drawsGlass,
                                             std::string &error) {
  (void)plan;
  (void)drawsGlass;
  return Accepted(frame.Shadow.Configure(renderer.ActiveState().Content.Geometry, frame.Handles),
                  error);
}

void SceneRenderer::ApplyWorldDeclarations() {
  ActiveFrame().MediumTransmittance.Declare(ActiveState().Medium);
  ActiveFrame().MultiScatter.Declare(ActiveState().Medium);
  ActiveFrame().Radiance.Declare(
      ActiveState().Medium, ActiveState().CosSunZenith, ActiveState().EyeHeightM);
  ActiveFrame().SkyIrradianceStage.Declare(ActiveState().Medium, ActiveState().CosSunZenith);
  ActiveFrame().EnvironmentSpecularStage.Declare(
      ActiveState().Medium,
      ActiveState().CosSunZenith,
      ActiveState().EyeHeightM,
      ActiveState().Content.Geometry.Environment().GroundAlbedo);
  if (ActiveState().SkyDeclared) {
    const SkyStanding stands = {.SunDir = ActiveState().SkyToSun,
                                .WorldUp = ActiveState().SkyUp,
                                .IlluminanceLux = ActiveState().SkyIlluminanceLux,
                                .EyeHeightM = ActiveState().EyeHeightM};
    ActiveFrame().Sky.Declare(ActiveState().Medium, stands);
  }
  const GeometryRenderer::Reflections reflections{
      .Atlas = ActiveState().MediumDeclared && ActiveState().SkyDeclared
                   ? ActiveFrame().EnvironmentSpecular.get()
                   : nullptr,
      .Sampler = ActiveFrame().LutSamp.get(),
      .Up = ActiveState().SkyUp,
      .ToSun = ActiveState().SkyToSun};
  ActiveState().Content.Geometry.ReflectionsFrom(reflections);
  ActiveState().Content.Glass.ReflectionsFrom(reflections);
  if (ActiveState().ShadowDeclared) {
    ActiveFrame().Shadow.Declare({.ToSun = ActiveState().ShadowToSun, .Up = ActiveState().ShadowUp},
                                 ActiveState().ShadowRadiusM,
                                 ActiveState().ShadowCameraCentred);
  }
}

void SceneRenderer::BindFrameResources() {
  ActiveState().Content.Geometry.UsePipelines(ActiveFrame().GeometryPipelines);
  ActiveState().Content.Glass.UsePipelines(ActiveFrame().GlassPipelines);
  ActiveState().Content.Glass.Shares(ActiveState().Content.Geometry.Owned());
  ActiveFrame().Shadow.Binds(ActiveState().Content.Geometry);
  ActiveFrame().Cull.Binds(CullSource(ActiveState().Content.Geometry));
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
  for (int axis = 0; axis < 3; axis++) {
    ctx.PrevPreViewTranslation[axis] =
        ActiveState().Submitted ? -ActiveState().PrevEye[axis] : ctx.PreViewTranslation[axis];
  }
  for (int at = 0; at < 16; at++) {
    ctx.PrevMvp[at] = ActiveState().Submitted ? ActiveState().PrevMvp[at] : ctx.Mvp[at];
  }
  return ctx;
}

Native::Result<void> SceneRenderer::EncodeStage(Stage stage, const PassRecording &into) {
  const Executor *seat = ExecutorOf(stage);
  if (seat == nullptr || seat->Encode == nullptr) { return {}; }

  const FrameContext ctx = Framing();

  const outshine::Heap::Tagged encoding(Row(stage).Name);
  const auto began = std::chrono::steady_clock::now();
  if constexpr (kGpuValidation) { into.Commands->BeginLabel(Row(stage).Name); }
  auto result = (this->*seat->Encode)(ctx, into);
  if constexpr (kGpuValidation) { into.Commands->EndLabel(); }
  Effort &spent = PendingSpent_[static_cast<size_t>(stage)];
  spent.TookMs +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  spent.Draws = 0;
  spent.Triangles = 0;
  spent.Surfaces = 0;
  spent.Placements = 0;
  if (stage == Stage::Geometry || stage == Stage::TransmissiveGeometry) {
    const GeometryRenderer &drew =
        stage == Stage::Geometry ? ActiveState().Content.Geometry : ActiveState().Content.Glass;
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
  return result;
}

Native::Result<void> SceneRenderer::EncodeGeometry(const FrameContext &ctx,
                                                   const PassRecording &into) {
  return ActiveState().Content.Geometry.Encode(ctx, Picture(true, into));
}

Native::Result<void> SceneRenderer::EncodeGlass(const FrameContext &ctx,
                                                const PassRecording &into) {
  return ActiveState().Content.Glass.Encode(ctx, Picture(true, into));
}

Native::Result<void> SceneRenderer::EncodeCompositeTransmission(const FrameContext &ctx,
                                                                const PassRecording &into) {
  return ActiveFrame().CompositeTransmission.Encode(ctx, Picture(true, into));
}

Native::Result<void> SceneRenderer::EncodeTonemap(const FrameContext &ctx,
                                                  const PassRecording &into) {
  ActiveFrame().Tonemap.Bind(DisplaySource());
  const Vec2f delta = {{ActiveState().Jitter[0] - ActiveState().PrevJitter[0],
                        ActiveState().Jitter[1] - ActiveState().PrevJitter[1]}};
  ActiveFrame().Tonemap.BindTemporal(
      {.History = ActiveFrame().LinearTex[1 - ActiveFrame().LinearAt].get(),
       .Velocity = ActiveFrame().VelTex.get()},
      Extent{.WidthPx = ActiveFrame().Width, .HeightPx = ActiveFrame().Height},
      delta,
      ActiveFrame().HistoryHeld);
  return ActiveFrame().Tonemap.Encode(ctx, Picture(true, into));
}

Native::Result<void> SceneRenderer::EncodeOverlay(const FrameContext &ctx,
                                                  const PassRecording &into) {
  ActiveFrame().OverlayPipe.Bind(
      Extent{.WidthPx = ActiveFrame().Width, .HeightPx = ActiveFrame().Height});
  return ActiveFrame().OverlayPipe.Encode(ActiveState().Content.Overlay, ctx, Picture(false, into));
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
  return ActiveFrame().MediumTransmittance.Encode(into);
}

Native::Result<void> SceneRenderer::EncodeMediumMultiScatter(const FrameContext &ctx,
                                                             const PassRecording &into) {
  (void)ctx;
  return ActiveFrame().MultiScatter.Encode(into);
}

Native::Result<void> SceneRenderer::EncodeMediumRadiance(const FrameContext &ctx,
                                                         const PassRecording &into) {
  (void)ctx;
  return ActiveFrame().Radiance.Encode(into);
}

bool SceneRenderer::SetGroundClasses(std::span<const uint32_t> classes,
                                     std::span<const float> palette,
                                     std::string &error,
                                     GroundClassUploadMetrics *metrics) {
  if (!UploadGroundClasses(classes, palette, error, metrics)) { return false; }
  const auto copyAt = std::chrono::steady_clock::now();
  ActiveState().Content.Resources.SetGroundClassification(classes, palette);
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
  ActiveState().Content.Resources.SetGroundClassification(std::move(source));
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
  auto begun = content.Ground.BeginReplace(ActiveFrame().Handles.Device,
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
  auto advanced =
      content.Ground.AdvanceReplace(bytesMost, metrics != nullptr ? &metrics->Storage : nullptr);
  if (!advanced) {
    content.PendingGroundClasses.reset();
    return std::unexpected(std::move(advanced.error().Message));
  }
  if (!*advanced) { return false; }
  content.Geometry.GroundFrom(
      {.Classes = content.Ground.Classes(), .Palette = content.Ground.Palette()});
  content.Glass.GroundFrom(
      {.Classes = content.Ground.Classes(), .Palette = content.Ground.Palette()});
  const auto retainAt = std::chrono::steady_clock::now();
  content.Resources.SetGroundClassification(std::move(*content.PendingGroundClasses));
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
  const auto uploaded =
      ActiveState().Content.Ground.Replace(ActiveFrame().Handles.Device,
                                           classes,
                                           palette,
                                           std::chrono::seconds(5),
                                           metrics != nullptr ? &metrics->Storage : nullptr);
  if (!uploaded) {
    error = uploaded.error().Message;
    return false;
  }
  ActiveState().Content.Geometry.GroundFrom({.Classes = ActiveState().Content.Ground.Classes(),
                                             .Palette = ActiveState().Content.Ground.Palette()});
  ActiveState().Content.Glass.GroundFrom({.Classes = ActiveState().Content.Ground.Classes(),
                                          .Palette = ActiveState().Content.Ground.Palette()});
  return true;
}

bool SceneRenderer::ConfigureEnvironmentSpecular(SceneRenderer &renderer,
                                                 FrameResources &frame,
                                                 const Compiled &plan,
                                                 bool drawsGlass,
                                                 std::string &error) {
  (void)renderer;
  (void)plan;
  (void)drawsGlass;
  return Accepted(
      frame.EnvironmentSpecularStage.Configure(frame.Handles,
                                               {.Sky = frame.SkyViewLut.get(),
                                                .Irradiance = frame.IrradianceBuffer.get(),
                                                .Sampler = frame.LutSamp.get(),
                                                .Target = frame.EnvironmentSpecular.get()}),
      error);
}

Native::Result<void> SceneRenderer::EncodeEnvironmentSpecular(const FrameContext &ctx,
                                                              const PassRecording &into) {
  (void)ctx;
  return ActiveFrame().EnvironmentSpecularStage.Encode(into);
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
        frame.Handles.Device, {}, {}, std::chrono::seconds(5));
    if (!uploaded) {
      error = uploaded.error().Message;
      return false;
    }
    renderer.ActiveState().Content.Geometry.GroundFrom(
        {.Classes = renderer.ActiveState().Content.Ground.Classes(),
         .Palette = renderer.ActiveState().Content.Ground.Palette()});
    renderer.ActiveState().Content.Glass.GroundFrom(
        {.Classes = renderer.ActiveState().Content.Ground.Classes(),
         .Palette = renderer.ActiveState().Content.Ground.Palette()});
  }
  return Accepted(frame.SkyIrradianceStage.Configure(frame.Handles,
                                                     frame.TransmittanceLut.get(),
                                                     frame.MultiScatterLut.get(),
                                                     frame.LutSamp.get(),
                                                     frame.IrradianceBuffer.get()),
                  error);
}

Native::Result<void> SceneRenderer::EncodeIrradiance(const FrameContext &ctx,
                                                     const PassRecording &into) {
  (void)ctx;
  return ActiveFrame().SkyIrradianceStage.Encode(into);
}

bool SceneRenderer::ConfigureDepthPyramid(SceneRenderer &renderer,
                                          FrameResources &frame,
                                          const Compiled &plan,
                                          bool drawsGlass,
                                          std::string &error) {
  static_cast<void>(renderer);
  (void)plan;
  (void)drawsGlass;
  return Accepted(frame.PyramidStage.Configure(frame.Handles,
                                               frame.DepthTex.get(),
                                               frame.Samp.get(),
                                               frame.Pyramid.get(),
                                               {.WidthPx = frame.Width, .HeightPx = frame.Height}),
                  error);
}

Native::Result<void> SceneRenderer::EncodeDepthPyramid(const FrameContext &ctx,
                                                       const PassRecording &into) {
  (void)ctx;
  return ActiveFrame().PyramidStage.Encode(into);
}

bool SceneRenderer::ConfigureGeometryCull(SceneRenderer &renderer,
                                          FrameResources &frame,
                                          const Compiled &plan,
                                          bool drawsGlass,
                                          std::string &error) {
  (void)plan;
  (void)drawsGlass;
  frame.Cull.PyramidFrom(frame.Pyramid.get(),
                         PyramidOver({.WidthPx = static_cast<uint32_t>(frame.Width),
                                      .HeightPx = static_cast<uint32_t>(frame.Height)}));
  return Accepted(
      frame.Cull.Configure(CullSource(renderer.ActiveState().Content.Geometry), frame.Handles),
      error);
}

Native::Result<void> SceneRenderer::EncodeGeometryCull(const FrameContext &ctx,
                                                       const PassRecording &into) {
  ActiveFrame().Cull.Binds(CullSource(ActiveState().Content.Geometry));
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
  ActiveState().Content.Geometry.ShadowedBy(ActiveFrame().ShadowAtlas.get(),
                                            ActiveFrame().LutSamp.get(),
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
  const SkyStanding sky{.SunDir = ActiveState().SkyToSun,
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

Native::Result<void>
SceneRenderer::EncodePass(Native::Commands *commands, size_t pass, StageSubmission &submission) {
  const auto &declared = ActiveState().Plan->Passes()[pass];
  if (declared.Count == 1 &&
      ActiveState().Plan->Order()[declared.First] == Stage::LightVisibility &&
      ActiveFrame().Shadow.Cached()) {
    return EncodeStage(Stage::LightVisibility,
                       {.Commands = commands, .Submission = submission, .Area = {}, .Clip = {}});
  }
  return declared.Kind == PassKind::Compute ? EncodeComputePass(commands, declared, submission)
                                            : EncodeGraphicsPass(commands, declared, submission);
}

Native::Result<void> SceneRenderer::EncodeComputePass(Native::Commands *commands,
                                                      const Compiled::Pass &declared,
                                                      StageSubmission &submission) {
  const PassRecording into{.Commands = commands, .Submission = submission, .Area = {}, .Clip = {}};
  for (size_t at = 0; at < declared.Count; ++at) {
    auto encoded = EncodeStage(ActiveState().Plan->Order()[declared.First + at], into);
    if (!encoded) { return encoded; }
  }
  return {};
}

Native::Result<void> SceneRenderer::EncodeGraphicsPass(Native::Commands *commands,
                                                       const Compiled::Pass &declared,
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
  auto begun =
      commands->BeginRenderPass({.Colours = std::span(colours).first(count), .Depth = depth});
  if (!begun) { return begun; }
  const PassRecording into{
      .Commands = commands,
      .Submission = submission,
      .Area = {.Width = static_cast<float>(width), .Height = static_cast<float>(height)},
      .Clip = {.Width = width, .Height = height}};
  for (size_t at = 0; at < declared.Count; ++at) {
    auto encoded = EncodeStage(ActiveState().Plan->Order()[declared.First + at], into);
    if (!encoded) { return encoded; }
  }
  return commands->EndRenderPass();
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

  ActiveState().Content.Geometry.CastsNoShadow();
  for (bool &touched : Touched_) { touched = false; }
  {
    std::string why;
    if (!ActiveState().Content.Geometry.HandTables(why) ||
        !ActiveState().Content.Geometry.HandPlacements(true, why) ||
        (ActiveState().Content.DrawsGlass && !ActiveState().Content.Glass.HandTables(why))) {
      return std::unexpected(std::move(why));
    }
  }
  SettleShadow();
  return {};
}

Native::Result<void> SceneRenderer::FlushCrossings(Native::Commands &commands) {
  auto flushed = ActiveState().Content.Geometry.FlushCrossings(&commands);
  if (!flushed) { return flushed; }
  if (ActiveState().Content.DrawsGlass) {
    return ActiveState().Content.Glass.FlushCrossings(&commands);
  }
  return {};
}

std::expected<Native::Completion, std::string> SceneRenderer::PrepareWorldResources() {
  const auto published = PublishedWorld();
  if (const auto prepared = PrepareFrame(); !prepared) { return std::unexpected(prepared.error()); }
  auto commands =
      Uploads_->Begin([this] { return Submission_.Acquire(Submission_.Context, Device_.get()); });
  if (!commands) { return std::unexpected(commands.error().Message); }
  const auto flushed = FlushCrossings(**commands);
  if (!flushed) { return std::unexpected(flushed.error().Message); }
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
    auto encoded = EncodePass(commands->get(), pass, preparation);
    if (!encoded) { return std::unexpected(encoded.error().Message); }
  }
  auto completion = Submission_.Submit(Submission_.Context, Device_.get(), std::move(*commands));
  if (!completion) { return std::unexpected(completion.error().Message); }
  ActiveState().Content.Geometry.CommitCrossings();
  if (ActiveState().Content.DrawsGlass) { ActiveState().Content.Glass.CommitCrossings(); }
  preparation.Commit();
  Uploads_->Submitted(*completion);
  return *completion;
}

bool SceneRenderer::WorldResourcesComplete(const Native::Completion &completion) {
  if (!Device_) { return false; }
  const auto status = Submission_.QueryFence(Submission_.Context, Device_.get(), completion);
  if (!status || *status != Native::CompletionStatus::Ready) { return false; }
  ActiveState().Content.Geometry.Owned().ReleaseUploadScratch();
  ActiveState().Content.Glass.Owned().ReleaseUploadScratch();
  return true;
}

std::expected<void, std::string> SceneRenderer::RenderFrame() {
  const auto published = PublishedWorld();
  return RenderPublishedFrame();
}

void SceneRenderer::CommitSubmittedFrame(StageSubmission &stageSubmission) {
  ActiveState().Content.Geometry.CommitCrossings();
  if (ActiveState().Content.DrawsGlass) { ActiveState().Content.Glass.CommitCrossings(); }
  stageSubmission.Commit();
  ActiveFrame().Cull.DepthSubmitted(ActiveState().Plan->Holds(Stage::DepthPyramid));
  LandedAt_ = (LandedAt_ + 1) % kFramesInFlight;
  for (int axis = 0; axis < 3; axis++) {
    ActiveState().PrevEye[axis] = ActiveState().Camera.EyeM[axis];
  }
  ActiveState().Content.Geometry.CarryFrame();
  ActiveState().Content.Glass.CarryFrame();

  ActiveState().PrevMvp = Through().ViewProjection(ActiveState().Camera);
  ActiveState().Submitted = true;
  LastSubmittedCamera_.Basis = ActiveState().Camera;
  LastSubmittedCamera_.GeometryAnchorM = ActiveState().Content.Geometry.AnchorM();
  ++LastSubmittedCamera_.Serial;
}

std::expected<void, std::string> SceneRenderer::RenderPublishedFrame() {
  const auto nonblocking = OfflineUploads(std::chrono::nanoseconds::zero());
  const auto began = std::chrono::steady_clock::now();
  auto phaseBegan = began;
  const auto phaseMs = [&phaseBegan] {
    const auto now = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(now - phaseBegan).count();
    phaseBegan = now;
    return ms;
  };
  RenderFrameTiming timing;
  const auto record = [&timing, &phaseMs](RenderFramePhase phase) {
    timing.PhaseMs[static_cast<size_t>(phase)] = phaseMs();
  };
  const auto slot = WaitForFrameSlot();
  if (!slot) { return std::unexpected(slot.error().Message); }
  record(RenderFramePhase::FenceWait);
  auto prepared = PrepareFrame();
  if (!prepared) { return prepared; }
  record(RenderFramePhase::Prepare);
  auto commands =
      Uploads_->Begin([this] { return Submission_.Acquire(Submission_.Context, Device_.get()); });
  if (!commands) { return std::unexpected(commands.error().Message); }
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
    if (presenting) { ActiveFrame().HostSurface = ActiveFrame().Offscreen.get(); }
  };

  auto culled = ActiveState().Content.Geometry.Ground().Cull(
      Framing(), ActiveState().Content.Geometry.AnchorM(), **commands);
  if (!culled) {
    restoreTemporalState();
    return std::unexpected(culled.error().Message);
  }
  record(RenderFramePhase::Cull);
  PendingSpent_ = {};
  for (size_t pass = 0; pass < ActiveState().Plan->Passes().size(); ++pass) {
    auto encoded = EncodePass(commands->get(), pass, stageSubmission);
    if (!encoded) {
      restoreTemporalState();
      return std::unexpected(encoded.error().Message);
    }
  }
  record(RenderFramePhase::Encode);
  if (presenting) {
    auto queued = (*commands)->Present(std::move(presenting));
    ActiveFrame().HostSurface = ActiveFrame().Offscreen.get();
    if (!queued) {
      restoreTemporalState();
      return std::unexpected(queued.error().Message);
    }
  }
  auto submitted = Submission_.Submit(Submission_.Context, Device_.get(), std::move(*commands));
  if (!submitted) {
    restoreTemporalState();
    return std::unexpected(submitted.error().Message);
  }
  Landed_[LandedAt_] = *submitted;
  LastFrameSubmission_ = *submitted;
  Uploads_->Submitted(*submitted);
  record(RenderFramePhase::Submit);
  CommitSubmittedFrame(stageSubmission);
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
  ActiveState().Submitted = false;
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
  ActiveState().Content.Ground.UploadsFrom(Uploads_.get());
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
