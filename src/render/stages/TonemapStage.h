/* THE DISPLAY TRANSFER ON ITS OWN: one full-screen fragment from the resolved linear radiance to the
 * frame the picture is read off. It is MACHINERY -- without it a plan that asks for a picture has no
 * path from radiance to a frame at all -- so the compiler pulls it and no declaration names it.
 *
 * WHAT IT READS IS WHAT THE PLAN BOUND in `sceneLinear`'s place, which in a plan with no temporal
 * resolve is the scene target itself through the plan's own alias -- so the picture costs no
 * full-screen blit that exists only to copy. */
#ifndef TONEMAPSTAGE_H
#define TONEMAPSTAGE_H

#include <string>

#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"
#include "Resolve.h"

namespace outshine::Render {

class TonemapStage {
public:
  /* `scene` is the linear radiance and `depth` the attachment whose value is the frame's coverage.
   * `exact` is the sampler both are read through: the transfer takes one texel per fragment at the
   * fragment's own coordinate, so it is a fetch and not a filter -- but SDL_GPU pairs every sampled
   * texture with a sampler, so the pair is what a fetch is spelled as here. */
  /* `linear` IS THE PLAN'S FORMAT FOR `SceneLinear` and is read only where the resolve is fused in:
   * a pipeline that hardcoded a width would be refused by the driver, and a refused pipeline encodes
   * nothing, which reads as a black frame rather than as an error. */
  [[nodiscard]] bool Configure(const Gpu &gpu, SDL_GPUTexture *scene, SDL_GPUTexture *depth,
                               SDL_GPUSampler *exact, SDL_GPUTextureFormat linear,
                               const DisplayOptions &options, std::string &error);
  /* THE SCENE TEXTURE MAY CHANGE BETWEEN FRAMES AND THE PIPELINE MAY NOT (board:1413). A temporal
   * resolve writes into one of two `SceneLinear` textures and swaps them, so what this stage samples
   * alternates while everything about how it samples stays put -- which is a re-BIND and never a
   * reconfigure. */
  void Bind(SDL_GPUTexture *scene) { Scene = scene; }
  /* WHAT THE FUSED RESOLVE NEEDS AND THE TRANSFER DOES NOT (board:1413), handed in per frame because
   * every one of them changes per frame: the history swaps, the offset advances, and whether there
   * IS a past is a fact about where the run began. */
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

} // namespace outshine::Render
#endif
