#include "AerialPerspectiveStage.h"
#include "math/Vec3.h"

#include "StagePipelines.h"
#include <expected>
#include <span>
#include <utility>
#include <array>
#include <string>
#include <cstdint>

namespace outshine::Render {

constexpr float kMPerKmF = 1000.0f;

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

void AerialPerspectiveStage::Declare(const Medium &medium, SkyStanding stands) {
  for (int axis = 0; axis < 3; ++axis) {
    Pushed_.SunDir[axis] = stands.SunDir[axis];
    Pushed_.WorldUp[axis] = stands.WorldUp[axis];
  }
  Pushed_.Illuminance = stands.IlluminanceLux;
  Pushed_.EyeRadiusKm = medium.BottomRadiusKm + kMediumGroundLiftKm +
                        (stands.EyeHeightM > 0.0f ? stands.EyeHeightM : 0.0f) / kMPerKmF;
  Pushed_.Air = medium;
  Declared_ = true;
}

void AerialPerspectiveStage::Eye(const Medium &medium, float eyeHeightM) {
  if (!Declared_) { return; }
  Pushed_.EyeRadiusKm = medium.BottomRadiusKm + kMediumGroundLiftKm +
                        (eyeHeightM > 0.0f ? eyeHeightM : 0.0f) / kMPerKmF;
}

void AerialPerspectiveStage::SetBasis(const EyeBasis &eye) {
  for (int axis = 0; axis < 3; ++axis) {
    Pushed_.Right[axis] = eye.Right[axis];
    Pushed_.Up[axis] = eye.Up[axis];
    Pushed_.Fwd[axis] = eye.Forward[axis];
  }
  Pushed_.TanHalf[0] = eye.TanHalfWidth;
  Pushed_.TanHalf[1] = eye.TanHalfHeight;
}

Native::Result<void> AerialPerspectiveStage::Encode([[maybe_unused]] const FrameContext &ctx,
                                                    const PassRecording &into) {
  if (!Declared_) { return {}; }
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
                      .Slot = 0,
                      .Value = Native::Uniform{.Bytes = std::as_bytes(std::span(&Pushed_, 1))}}};
  return into.Commands->Draw({.State = {.Pipeline = Pipe.get(),
                                        .Bindings = bindings,
                                        .Vertices = {},
                                        .Area = into.Area,
                                        .Clip = into.Clip},
                              .Range = {.Count = 3}});
}
}
