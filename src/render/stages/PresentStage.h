#ifndef PRESENTSTAGE_H
#define PRESENTSTAGE_H

#include <string>

#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"

namespace outshine::Render {

class PresentStage {
public:

  [[nodiscard]] bool Configure(const Gpu &gpu, SDL_GPUTexture *frame, SDL_GPUSampler *exact,
                               std::string &error);

  [[nodiscard]] bool For(const Gpu &gpu, SDL_GPUTextureFormat surfaceFormat, std::string &error);
  void Encode(const FrameContext &ctx, const PassRecording &into);

private:
  OwnedPipeline Pipe;
  SDL_GPUTexture *Frame = nullptr;
  SDL_GPUSampler *Exact = nullptr;
  SDL_GPUTextureFormat Built = SDL_GPU_TEXTUREFORMAT_INVALID;
};

}
#endif
