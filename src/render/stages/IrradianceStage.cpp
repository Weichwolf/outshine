#include "math/Vec2.h"
#include "IrradianceStage.h"

#include <array>
#include <cstdint>
#include <string>

#include <cstdio>
#include <cstring>

#include "ShaderFile.h"
#include <utility>

namespace outshine::Render {

namespace {

struct Pushed {
  Medium Declared;
  float CosSunZenith = 0.0f;
  float GroundRadiusKm = 0.0f;
  Vec2f Pad = {{0.0f, 0.0f}};
};

static_assert(sizeof(Pushed) == sizeof(Medium) + 16, "the push keeps the medium's alignment");

}

bool IrradianceStage::Configure(const Gpu &gpu,
                                SDL_GPUTexture *transmittance,
                                SDL_GPUTexture *multiScatter,
                                SDL_GPUSampler *lut,
                                SDL_GPUBuffer *into,
                                std::string &error) {
  if (Into != into || Transmittance != transmittance || MultiScatter != multiScatter) {
    Cache_.Invalidate();
  }
  Transmittance = transmittance;
  MultiScatter = multiScatter;
  Lut = lut;
  Into = into;
  if (Transmittance == nullptr || MultiScatter == nullptr || Lut == nullptr || Into == nullptr) {
    error = "the sky's irradiance needs both medium tables, their sampler and a buffer of its own, "
            "and the plan did not hold all four";
    return false;
  }
  if (Pipe) { return true; }

  auto made = CreateComputePipeline(gpu.Device, Shader);
  if (!made) {
    error = std::move(made.error());
    return false;
  }
  Pipe = std::move(*made);
  Cache_.Invalidate();
  return true;
}

void IrradianceStage::Declare(const Medium &medium, float cosSunZenith) {
  const Standing wanted = {.Declared = medium, .CosSunZenith = cosSunZenith};
  if (Cache_.Submitted() && Standing_ == wanted) { return; }
  Standing_ = wanted;
  Cache_.Invalidate();
}

void IrradianceStage::Encode(const PassRecording &into) {
  if (!Pipe || !Cache_.NeedsRecording() || into.Dispatch == nullptr) { return; }
  Pushed pushed{};
  pushed.Declared = Standing_.Declared;
  pushed.CosSunZenith = Standing_.CosSunZenith;
  pushed.GroundRadiusKm = Standing_.Declared.BottomRadiusKm + kMediumGroundLiftKm;
  SDL_PushGPUComputeUniformData(into.Commands, 0, &pushed, static_cast<uint32_t>(sizeof pushed));
  SDL_BindGPUComputePipeline(into.Dispatch, Pipe.Get());
  const std::array<SDL_GPUTextureSamplerBinding, 2> bound = {
      {{.texture = Transmittance, .sampler = Lut}, {.texture = MultiScatter, .sampler = Lut}}};
  SDL_BindGPUComputeSamplers(into.Dispatch, 0, bound.data(), 2);
  SDL_DispatchGPUCompute(into.Dispatch, 1u, 1u, 1u);
  into.Submission.Record(Stage::Irradiance, Cache_);
}

}
