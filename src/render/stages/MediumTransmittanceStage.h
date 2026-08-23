#ifndef OUTSHINE_RENDER_STAGES_MEDIUMTRANSMITTANCESTAGE_H
#define OUTSHINE_RENDER_STAGES_MEDIUMTRANSMITTANCESTAGE_H

#include <string>

#include "KernelShape.h"

#include "Gpu.h"
#include "GpuOwned.h"
#include "ParticipatingMedium.h"

namespace outshine::Render {

class MediumTransmittanceStage {
public:
  [[nodiscard]] static std::string KernelSource();
  [[nodiscard]] static std::string KernelSource(std::string &error);
  static constexpr ComputeShape KernelShape{
      .ReadWriteTextures = 1, .UniformBuffers = 1, .GroupX = 8, .GroupY = 8};
  [[nodiscard]] bool Configure(const Gpu &gpu, SDL_GPUTexture *lut, std::string &error);

  void Declare(const Medium &medium);

  void Encode(const PassRecording &into);

  [[nodiscard]] bool Settled() const { return Settled_; }

private:
  OwnedComputePipeline Pipe;
  SDL_GPUTexture *Lut = nullptr;
  Medium Declared_;
  bool Settled_ = false;
};

}
#endif
