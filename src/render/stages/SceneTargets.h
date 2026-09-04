#ifndef OUTSHINE_RENDER_STAGES_SCENETARGETS_H
#define OUTSHINE_RENDER_STAGES_SCENETARGETS_H

#include <array>
#include <cstdio>
#include <string>

#include "ShaderFile.h"

#include <SDL3/SDL_gpu.h>

namespace outshine::Render {

constexpr size_t kDefineBytes = 48;

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
  std::array<char, kDefineBytes> made{};
  std::snprintf(made.data(),
                made.size(),
                "#define VELOCITY_STATIC %.9ef\n",
                static_cast<double>(kVelocityStatic));
  return {made.data()};
}

inline ShaderText &VelocityStatic(ShaderText &into) {
  return into.Reads("src/render/shaders/velocityStatic.msl");
}

[[nodiscard]] inline std::string VelocityStaticMsl(std::string &error) {
  ShaderText source;
  return VelocityStatic(source).Take(error);
}

} // namespace outshine::Render
#endif
