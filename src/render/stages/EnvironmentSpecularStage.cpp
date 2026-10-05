#include "EnvironmentSpecularStage.h"
#include "EnvironmentSpecularLayout.h"
#include "ShaderFile.h"
#include "math/Vec2.h"
#include "math/Vec4.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <string>

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

bool EnvironmentSpecularStage::Configure(const Gpu &gpu, Sources from, std::string &error) {
  if (Sources_.Sky != from.Sky || Sources_.Irradiance != from.Irradiance ||
      Sources_.Sampler != from.Sampler || Sources_.Target != from.Target) {
    Cache_.Invalidate();
  }
  Sources_ = from;
  if (from.Sky == nullptr || from.Irradiance == nullptr || from.Sampler == nullptr ||
      from.Target == nullptr) {
    error = "environment specular needs sky radiance, irradiance, sampler and target";
    return false;
  }
  if (Pipe_) { return true; }
  auto made = CreateComputePipeline(gpu.Device, Shader);
  if (!made) {
    error = std::move(made.error());
    return false;
  }
  Pipe_ = std::move(*made);
  Cache_.Invalidate();
  return true;
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

void EnvironmentSpecularStage::Encode(const PassRecording &into) {
  if (!Pipe_ || !Standing_ || !Cache_.NeedsRecording() || into.Dispatch == nullptr) { return; }
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
  SDL_PushGPUComputeUniformData(into.Commands, 0, &pushed, sizeof(pushed));
  SDL_BindGPUComputePipeline(into.Dispatch, Pipe_.Get());
  const SDL_GPUTextureSamplerBinding sampled{.texture = Sources_.Sky, .sampler = Sources_.Sampler};
  SDL_BindGPUComputeSamplers(into.Dispatch, 0, &sampled, 1);
  const std::array<SDL_GPUBuffer *, 1> buffers{Sources_.Irradiance};
  SDL_BindGPUComputeStorageBuffers(into.Dispatch, 0, buffers.data(), 1);
  SDL_DispatchGPUCompute(into.Dispatch,
                         (kEnvironmentWidth + KernelShape.GroupX - 1u) / KernelShape.GroupX,
                         (kEnvironmentHeight + KernelShape.GroupY - 1u) / KernelShape.GroupY,
                         1u);
  into.Submission.Record(Stage::EnvironmentSpecular, Cache_);
}
}
