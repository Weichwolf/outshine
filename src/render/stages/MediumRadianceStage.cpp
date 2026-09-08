#include "math/Vec2.h"
#include "MediumRadianceStage.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "ShaderFile.h"

namespace outshine::Render {

constexpr float kMPerKmF = 1000.0f;

namespace {

struct Pushed {
  Medium Declared;
  float CosSunZenith;
  float EyeRadiusKm;
  Vec2f Pad;
};

static_assert(sizeof(Pushed) == sizeof(Medium) + 16, "the push keeps the medium's alignment");

}

bool MediumRadianceStage::Configure(const Gpu &gpu,
                                    SDL_GPUTexture *transmittance,
                                    SDL_GPUTexture *multiScatter,
                                    SDL_GPUSampler *lut,
                                    SDL_GPUTexture *into,
                                    std::string &error) {
  if (Into != into || Transmittance != transmittance || MultiScatter != multiScatter) {
    Settled_ = false;
  }
  Transmittance = transmittance;
  MultiScatter = multiScatter;
  Lut = lut;
  Into = into;
  if (Transmittance == nullptr || MultiScatter == nullptr || Lut == nullptr || Into == nullptr) {
    error = "the sky view needs both medium tables, their sampler and a table of its own, and the "
            "plan did not hold all four";
    return false;
  }
  if (Pipe) { return true; }

  SDL_GPUComputePipeline *const made =
      ComputeFrom(gpu.Device, "build/shaders/mediumRadiance.comp.spv", KernelShape, error);
  if (made == nullptr) { return false; }
  Pipe = OwnedComputePipeline(gpu.Device, made);
  Settled_ = false;
  return true;
}

void MediumRadianceStage::Declare(const Medium &medium, float cosSunZenith, float eyeHeightM) {
  const Standing wanted = {
      .Declared = medium, .CosSunZenith = cosSunZenith, .EyeHeightM = eyeHeightM};
  if (Settled_ && Standing_ == wanted) { return; }
  Standing_ = wanted;
  Settled_ = false;
}

void MediumRadianceStage::Encode(const PassRecording &into) {
  if (!Pipe || Settled_ || into.Dispatch == nullptr) { return; }
  Pushed pushed{};
  pushed.Declared = Standing_.Declared;
  pushed.CosSunZenith = Standing_.CosSunZenith;
  pushed.EyeRadiusKm = Standing_.Declared.BottomRadiusKm + kMediumGroundLiftKm +
                       std::fmax(0.0f, Standing_.EyeHeightM) / kMPerKmF;
  SDL_PushGPUComputeUniformData(into.Commands, 0, &pushed, static_cast<uint32_t>(sizeof pushed));
  SDL_BindGPUComputePipeline(into.Dispatch, Pipe.Get());
  std::array<SDL_GPUTextureSamplerBinding, 2> bound = {
      {{.texture = Transmittance, .sampler = Lut}, {.texture = MultiScatter, .sampler = Lut}}};
  SDL_BindGPUComputeSamplers(into.Dispatch, 0, bound.data(), 2);
  SDL_DispatchGPUCompute(into.Dispatch,
                         (kSkyViewLutWidth + KernelShape.GroupX - 1u) / KernelShape.GroupX,
                         (kSkyViewLutHeight + KernelShape.GroupY - 1u) / KernelShape.GroupY,
                         1u);
  Settled_ = true;
}

}
