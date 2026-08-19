/* THE FRAME ONTO THE SURFACE THE HOST DECLARED, and nothing else (board:1447).
 *
 * **IT IS MACHINERY**: a plan that asks for `Resource::Surface` has no path from a frame to it without
 * this, so the compiler pulls it and no consumer names it. What it does is one full-screen fragment
 * that reads `FrameTex` at its own coordinate -- a fetch and not a filter, because the two are the
 * same size by construction and any weight between them would be a resample nobody asked for.
 *
 * **THE SURFACE IS NOT THE ENGINE'S AND ITS FORMAT IS NOT KNOWN AT INIT.** A swapchain image belongs to
 * whoever owns the window and its format is the window's; the pipeline is therefore built for the
 * format the host's surface actually carries, and rebuilt if the host ever hands over a different one.
 * A pipeline that guessed would be refused by the driver, and a refused pipeline encodes nothing --
 * which reads as a black window rather than as an error. */
#ifndef PRESENTSTAGE_H
#define PRESENTSTAGE_H

#include <string>

#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"

namespace outshine::Render {

class PresentStage {
public:
  /* `frame` is what the display transfer produced and `exact` the sampler it is fetched through. */
  [[nodiscard]] bool Configure(const Gpu &gpu, SDL_GPUTexture *frame, SDL_GPUSampler *exact,
                               std::string &error);
  /* THE HOST'S FORMAT ARRIVES PER FRAME BECAUSE THE HOST'S SURFACE DOES. Building the pipeline the
   * first time a format is seen and keeping it is what makes this cost nothing on every frame after. */
  [[nodiscard]] bool For(const Gpu &gpu, SDL_GPUTextureFormat surfaceFormat, std::string &error);
  void Encode(const FrameContext &ctx, const PassRecording &into);

private:
  OwnedPipeline Pipe;
  SDL_GPUTexture *Frame = nullptr;
  SDL_GPUSampler *Exact = nullptr;
  SDL_GPUTextureFormat Built = SDL_GPU_TEXTUREFORMAT_INVALID;
};

} // namespace outshine::Render
#endif
