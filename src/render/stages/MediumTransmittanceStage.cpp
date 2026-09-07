#include "MediumTransmittanceStage.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "ShaderFile.h"

namespace outshine::Render {

bool MediumTransmittanceStage::Configure(const Gpu &gpu, SDL_GPUTexture *lut, std::string &error) {
  if (Lut != lut) { Settled_ = false; }
  Lut = lut;
  if (Lut == nullptr) {
    error = "the plan holds no transmittance table for this stage to write, so nothing downstream "
            "could read one";
    return false;
  }
  if (Pipe) { return true; }

  SDL_GPUComputePipeline *const made =
      ComputeFrom(gpu.Device, "build/shaders/mediumTransmittance.comp.spv", KernelShape, error);
  if (made == nullptr) { return false; }
  Pipe = OwnedComputePipeline(gpu.Device, made);
  Settled_ = false;
  return true;
}

void MediumTransmittanceStage::Declare(const Medium &medium) {
  if (Settled_ && Declared_ == medium) { return; }
  Declared_ = medium;
  Settled_ = false;
}

void MediumTransmittanceStage::Encode(const PassRecording &into) {
  if (!Pipe || Settled_ || into.Dispatch == nullptr) { return; }
  SDL_PushGPUComputeUniformData(
      into.Commands, 0, &Declared_, static_cast<uint32_t>(sizeof Declared_));
  SDL_BindGPUComputePipeline(into.Dispatch, Pipe.Get());
  SDL_DispatchGPUCompute(into.Dispatch,
                         (kTransmittanceLutWidth + KernelShape.GroupX - 1u) / KernelShape.GroupX,
                         (kTransmittanceLutHeight + KernelShape.GroupY - 1u) / KernelShape.GroupY,
                         1u);
  Settled_ = true;
}

} // namespace outshine::Render
