#include <string>
#include <cstdint>
#include <atomic>
#include "MaterialFieldPassState.h"
#include "FrameContext.h"
#include "Lens.h"
#include "StageSubmission.h"
#include "UploadQueue.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <memory>
#include <limits>
#include <span>
#include <utility>

namespace outshine::Render {
namespace {
bool ValidCameraBasis(const Viewpoint &camera) {
  const auto finite = [](const auto &values) {
    return std::ranges::all_of(values, [](double value) { return std::isfinite(value); });
  };
  return (camera.Kind == CameraKind::Perspective || camera.Kind == CameraKind::Orthographic) &&
         finite(camera.EyeM) && finite(camera.Right) && finite(camera.Up) &&
         finite(camera.Forward) &&
         std::abs(Dot(camera.Right, camera.Right) - 1) <= kMaterialFieldUnitTolerance &&
         std::abs(Dot(camera.Up, camera.Up) - 1) <= kMaterialFieldUnitTolerance &&
         std::abs(Dot(camera.Forward, camera.Forward) - 1) <= kMaterialFieldUnitTolerance &&
         Dot(Cross(camera.Right, camera.Up), camera.Forward) <= -1 + kMaterialFieldUnitTolerance;
}

Native::Result<FrameContext> FrameOf(const Viewpoint &camera, Extent extent) {
  if (!ValidCameraBasis(camera)) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "material field camera has an invalid basis"});
  }
  auto lens = Lens::From(camera, extent.WidthPx, extent.HeightPx);
  if (!lens) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "material field camera has an invalid lens"});
  }
  FrameContext frame;
  for (size_t axis = 0; axis < 3; ++axis) {
    frame.PreViewTranslation[axis] = frame.PrevPreViewTranslation[axis] = -camera.EyeM[axis];
  }
  frame.Mvp = frame.PrevMvp = lens->ViewProjection(camera);
  if (camera.Kind == CameraKind::Orthographic) {
    for (size_t axis = 0; axis < 3; ++axis) {
      frame.ViewPosition[axis] = static_cast<float>(-camera.Forward[axis]);
    }
    frame.ViewPosition[3] = 0;
  }
  return frame;
}

Native::Result<FrameContext> FrameOf(const MaterialFieldView &view, Extent extent) {
  auto frame = FrameOf(view.Camera, extent);
  if (!frame || !view.LightingCamera) { return frame; }
  const auto &camera = *view.LightingCamera;
  if (!ValidCameraBasis(camera)) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "material field lighting camera has an invalid basis"});
  }
  const bool parallel = camera.Kind == CameraKind::Orthographic;
  for (size_t axis = 0; axis < 3; ++axis) {
    const double value =
        parallel ? -camera.Forward[axis] : camera.EyeM[axis] + frame->PreViewTranslation[axis];
    if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max()) {
      return std::unexpected(
          Native::Error{.Kind = Native::ErrorKind::Invalid,
                        .Message = "material field lighting eye exceeds its frame"});
    }
    frame->ViewPosition[axis] = static_cast<float>(value);
  }
  frame->ViewPosition[3] = parallel ? 0 : 1;
  return frame;
}

Native::Result<std::shared_ptr<Native::Texture>> Target(Native::Device &device,
                                                        Extent extent,
                                                        Native::Format format,
                                                        Native::UseMask<Native::TextureUse> use) {
  auto image = device.CreateTexture({.Width = static_cast<uint32_t>(extent.WidthPx),
                                     .Height = static_cast<uint32_t>(extent.HeightPx),
                                     .Texels = format,
                                     .Use = use});
  if (!image) { return std::unexpected(image.error()); }
  if (!*image) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Driver,
                                         .Message = "material field target has no texture"});
  }
  return std::shared_ptr<Native::Texture>(std::move(*image));
}

bool ValidOutput(const MaterialFieldView &view, Extent raster, uint32_t textureLimit) {
  switch (view.Encoding) {
    case RadianceEncoding::Texels: return true;
    case RadianceEncoding::IntegralPairs:
      return view.Target == raster &&
             RadianceIntegral::StorageExtent(raster, textureLimit).has_value();
  }
  return false;
}

struct FieldTargets {
  std::shared_ptr<Native::Texture> Raw, Filtered, Depth, Scratch;
};

Native::Result<FieldTargets>
Targets(Native::Device &device, Extent raster, const MaterialFieldView &view) {
  constexpr auto kColourUse = Native::TextureUse::ColourAttachment | Native::TextureUse::Sampled |
                              Native::TextureUse::Storage | Native::TextureUse::CopySource;
  const bool integral = view.Encoding == RadianceEncoding::IntegralPairs;
  const auto storage = RadianceIntegral::StorageExtent(raster, device.Limits().MaxTextureExtent);
  if (integral && !storage) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "paired integral storage exceeds the device extent"});
  }
  const Extent extent = integral ? *storage : view.Target;
  auto raw = Target(device, raster, Native::Format::Rgba16Float, kColourUse);
  if (!raw) { return std::unexpected(raw.error()); }
  const auto use =
      Native::TextureUse::Storage | Native::TextureUse::Sampled | Native::TextureUse::CopySource;
  auto filtered = !integral && view.Target == raster
                      ? raw
                      : Target(device,
                               extent,
                               integral ? Native::Format::Rgba32Float : Native::Format::Rgba16Float,
                               use);
  if (!filtered) { return std::unexpected(filtered.error()); }
  auto depth = Target(device,
                      raster,
                      Native::Format::Depth32Float,
                      Native::TextureUse::DepthAttachment | Native::TextureUse::Sampled |
                          Native::TextureUse::CopySource);
  if (!depth) { return std::unexpected(depth.error()); }
  std::shared_ptr<Native::Texture> scratch;
  if (integral) {
    auto allocated = Target(device,
                            extent,
                            Native::Format::Rgba32Float,
                            Native::TextureUse::Storage | Native::TextureUse::Sampled);
    if (!allocated) { return std::unexpected(allocated.error()); }
    scratch = std::move(*allocated);
  }
  return FieldTargets{.Raw = std::move(*raw),
                      .Filtered = std::move(*filtered),
                      .Depth = std::move(*depth),
                      .Scratch = std::move(scratch)};
}

}

Native::Result<void> ResidentMaterialField::Data::Encode(const MaterialFieldView &view,
                                                         const FrameContext &frame,
                                                         const PassRecording &recording) {
  if (Stored) {
    return Stored->Encode(
        *recording.Commands, *Pipelines->Raster, *Pipelines->Sampling, view, frame, Raster);
  }
  std::string error;
  if (!Renderer.SetLights(view.Lights, error)) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = std::move(error)});
  }
  Renderer.SetEnvironment(view.Environment);
  return Renderer.Encode(frame, recording);
}

Native::Result<MaterialFieldImages> MaterialFieldPass::Relight(const ResidentMaterialField &source,
                                                               const MaterialFieldView &view,
                                                               const std::atomic_bool *stopping) {
  auto &state = *State_;
  const auto &data = source.Data_;
  if (!state.OnOwner() || !state.Pipelines->Ready || !data || data->Pipelines != state.Pipelines ||
      view.Target.WidthPx <= 0 || view.Target.HeightPx <= 0 ||
      view.Target.WidthPx > data->Raster.WidthPx || view.Target.HeightPx > data->Raster.HeightPx) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "material field has no matching source or footprint"});
  }
  if (data->Capture && !SameMaterialFieldCapture(*data->Capture, view.Camera)) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "stored material samples require their original capture volume"});
  }
  if (!ValidOutput(view, data->Raster, state.Pipelines->Device.Limits().MaxTextureExtent)) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "integral field needs its padded source raster"});
  }
  if (!ValidMaterialFieldLighting(view)) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "material field lighting is invalid"});
  }
  auto frame = FrameOf(view, data->Raster);
  if (!frame) { return std::unexpected(frame.error()); }
  if (stopping != nullptr && stopping->load(std::memory_order_relaxed)) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Cancelled,
                                         .Message = "material field request cancelled"});
  }
  if (const auto retired = Collect(); !retired) { return std::unexpected(retired.error()); }
  auto &device = state.Pipelines->Device;
  auto commands = device.Begin();
  if (!commands) { return std::unexpected(commands.error()); }
  auto targets = Targets(device, data->Raster, view);
  if (!targets) { return std::unexpected(targets.error()); }
  auto &[raw, filtered, depth, scratch] = *targets;
  StageSubmission submission;
  const auto width = static_cast<uint32_t>(data->Raster.WidthPx);
  const auto height = static_cast<uint32_t>(data->Raster.HeightPx);
  const PassRecording recording{
      .Commands = commands->get(),
      .Submission = submission,
      .Area = {.Width = static_cast<float>(width), .Height = static_cast<float>(height)},
      .Clip = {.Width = width, .Height = height}};
  const std::array attachments{
      Native::ColourAttachment{.Image = {.Image = raw.get()}, .Begin = Native::Load::Clear}};
  auto begun = (*commands)->BeginRenderPass(
      {.Colours = attachments,
       .Depth =
           Native::DepthAttachment{.Image = {.Image = depth.get()}, .Begin = Native::Load::Clear}});
  if (!begun) { return std::unexpected(begun.error()); }
  auto drawn = data->Encode(view, *frame, recording);
  if (!drawn) { return std::unexpected(drawn.error()); }
  auto ended = (*commands)->EndRenderPass();
  if (!ended) { return std::unexpected(ended.error()); }
  if (view.Encoding == RadianceEncoding::IntegralPairs) {
    const auto integrated = state.Pipelines->Integral.Encode(**commands, *raw, *scratch, *filtered);
    if (!integrated) { return std::unexpected(integrated.error()); }
  } else if (view.Target != data->Raster) {
    auto reduced = state.Pipelines->Filter.Encode(**commands, *raw, *filtered);
    if (!reduced) { return std::unexpected(reduced.error()); }
  }
  if (stopping != nullptr && stopping->load(std::memory_order_relaxed)) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Cancelled,
                                         .Message = "material field request cancelled"});
  }
  auto produced = SubmitUpload(device, state.Uploads, std::move(*commands));
  if (!produced) { return std::unexpected(produced.error()); }
  submission.Commit();
  MaterialFieldImages images{.Unfiltered = std::make_shared<RadianceImage>(
                                 RadianceImage{.Device = &device,
                                               .Texture = raw,
                                               .Sampling = state.Pipelines->Sampling,
                                               .Produced = *produced}),
                             .Filtered = std::make_shared<RadianceImage>(
                                 RadianceImage{.Device = &device,
                                               .Texture = filtered,
                                               .Sampling = state.Pipelines->Sampling,
                                               .Produced = *produced,
                                               .Encoding = view.Encoding}),
                             .Depth = depth,
                             .Camera = view.Camera,
                             .LightingCamera = view.LightingCamera.value_or(view.Camera)};
  state.Last = *produced;
  state.InFlight.push_back(
      {.Completion = *produced, .Source = data, .Images = images, .Scratch = std::move(scratch)});
  return images;
}
}
