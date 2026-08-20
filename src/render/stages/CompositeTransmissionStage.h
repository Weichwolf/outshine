#ifndef COMPOSITETRANSMISSIONSTAGE_H
#define COMPOSITETRANSMISSIONSTAGE_H

#include <string>

#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"

namespace outshine::Render {

class CompositeTransmissionStage {
public:

  [[nodiscard]] bool Configure(const Gpu &gpu, SDL_GPUTexture *opaque, SDL_GPUTexture *transmissive,
                               SDL_GPUSampler *exact, SDL_GPUTextureFormat target,
                               std::string &error);
  void Encode(const FrameContext &ctx, const PassRecording &into);

private:
  OwnedPipeline Pipe;
  SDL_GPUTexture *Opaque = nullptr;
  SDL_GPUTexture *Transmissive = nullptr;
  SDL_GPUSampler *Exact = nullptr;
};

}
#endif
