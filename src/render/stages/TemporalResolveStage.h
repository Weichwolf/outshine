/* THE TEMPORAL RESOLVE THE PLAN HAS ALWAYS DECLARED (board:1413). One full-screen fragment: the
 * current frame, the previous frame reprojected through the velocity the geometry pass wrote, and a
 * clamp that says how far the past is allowed to disagree with the present.
 *
 * WHAT MAKES IT ANTI-ALIASING RATHER THAN MERELY STABILITY IS THE JITTER, and the jitter is not here.
 * It lives in the projection, because that is the only place a sub-pixel offset can move the
 * RASTERISATION -- this stage would otherwise average many frames of the same samples and return the
 * aliased picture, smoother and no sharper. `Renderer` owns the sequence and hands the offset in.
 *
 * THE HISTORY IS THE PREVIOUS FRAME'S OUTPUT AND IT IS NOT A PLAN EDGE. A read of what a later frame
 * produced is a cycle in a per-frame graph, so `SceneLinear` is allocated twice and the two are
 * swapped -- no copy, no blit, and `SettleFrames_` already existed for exactly the frames in which
 * there is nothing to read yet.
 *
 * THE VELOCITY CARRIES THE JITTER AND IT MUST NOT. Both view-projections are jittered by their own
 * frame's offset, so what the geometry pass wrote differs from the true motion by exactly
 * `jitterNow - jitterPrevious`. That is a constant over the frame and it is subtracted here rather
 * than in the geometry shader, where it would cost a second matrix in the vertex path. */
#ifndef TEMPORALRESOLVESTAGE_H
#define TEMPORALRESOLVESTAGE_H

#include <string>

#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"

namespace outshine::Render {

class TemporalResolveStage {
public:
  /* `current` is the scene as every contributor left it, `history` the texture the previous frame's
   * resolve wrote, `velocity` what the geometry pass recorded and `exact` the sampler the first three
   * are fetched through. `smooth` is the filtered one, and it is used for the history alone: the
   * reprojected coordinate falls between texels by construction, and a nearest fetch there is the
   * whole of what makes a reprojection shimmer. */
  /* `target` IS THE PLAN'S FORMAT FOR `SceneLinear` AND IS NOT ASSUMED. A scene declared at float
   * precision carries 32 bits a channel and a pipeline that hardcoded 16 is refused by the driver --
   * [MEASURED] as *every declared arm rendered* going red with no other symptom, because a pipeline
   * that will not build encodes nothing and a pass with nothing in it is a black frame rather than
   * an error. */
  [[nodiscard]] bool Configure(const Gpu &gpu, SDL_GPUTextureFormat target, SDL_GPUSampler *exact,
                               SDL_GPUSampler *smooth, std::string &error);
  /* THE SIZE COMES IN HERE AND NOT THROUGH `FrameContext`, which carries the camera and nothing about
   * the frame's shape: a field added there for this one stage would be a field every stage reads past. */
  void Bind(SDL_GPUTexture *current, SDL_GPUTexture *history, SDL_GPUTexture *velocity, int width,
            int height);
  /* `jitterDelta` is `jitterNow - jitterPrevious` in PIXELS, and `historyHeld` says whether the
   * previous frame produced anything -- on the first frame of a run there is no past and the answer
   * is the present, stated rather than blended towards a cleared texture. */
  void Encode(const PassRecording &into, const float jitterDelta[2], bool historyHeld);

private:
  OwnedPipeline Pipe;
  SDL_GPUTexture *Current = nullptr;
  SDL_GPUTexture *History = nullptr;
  SDL_GPUTexture *Velocity = nullptr;
  SDL_GPUSampler *Exact = nullptr;
  SDL_GPUSampler *Smooth = nullptr;
  int Width = 0;
  int Height = 0;
};

} // namespace outshine::Render
#endif
