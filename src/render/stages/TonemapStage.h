#ifndef TONEMAPSTAGE_H
#define TONEMAPSTAGE_H

#include <string>

#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"
#include "KernelShape.h"
#include "Resolve.h"

namespace outshine::Render {

class TonemapStage {
public:
  [[nodiscard]] static std::string ShaderSource(const DisplayOptions &options);
  [[nodiscard]] static std::string ShaderSource(const DisplayOptions &options, std::string &error);
  static constexpr DrawShape ShaderShape{.FragmentSamplers = 2};
  static constexpr DrawShape TemporalShaderShape{.FragmentSamplers = 4,
                                                 .FragmentUniformBuffers = 1};

  [[nodiscard]] bool Configure(const Gpu &gpu, SDL_GPUTexture *scene, SDL_GPUTexture *depth,
                               SDL_GPUSampler *exact, SDL_GPUTextureFormat linear,
                               const DisplayOptions &options, std::string &error);

  void Bind(SDL_GPUTexture *scene) { Scene = scene; }

  void BindTemporal(SDL_GPUTexture *history, SDL_GPUTexture *velocity, int width, int height,
                    const float jitterDelta[2], bool historyHeld) {
    History = history;
    Velocity = velocity;
    Width = width;
    Height = height;
    JitterDelta[0] = jitterDelta[0];
    JitterDelta[1] = jitterDelta[1];
    HistoryHeld = historyHeld;
  }
  void Encode(const FrameContext &ctx, const PassRecording &into);

private:
  OwnedPipeline Pipe;
  SDL_GPUTexture *Scene = nullptr;
  SDL_GPUTexture *Depth = nullptr;
  SDL_GPUTexture *History = nullptr;
  SDL_GPUTexture *Velocity = nullptr;
  bool Temporal = false;
  bool HistoryHeld = false;
  int Width = 0, Height = 0;
  float JitterDelta[2] = {0.0f, 0.0f};
  SDL_GPUSampler *Exact = nullptr;
};

}
#endif
