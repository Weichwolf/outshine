#ifndef MEDIUMMULTISCATTERSTAGE_H
#define MEDIUMMULTISCATTERSTAGE_H

#include <string>

#include "Gpu.h"
#include "GpuOwned.h"
#include "ParticipatingMedium.h"

namespace outshine::Render {

class MediumMultiScatterStage {
public:
  [[nodiscard]] bool Configure(const Gpu &gpu, SDL_GPUTexture *transmittance, SDL_GPUSampler *lut,
                               SDL_GPUTexture *into, std::string &error);

  void Declare(const Medium &medium);

  void Encode(const PassRecording &into);

  [[nodiscard]] bool Settled(void) const { return Settled_; }

private:
  OwnedComputePipeline Pipe;
  SDL_GPUTexture *Transmittance = nullptr;
  SDL_GPUSampler *Lut = nullptr;
  SDL_GPUTexture *Into = nullptr;
  Medium Declared_;
  bool Settled_ = false;
};

}
#endif
