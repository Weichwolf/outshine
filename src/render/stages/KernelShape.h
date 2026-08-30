#ifndef OUTSHINE_RENDER_STAGES_KERNELSHAPE_H
#define OUTSHINE_RENDER_STAGES_KERNELSHAPE_H

#include <SDL3/SDL_gpu.h>

#include <cstdint>
#include <string_view>

namespace outshine::Render {

struct ComputeShape {
  uint32_t Samplers = 0;
  uint32_t ReadOnlyTextures = 0;
  uint32_t ReadWriteTextures = 0;

  uint32_t ReadOnlyBuffers = 0;
  uint32_t ReadWriteBuffers = 0;
  uint32_t UniformBuffers = 0;
  uint32_t GroupX = 1;
  uint32_t GroupY = 1;
  uint32_t GroupZ = 1;
};

struct DrawShape {
  uint32_t VertexSamplers = 0;
  uint32_t VertexUniformBuffers = 0;
  uint32_t VertexStorageBuffers = 0;
  uint32_t FragmentSamplers = 0;
  uint32_t FragmentUniformBuffers = 0;
  uint32_t FragmentStorageBuffers = 0;
};

[[nodiscard]] inline SDL_GPUShader *ShaderFrom(SDL_GPUDevice *device,
                                               std::string_view source,
                                               const char *entry,
                                               SDL_GPUShaderStage stage,
                                               const DrawShape &shape) {
  const bool fragment = stage == SDL_GPU_SHADERSTAGE_FRAGMENT;
  SDL_GPUShaderCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.data());
  wanted.code_size = source.size();
  wanted.entrypoint = entry;
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.stage = stage;
  wanted.num_samplers = fragment ? shape.FragmentSamplers : shape.VertexSamplers;
  wanted.num_storage_buffers = fragment ? shape.FragmentStorageBuffers : shape.VertexStorageBuffers;
  wanted.num_uniform_buffers = fragment ? shape.FragmentUniformBuffers : shape.VertexUniformBuffers;
  return SDL_CreateGPUShader(device, &wanted);
}

} // namespace outshine::Render

#endif
