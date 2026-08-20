#ifndef SCENETARGETS_H
#define SCENETARGETS_H

#include <SDL3/SDL_gpu.h>

namespace outshine::Render {

inline constexpr SDL_GPUTextureFormat kVelocityFormat = SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT;

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

}
#endif
