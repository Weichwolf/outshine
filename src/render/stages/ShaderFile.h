#ifndef OUTSHINE_RENDER_STAGES_SHADERFILE_H
#define OUTSHINE_RENDER_STAGES_SHADERFILE_H

#include <string>
#include <string_view>
#include <SDL3/SDL_gpu.h>

namespace outshine::Render {

struct ComputeShape;
struct DrawShape;
[[nodiscard]] SDL_GPUShader *ShaderFrom(SDL_GPUDevice *device,
                                        std::string_view path,
                                        SDL_GPUShaderStage stage,
                                        const DrawShape &shape,
                                        std::string &error);
[[nodiscard]] SDL_GPUComputePipeline *ComputeFrom(SDL_GPUDevice *device,
                                                  std::string_view path,
                                                  const ComputeShape &shape,
                                                  std::string &error);

}

#endif
