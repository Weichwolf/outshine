#include "SkyStage.h"
#include "math/Vec3.h"

#include <array>
#include <cstdint>
#include <numbers>
#include <cstdio>
#include <string>

#include "SceneTargets.h"
#include "StagePipelines.h"
#include <expected>
#include <span>
#include <utility>

namespace outshine::Render {

namespace Says {
constexpr auto UnsupportedSkyAttachment = "sky pass has an unsupported colour attachment";
}

constexpr float kMPerKmF = 1000.0f;

Native::Result<void> SkyStage::Configure(const Gpu &gpu, Tables from) {
  if (gpu.Device == nullptr || from.SkyView == nullptr || from.Transmittance == nullptr ||
      from.Lut == nullptr || gpu.SceneColours.Empty()) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "sky draw requires complete native inputs and targets"});
  }
  std::array<Native::ColourTarget, kMaxColourAttachments> targets{};
  size_t index = 0;
  for (const auto resource : gpu.SceneColours) {
    auto &target = targets[index++];
    switch (resource) {
      case Resource::SceneHdr: target.Texels = gpu.HdrFormat; break;
      case Resource::SceneVelocity: target = VelocityTarget(true); break;
      case Resource::SceneSurfaceBase:
      case Resource::SceneShadingNormal:
        target.Texels = Native::Format::Rgba16Float;
        target.Write = {};
        break;
      case Resource::SceneSurfaceMetalRough:
        target.Texels = Native::Format::Rg16Float;
        target.Write = {};
        break;
      case Resource::SceneSurfaceIdentity:
        target.Texels = Native::Format::Rgba32Float;
        target.Write = {};
        break;
      default:
        return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                             .Message = Says::UnsupportedSkyAttachment});
    }
  }
  auto made = CreateFullscreenNdcPipeline(*gpu.Device,
                                          "build/shaders/sky.frag.spv",
                                          std::span(targets).first(gpu.SceneColours.Size()),
                                          Native::DepthTarget{.Test = false, .Write = false});
  if (!made) { return std::unexpected(made.error()); }
  Pipe = std::move(*made);
  SkyView = from.SkyView;
  Veil = from.Transmittance;
  Lut = from.Lut;
  return {};
}

void SkyStage::Declare(const Medium &medium, SkyStanding stands) {
  for (int axis = 0; axis < 3; ++axis) {
    Pushed_.SunDir[axis] = stands.SunDir[axis];
    Pushed_.WorldUp[axis] = stands.WorldUp[axis];
  }
  Pushed_.Illuminance = stands.IlluminanceLux;
  Pushed_.EyeRadiusKm = medium.BottomRadiusKm + kMediumGroundLiftKm +
                        (stands.EyeHeightM > 0.0f ? stands.EyeHeightM : 0.0f) / kMPerKmF;
  Pushed_.BottomRadiusKm = medium.BottomRadiusKm;
  Pushed_.SunHalfAngleRad = kSunHalfAngleRad;
  Pushed_.Air = medium;
  Pushed_.TopRadiusKm = medium.TopRadiusKm;
  Declared_ = true;
}

void SkyStage::Eye(const Medium &medium, float eyeHeightM) {
  if (!Declared_) { return; }
  Pushed_.EyeRadiusKm = medium.BottomRadiusKm + kMediumGroundLiftKm +
                        (eyeHeightM > 0.0f ? eyeHeightM : 0.0f) / kMPerKmF;
}

void SkyStage::SetBasis(const EyeBasis &eye) {
  const float tanHalfW = eye.TanHalfWidth;
  const float tanHalfH = eye.TanHalfHeight;
  for (int axis = 0; axis < 3; ++axis) {
    Pushed_.Right[axis] = eye.Right[axis];
    Pushed_.Up[axis] = eye.Up[axis];
    Pushed_.Fwd[axis] = eye.Forward[axis];
  }
  Pushed_.TanHalf[0] = tanHalfW;
  Pushed_.TanHalf[1] = tanHalfH;
}

Native::Result<void> SkyStage::Encode([[maybe_unused]] const FrameContext &ctx,
                                      const PassRecording &into) {
  if (!Declared_) { return {}; }
  if (!Pipe || into.Commands == nullptr) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = "sky draw is not configured"});
  }
  const std::array bindings{
      Native::Binding{.Set = 2,
                      .Slot = 0,
                      .Value =
                          Native::SampledTexture{.Image = {.Image = SkyView}, .Sampling = Lut}},
      Native::Binding{.Set = 2,
                      .Slot = 1,
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
