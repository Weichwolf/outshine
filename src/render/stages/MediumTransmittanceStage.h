#ifndef MEDIUMTRANSMITTANCESTAGE_H
#define MEDIUMTRANSMITTANCESTAGE_H

#include <string>

#include "Gpu.h"
#include "GpuOwned.h"
#include "ParticipatingMedium.h"

namespace outshine::Render {

class MediumTransmittanceStage {
public:
  [[nodiscard]] static std::string KernelSource(void);
  [[nodiscard]] bool Configure(const Gpu &gpu, SDL_GPUTexture *lut, std::string &error);

  void Declare(const Medium &medium);

  void Encode(const PassRecording &into);

  [[nodiscard]] bool Settled(void) const { return Settled_; }

private:
  OwnedComputePipeline Pipe;
  SDL_GPUTexture *Lut = nullptr;
  Medium Declared_;
  bool Settled_ = false;
};

}
#endif
