#include "MediumMultiScatterStage.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "ShaderFile.h"
#include <utility>

namespace outshine::Render {

bool MediumMultiScatterStage::Configure(const Gpu &gpu,
                                        SDL_GPUTexture *transmittance,
                                        SDL_GPUSampler *lut,
                                        SDL_GPUTexture *into,
                                        std::string &error) {
  if (Into != into || Transmittance != transmittance) { Cache_.Invalidate(); }
  Transmittance = transmittance;
  Lut = lut;
  Into = into;
  if (Transmittance == nullptr || Lut == nullptr || Into == nullptr) {
    error = "the multiple scattering table needs the transmittance table, its sampler and a "
            "table of its own, and the plan did not hold all three";
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

void MediumMultiScatterStage::Declare(const Medium &medium) {
  if (Cache_.Submitted() && Declared_ == medium) { return; }
  Declared_ = medium;
  Cache_.Invalidate();
}

void MediumMultiScatterStage::Encode(const PassRecording &into) {
  if (!Pipe || !Cache_.NeedsRecording() || into.Dispatch == nullptr) { return; }
  SDL_PushGPUComputeUniformData(
      into.Commands, 0, &Declared_, static_cast<uint32_t>(sizeof Declared_));
  SDL_BindGPUComputePipeline(into.Dispatch, Pipe.Get());
  const SDL_GPUTextureSamplerBinding bound{.texture = Transmittance, .sampler = Lut};
  SDL_BindGPUComputeSamplers(into.Dispatch, 0, &bound, 1);
  SDL_DispatchGPUCompute(into.Dispatch,
                         (kMultiScatterLutSize + KernelShape.GroupX - 1u) / KernelShape.GroupX,
                         (kMultiScatterLutSize + KernelShape.GroupY - 1u) / KernelShape.GroupY,
                         1u);
  into.Submission.Record(Stage::MediumMultiScatter, Cache_);
}

}
