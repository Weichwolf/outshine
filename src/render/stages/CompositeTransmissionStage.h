/* THE OPAQUE SCENE AND THE TRANSMISSIVE ONE PUT TOGETHER: one full-screen fragment, over-composited.
 *
 * IT EXISTS BECAUSE THE PLAN'S OWN ORDERING RULE FORBIDS THE SHORTER SHAPE (board:1386). A
 * transmissive draw has to read what stands behind it, and `TopologicalOrderHolds` refuses a stage
 * that reads a target a later stage still contributes to -- *no stage at or after `s` may produce
 * anything `s` reads*. So the glass writes its own target and this stage is what joins them.
 *
 * WHAT IT COSTS WHEN THERE IS NO GLASS IS NOTHING. `SceneComposited` aliases to `SceneHdr`, so a plan
 * that declares no transmissive draw pulls neither this stage nor its target, and the temporal
 * resolve reads the scene directly -- the same trick that keeps a picture without TAA from paying a
 * blit that exists only to copy. */
#ifndef COMPOSITETRANSMISSIONSTAGE_H
#define COMPOSITETRANSMISSIONSTAGE_H

#include <string>

#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"

namespace outshine::Render {

class CompositeTransmissionStage {
public:
  /* `opaque` is the scene as every opaque contributor left it and `transmissive` is what the glass
   * drew, PREMULTIPLIED with its coverage in alpha. `exact` is the sampler both are read through:
   * one texel per fragment at the fragment's own coordinate, so it is a fetch rather than a filter. */
  /* `target` IS THE PLAN'S FORMAT FOR THE RADIANCE THIS WRITES and is not assumed. A scene declared
   * at float precision carries 32 bits a channel; a pipeline that hardcoded 16 is refused by the
   * driver, and Metal says so by aborting the encoder rather than by failing the build (board:1386). */
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

} // namespace outshine::Render
#endif
