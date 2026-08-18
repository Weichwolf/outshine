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
  [[nodiscard]] bool Configure(const Gpu &gpu, SDL_GPUTexture *scene, SDL_GPUTexture *depth,
                               SDL_GPUSampler *exact, const DisplayOptions &options,
                               std::string &error);
  /* THE SCENE TEXTURE MAY CHANGE BETWEEN FRAMES AND THE PIPELINE MAY NOT (board:1413). A temporal
   * resolve writes into one of two `SceneLinear` textures and swaps them, so what this stage samples
   * alternates while everything about how it samples stays put -- which is a re-BIND and never a
   * reconfigure. */
  void Bind(SDL_GPUTexture *scene) { Scene = scene; }
  void Encode(const FrameContext &ctx, const PassRecording &into);

private:
  OwnedPipeline Pipe;
  SDL_GPUTexture *Scene = nullptr;
  SDL_GPUTexture *Depth = nullptr;
  SDL_GPUSampler *Exact = nullptr;
};

} // namespace outshine::Render
#endif
