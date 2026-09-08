#include "MediumTransmittanceStage.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "ShaderFile.h"
#include <utility>

namespace outshine::Render {

bool MediumTransmittanceStage::Configure(const Gpu &gpu, SDL_GPUTexture *lut, std::string &error) {
  if (Lut != lut) { Cache_.Invalidate(); }
  Lut = lut;
  if (Lut == nullptr) {
    error = "the plan holds no transmittance table for this stage to write, so nothing downstream "
            "could read one";
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

void MediumTransmittanceStage::Declare(const Medium &medium) {
  if (Cache_.Submitted() && Declared_ == medium) { return; }
  Declared_ = medium;
  Cache_.Invalidate();
}

void MediumTransmittanceStage::Encode(const PassRecording &into) {
  if (!Pipe || !Cache_.NeedsRecording() || into.Dispatch == nullptr) { return; }
  SDL_PushGPUComputeUniformData(
      into.Commands, 0, &Declared_, static_cast<uint32_t>(sizeof Declared_));
  SDL_BindGPUComputePipeline(into.Dispatch, Pipe.Get());
  SDL_DispatchGPUCompute(into.Dispatch,
                         (kTransmittanceLutWidth + KernelShape.GroupX - 1u) / KernelShape.GroupX,
                         (kTransmittanceLutHeight + KernelShape.GroupY - 1u) / KernelShape.GroupY,
                         1u);
  into.Submission.Record(Stage::MediumTransmittance, Cache_);
}

}
