#include "EnvironmentSpecularStage.h"
#include "EnvironmentSpecularLayout.h"
#include "IrradianceLayout.h"
#include "StagePipelines.h"
#include "math/Vec2.h"
#include "math/Vec4.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <span>
#include <utility>

namespace outshine::Render {
namespace {
struct alignas(16) Pushed {
  Medium Air;
  float CosSunZenith;
  float EyeRadiusKm;
  Vec2f Pad;
  Vec4f GroundAlbedo;
};

static_assert(sizeof(Pushed) == sizeof(Medium) + 32 && alignof(Pushed) == 16);
static_assert(offsetof(Pushed, CosSunZenith) == sizeof(Medium));
static_assert(offsetof(Pushed, EyeRadiusKm) == sizeof(Medium) + sizeof(float));
static_assert(offsetof(Pushed, GroundAlbedo) == sizeof(Medium) + 16);
}

Native::Result<void> EnvironmentSpecularStage::Configure(const Gpu &gpu, Sources from) {
  if (gpu.Device == nullptr || from.Sky == nullptr || from.Irradiance == nullptr ||
      from.Sampler == nullptr || from.Target == nullptr ||
      from.Irradiance->Description().Bytes < kIrradianceFloats * sizeof(float)) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "environment specular needs complete feeds"});
  }
  if (!Pipe_) {
    auto made = CreateComputePipeline(*gpu.Device, Shader);
    if (!made) { return std::unexpected(made.error()); }
    Pipe_ = std::move(*made);
    Cache_.Invalidate();
  }
  if (Sources_.Sky != from.Sky || Sources_.Irradiance != from.Irradiance ||
      Sources_.Sampler != from.Sampler || Sources_.Target != from.Target) {
    Cache_.Invalidate();
  }
  Sources_ = from;
  return {};
}

void EnvironmentSpecularStage::Declare(const Medium &medium,
                                       float cosSunZenith,
                                       float eyeHeightM,
                                       Vec3 groundAlbedo) {
  const Standing wanted{.Air = medium,
                        .CosSunZenith = cosSunZenith,
                        .EyeHeightM = eyeHeightM,
                        .GroundAlbedo = groundAlbedo};
  if (Cache_.Submitted() && Standing_ == wanted) { return; }
  Standing_ = wanted;
  Cache_.Invalidate();
}

Native::Result<void> EnvironmentSpecularStage::Encode(const PassRecording &into) {
  if (!Cache_.NeedsRecording()) { return {}; }
  if (!Pipe_ || !Standing_ || Sources_.Sky == nullptr || Sources_.Irradiance == nullptr ||
      Sources_.Sampler == nullptr || Sources_.Target == nullptr || into.Commands == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "environment specular is not configured"});
  }
  constexpr float metresPerKm = 1000.0f;
  Pushed pushed{.Air = Standing_->Air,
                .CosSunZenith = Standing_->CosSunZenith,
                .EyeRadiusKm = Standing_->Air.BottomRadiusKm + kMediumGroundLiftKm +
                               std::max(0.0f, Standing_->EyeHeightM) / metresPerKm,
                .Pad = {},
                .GroundAlbedo = {}};
  for (size_t axis = 0; axis < 3; ++axis) {
    pushed.GroundAlbedo[axis] = static_cast<float>(Standing_->GroundAlbedo[axis]);
  }
  const std::array bindings{
      Native::Binding{.Set = 0,
                      .Slot = 0,
                      .Value = Native::SampledTexture{.Image = {.Image = Sources_.Sky},
                                                      .Sampling = Sources_.Sampler}},
      Native::Binding{
          .Set = 0,
          .Slot = 1,
          .Value =
              Native::StorageBuffer{.Data = Sources_.Irradiance,
                                    .Range = {.Bytes = Sources_.Irradiance->Description().Bytes}}},
      Native::Binding{.Set = 1,
                      .Slot = 0,
                      .Value = Native::StorageTexture{.Image = {.Image = Sources_.Target}}},
      Native::Binding{.Set = 2,
                      .Slot = 0,
                      .Value =
                          Native::Uniform{.Bytes = std::as_bytes(std::span{&pushed, size_t{1}})}}};
  const auto encoded = into.Commands->Dispatch(
      *Pipe_,
      bindings,
      {.X = (kEnvironmentWidth + KernelShape.GroupX - 1u) / KernelShape.GroupX,
       .Y = (kEnvironmentHeight + KernelShape.GroupY - 1u) / KernelShape.GroupY,
       .Z = 1u});
  if (!encoded) { return encoded; }
  into.Submission.Record(Stage::EnvironmentSpecular, Cache_);
  return {};
}
}
