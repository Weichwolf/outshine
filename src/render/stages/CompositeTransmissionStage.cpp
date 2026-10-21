#include "CompositeTransmissionStage.h"
#include "StagePipelines.h"
#include <array>
#include <expected>
#include <span>
#include <utility>

namespace outshine::Render {
Native::Result<void> CompositeTransmissionStage::Configure(const Gpu &gpu, const Feeds &from) {
  if (gpu.Device == nullptr || from.Opaque == nullptr || from.Transmissive == nullptr ||
      from.Exact == nullptr) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "transmission composite requires native images and sampler"});
  }
  const std::array colours{Native::ColourTarget{.Texels = from.Target, .Rgb = {}, .Alpha = {}}};
  auto made = CreateFullscreenPipeline(
      *gpu.Device, "build/shaders/compositeTransmission.frag.spv", colours);
  if (!made) { return std::unexpected(made.error()); }
  Pipe = std::move(*made);
  Opaque = from.Opaque;
  Transmissive = from.Transmissive;
  Exact = from.Exact;
  return {};
}

Native::Result<void> CompositeTransmissionStage::Encode([[maybe_unused]] const FrameContext &ctx,
                                                        const PassRecording &into) {
  if (!Pipe || into.Commands == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "transmission composite is not configured"});
  }
  const std::array bindings{
      Native::Binding{.Set = 2,
                      .Slot = 0,
                      .Value =
                          Native::SampledTexture{.Image = {.Image = Opaque}, .Sampling = Exact}},
      Native::Binding{
          .Set = 2,
          .Slot = 1,
          .Value = Native::SampledTexture{.Image = {.Image = Transmissive}, .Sampling = Exact}}};
  return into.Commands->Draw({.State = {.Pipeline = Pipe.get(),
                                        .Bindings = bindings,
                                        .Vertices = {},
                                        .Area = into.Area,
                                        .Clip = into.Clip},
                              .Range = {.Count = 3}});
}
}
