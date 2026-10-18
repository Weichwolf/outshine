#include "AerialPerspectiveStage.h"

#include "StagePipelines.h"
#include <expected>
#include <span>
#include <utility>
#include <array>
#include <string>
#include <cstdint>

namespace outshine::Render {

Native::Result<void>
AerialPerspectiveStage::Configure(const Gpu &gpu, Tables from, Native::Format targetFormat) {
  if (gpu.Device == nullptr || from.Scene == nullptr || from.Depth == nullptr ||
      from.SkyView == nullptr || from.Transmittance == nullptr || from.Exact == nullptr ||
      from.Lut == nullptr) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "aerial perspective requires complete native inputs"});
  }
  const std::array colours{Native::ColourTarget{.Texels = targetFormat, .Rgb = {}, .Alpha = {}}};
  auto made =
      CreateFullscreenNdcPipeline(*gpu.Device, "build/shaders/aerialPerspective.frag.spv", colours);
  if (!made) { return std::unexpected(made.error()); }
  Pipe = std::move(*made);
  Scene = from.Scene;
  Depth = from.Depth;
  SkyView = from.SkyView;
  Veil = from.Transmittance;
  Exact = from.Exact;
  Lut = from.Lut;
  return {};
}

Native::Result<void> AerialPerspectiveStage::Encode(const MediumViewParameters &view,
                                                    const PassRecording &into) {
  if (!Pipe || into.Commands == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "aerial perspective is not configured"});
  }
  const std::array bindings{
      Native::Binding{.Set = 2,
                      .Slot = 0,
                      .Value =
                          Native::SampledTexture{.Image = {.Image = Scene}, .Sampling = Exact}},
      Native::Binding{.Set = 2,
                      .Slot = 1,
                      .Value =
                          Native::SampledTexture{.Image = {.Image = Depth}, .Sampling = Exact}},
      Native::Binding{.Set = 2,
                      .Slot = 2,
                      .Value =
                          Native::SampledTexture{.Image = {.Image = SkyView}, .Sampling = Lut}},
      Native::Binding{.Set = 2,
                      .Slot = 3,
                      .Value = Native::SampledTexture{.Image = {.Image = Veil}, .Sampling = Lut}},
      Native::Binding{.Set = 3,
                      .Slot = 2,
                      .Value = Native::Uniform{.Bytes = std::as_bytes(std::span(&view, 1))}}};
  return into.Commands->Draw({.State = {.Pipeline = Pipe.get(),
                                        .Bindings = bindings,
                                        .Vertices = {},
                                        .Area = into.Area,
                                        .Clip = into.Clip},
                              .Range = {.Count = 3}});
}
}
