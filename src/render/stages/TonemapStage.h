#ifndef OUTSHINE_RENDER_STAGES_TONEMAPSTAGE_H
#define OUTSHINE_RENDER_STAGES_TONEMAPSTAGE_H

#include <string>

#include "Extent.h"
#include "math/Vec2.h"
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

  [[nodiscard]] bool Configure(const Gpu &gpu,
                               SDL_GPUTexture *scene,
                               SDL_GPUTexture *depth,
                               SDL_GPUSampler *exact,
                               SDL_GPUTextureFormat linear,
                               const DisplayOptions &options,
                               std::string &error);

  void Bind(SDL_GPUTexture *scene) { Scene = scene; }

  struct Temporal {
    SDL_GPUTexture *History = nullptr;
    SDL_GPUTexture *Velocity = nullptr;
  };

  void BindTemporal(Temporal from, Extent frame, const Vec2f &jitterDelta, bool historyHeld) {
    const int width = frame.WidthPx;
    const int height = frame.HeightPx;
    History = from.History;
    Velocity = from.Velocity;
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
  Vec2f JitterDelta = {{0.0f, 0.0f}};
  SDL_GPUSampler *Exact = nullptr;
};

} // namespace outshine::Render
#endif
