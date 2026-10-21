#include "PresentStage.h"
#include "StagePipelines.h"
#include <array>
#include <cstddef>
#include <expected>
#include <utility>

namespace outshine::Render {

Native::Result<void> PresentStage::Configure(const Native::Texture *frame,
                                             const Native::Sampler *exact) {
  if (frame == nullptr || exact == nullptr) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "presentation requires a source image and sampler"});
  }
  Frame = frame;
  Exact = exact;
  return {};
}

Native::Result<void> PresentStage::For(Native::Device &device, Native::Format surfaceFormat) {
  const auto slot = static_cast<size_t>(surfaceFormat);
  if (slot >= Pipelines.size() || (Device != nullptr && Device != &device)) {
    return std::unexpected(Native::Error{
        .Kind = Native::ErrorKind::Invalid,
        .Message = "presentation requires a valid target format on the owning device"});
  }
  auto &cached = Pipelines[slot];
  if (!cached) {
    auto made = CreatePresentPipeline(device, surfaceFormat);
    if (!made) { return std::unexpected(made.error()); }
    cached = std::move(*made);
  }
  Pipe = cached.get();
  Device = &device;
  return {};
}

Native::Result<void> PresentStage::Encode(const PassRecording &into) const {
  if (Pipe == nullptr || Frame == nullptr || Exact == nullptr || into.Commands == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "presentation stage is not configured"});
  }
  const std::array bindings{Native::Binding{
      .Set = 2,
      .Slot = 0,
      .Value = Native::SampledTexture{.Image = {.Image = Frame}, .Sampling = Exact}}};
  return into.Commands->Draw({.State = {.Pipeline = Pipe,
                                        .Bindings = bindings,
                                        .Vertices = {},
                                        .Area = into.Area,
                                        .Clip = into.Clip},
                              .Range = {.Count = 3}});
}

}
