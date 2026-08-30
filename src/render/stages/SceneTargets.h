#ifndef OUTSHINE_RENDER_STAGES_SCENETARGETS_H
#define OUTSHINE_RENDER_STAGES_SCENETARGETS_H

#include <cstdio>
#include <string>

#include "ShaderFile.h"

#include <SDL3/SDL_gpu.h>

namespace outshine::Render {

inline constexpr SDL_GPUTextureFormat kVelocityFormat = SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT;

inline constexpr float kVelocityStatic = -1.0e4f;

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

[[nodiscard]] inline std::string VelocityStaticDefine() {
  char made[48];
  std::snprintf(made, sizeof made, "#define VELOCITY_STATIC %.9ef\n", (double)kVelocityStatic);
  return std::string(made);
}

[[nodiscard]] inline std::string VelocityStaticMsl(std::string &error) {
  std::string held;
  if (!LoadShaderText("src/render/shaders/velocityStatic.msl", held, error)) {
    return std::string();
  }
  return held;
}

}
#endif
