#include <optional>
#include <array>
#include <cstdint>
#include <cstddef>
#include <atomic>
#include "MaterialFieldPassState.h"
#include "MaterialFieldInputData.h"
#include "UploadQueue.h"
#include "PipelineFactory.h"
#include <algorithm>
#include <cassert>
#include <expected>
#include <memory>
#include <string>
#include <utility>

namespace outshine::Render {
ResidentMaterialField::ResidentMaterialField(std::shared_ptr<Data> data) noexcept
    : Data_(std::move(data)) {}

ResidentMaterialField::~ResidentMaterialField() = default;

Native::Completion ResidentMaterialField::Produced() const noexcept {
  return Data_->Produced;
}

MaterialFieldPass::MaterialFieldPass(Native::Device &device, UploadQueue *uploads)
    : State_(std::make_unique<State>(device, uploads)) {}

MaterialFieldPass::~MaterialFieldPass() {
  assert(State_->InFlight.empty());
}

Native::Result<void> MaterialFieldPass::Configure() {
  const auto &state = *State_;
  auto &pipelines = *state.Pipelines;
  if (!state.OnOwner() ||
      (state.Uploads != nullptr && &state.Uploads->Device() != &pipelines.Device)) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "material field pass has a different owner"});
  }
  if (pipelines.Ready) { return {}; }
  RenderScene scene;
  GeometryRenderer renderer(scene);
  Gpu gpu;
  gpu.Device = &pipelines.Device;
  gpu.HdrFormat = Native::Format::Rgba16Float;
  gpu.SurfaceFormat = Native::Format::Rgba8Unorm;
  (void)gpu.SceneColours.Add(Resource::SceneLinear);
  const auto geometry = renderer.Configure(
      pipelines.Geometry, gpu, nullptr, nullptr, false, GeometryPipelineScope::Meshes);
  if (!geometry) { return geometry; }
  const std::array colours{
      Native::ColourTarget{.Texels = Native::Format::Rgba16Float, .Rgb = {}, .Alpha = {}}};
  auto raster = CreateFullscreenPipeline(
      pipelines.Device, "materialFieldRaster.frag.spv", colours, Native::DepthTarget{});
  if (!raster) { return std::unexpected(raster.error()); }
  pipelines.Raster = std::move(*raster);
  const auto filtered = pipelines.Filter.Configure(pipelines.Device);
  if (!filtered) { return filtered; }
  const auto integrated = pipelines.Integral.Configure(pipelines.Device);
  if (!integrated) { return integrated; }
  auto sampler = pipelines.Device.CreateSampler({.Min = Native::Filter::Nearest,
                                                 .Mag = Native::Filter::Nearest,
                                                 .Mip = Native::Filter::Nearest,
                                                 .U = Native::Address::ClampToEdge,
                                                 .V = Native::Address::ClampToEdge,
                                                 .MinLod = 0,
                                                 .MaxLod = 0});
  if (!sampler) { return std::unexpected(sampler.error()); }
  if (!*sampler) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Driver,
                                         .Message = "material field has no sampling resource"});
  }
  pipelines.Sampling = std::move(*sampler);
  pipelines.Ready = true;
  return {};
}

Native::Result<void> MaterialFieldPass::Collect() {
  auto &state = *State_;
  if (!state.OnOwner()) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "material fields must retire on their owner"});
  }
  for (size_t at = 0; at < state.InFlight.size();) {
    auto completed = state.Pipelines->Device.Poll(state.InFlight[at].Completion);
    if (!completed) {
      if (completed.error().Kind == Native::ErrorKind::DeviceLost) { state.InFlight.clear(); }
      return std::unexpected(completed.error());
    }
    if (*completed == Native::CompletionStatus::Ready) {
      if (const auto &source = state.InFlight[at].Source; source && source->Stored) {
        source->Stored->Staging.reset();
      }
      state.InFlight.erase(state.InFlight.begin() + static_cast<std::ptrdiff_t>(at));
    } else {
      ++at;
    }
  }
  return {};
}

Native::Result<void> MaterialFieldPass::State::PrepareGeometry(ResidentMaterialField::Data &source,
                                                               const MaterialFieldInput &input,
                                                               Native::Commands &commands) const {
  Gpu gpu;
  gpu.Device = &Pipelines->Device;
  gpu.HdrFormat = Native::Format::Rgba16Float;
  gpu.SurfaceFormat = Native::Format::Rgba8Unorm;
  (void)gpu.SceneColours.Add(Resource::SceneLinear);
  auto attached = source.Renderer.AttachPipelines(Pipelines->Geometry, gpu);
  if (!attached) { return std::unexpected(attached.error()); }
  const auto mesh = input.Data_->Mesh();
  std::string error;
  if (!source.Renderer.ValidateMaterials(input.Data_->Materials, error)) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = std::move(error)});
  }
  auto &residency = source.Scene.Meshes();
  {
    auto recording = residency.RecordUploads(commands);
    if (!recording) { return std::unexpected(recording.error()); }
    if (!source.Renderer.SetMaterials(input.Data_->Materials, error) ||
        !source.Renderer.SetMesh(mesh, error)) {
      return std::unexpected(
          recording->Failure() != nullptr
              ? *recording->Failure()
              : Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = std::move(error)});
    }
    if (auto flushed = residency.SubmitPendingUploads(); !flushed) {
      return std::unexpected(flushed.error());
    }
  }
  return {};
}

std::optional<Native::Completion> MaterialFieldPass::LastSubmission() const noexcept {
  return State_->Last;
}

Native::Result<std::shared_ptr<const ResidentMaterialField>>
MaterialFieldPass::Prepare(const std::shared_ptr<const MaterialFieldInput> &input,
                           const std::atomic_bool *stopping) {
  auto &state = *State_;
  if (!state.OnOwner() || !state.Pipelines->Ready || !input) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "material field requires prepared inputs and device"});
  }
  if (stopping != nullptr && stopping->load(std::memory_order_relaxed)) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Cancelled,
                                         .Message = "material field request cancelled"});
  }
  if (const auto retired = Collect(); !retired) { return std::unexpected(retired.error()); }
  auto &device = state.Pipelines->Device;
  auto commands = device.Begin();
  if (!commands) { return std::unexpected(commands.error()); }
  auto source = std::make_shared<ResidentMaterialField::Data>(state.Pipelines);
  source->Raster = input->Data_->Raster;
  source->Capture = input->Data_->Capture;
  if (input->Data_->Stored) {
    auto raster = MaterialFieldRaster::Prepare(device, **commands, *input->Data_->Stored);
    if (!raster) { return std::unexpected(raster.error()); }
    source->Stored = std::move(*raster);
  } else {
    auto uploaded = state.PrepareGeometry(*source, *input, **commands);
    if (!uploaded) { return std::unexpected(uploaded.error()); }
  }
  if (stopping != nullptr && stopping->load(std::memory_order_relaxed)) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Cancelled,
                                         .Message = "material field request cancelled"});
  }
  auto produced = SubmitUpload(device, state.Uploads, std::move(*commands));
  if (!produced) { return std::unexpected(produced.error()); }
  source->Produced = *produced;
  if (!source->Stored) { source->Scene.Meshes().ReleaseUploadScratch(); }
  state.Last = *produced;
  state.InFlight.push_back({.Completion = *produced, .Source = source, .Images = {}});
  return std::shared_ptr<const ResidentMaterialField>(new ResidentMaterialField(std::move(source)));
}
}
