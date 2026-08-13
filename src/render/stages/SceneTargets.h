/* THE SCENE PASS HAS TWO ATTACHMENTS and that is a contract, like the pass topology above it: linear
 * radiance, and the screen-space MOTION of whatever wrote the depth. Every pipeline recorded into
 * that pass must declare both targets whatever it actually writes, so the second one is described
 * here once instead of in every stage that draws into the scene.
 *
 * The clear value IS a statement: "nothing dynamic wrote this pixel". Nothing in this tree reads the
 * attachment yet -- the temporal resolve that did is gone with the port -- so what stands here is
 * the format, the sentinel and the write mask, and no consumer of them. */
#ifndef SCENETARGETS_H
#define SCENETARGETS_H

#include <SDL3/SDL_gpu.h>

namespace outshine::Render {

inline constexpr SDL_GPUTextureFormat kVelocityFormat = SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT;
/* NDC motion is bounded by 2 in each axis; -1e4 is unreachable and exactly representable in f16. */
inline constexpr float kVelocityStatic = -1.0e4f;

inline SDL_GPUColorTargetDescription VelocityTarget(bool writes) {
  SDL_GPUColorTargetDescription target{};
  target.format = kVelocityFormat;
  target.blend_state.enable_color_write_mask = true;
  target.blend_state.color_write_mask =
      writes ? (SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B |
                SDL_GPU_COLORCOMPONENT_A)
             : 0;
  return target;
}

static const char *kVelocityMsl = R"(
constant float kVelStatic = -1.0e4;
)";

} // namespace outshine::Render
#endif
