#ifndef OUTSHINE_RENDER_STAGES_SCENETARGETS_H
#define OUTSHINE_RENDER_STAGES_SCENETARGETS_H

#include <SDL3/SDL_gpu.h>

namespace outshine::Render {

inline constexpr SDL_GPUTextureFormat kVelocityFormat = SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT;

#define SCENE_FLOAT(name, value) inline constexpr float name = value;
#include "SceneConstants.inc"
#undef SCENE_FLOAT

inline SDL_GPUColorTargetDescription VelocityTarget(bool writes) {
  SDL_GPUColorTargetDescription target{};
  target.format = kVelocityFormat;
  target.blend_state.enable_color_write_mask = true;
  target.blend_state.color_write_mask = writes
                                            ? (SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G |
                                               SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A)
                                            : 0;
  return target;
}

} // namespace outshine::Render
#endif
