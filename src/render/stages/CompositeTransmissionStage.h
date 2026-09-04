#ifndef OUTSHINE_RENDER_STAGES_COMPOSITETRANSMISSIONSTAGE_H
#define OUTSHINE_RENDER_STAGES_COMPOSITETRANSMISSIONSTAGE_H

#include <string>

#include "KernelShape.h"

#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"

namespace outshine::Render {

class CompositeTransmissionStage {
public:
  [[nodiscard]] static std::string ShaderSource();
  [[nodiscard]] static std::string ShaderSource(std::string &error);
  static constexpr DrawShape ShaderShape{.FragmentSamplers = 2};

  struct Feeds {
    SDL_GPUTexture *Opaque = nullptr;
    SDL_GPUTexture *Transmissive = nullptr;
    SDL_GPUSampler *Exact = nullptr;
    SDL_GPUTextureFormat Target{};
  };

  [[nodiscard]] bool Configure(const Gpu &gpu, const Feeds &from, std::string &error);
  void Encode(const FrameContext &ctx, const PassRecording &into);

private:
  OwnedPipeline Pipe;
  SDL_GPUTexture *Opaque = nullptr;
  SDL_GPUTexture *Transmissive = nullptr;
  SDL_GPUSampler *Exact = nullptr;
};

} // namespace outshine::Render
#endif
