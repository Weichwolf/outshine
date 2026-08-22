#include <cstdio>
#include <string>

#include <SDL3/SDL.h>

#include "Check.h"

#include "CompositeTransmissionStage.h"
#include "KernelShape.h"
#include "MediumMultiScatterStage.h"
#include "MediumRadianceStage.h"
#include "MediumTransmittanceStage.h"
#include "OverlayDraw.h"
#include "PresentStage.h"
#include "SkyStage.h"

using namespace outshine::Render;

namespace {

SDL_GPUDevice *Device = nullptr;

bool Compute(const std::string &source, const char *entry, const ComputeShape &shape) {
  SDL_GPUComputePipelineCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = entry;
  wanted.num_samplers = shape.Samplers;
  wanted.num_readonly_storage_textures = shape.ReadOnlyTextures;
  wanted.num_readwrite_storage_textures = shape.ReadWriteTextures;
  wanted.num_uniform_buffers = shape.UniformBuffers;
  wanted.threadcount_x = shape.GroupX;
  wanted.threadcount_y = shape.GroupY;
  wanted.threadcount_z = shape.GroupZ;
  SDL_GPUComputePipeline *const made = SDL_CreateGPUComputePipeline(Device, &wanted);
  if (made == nullptr) {
    std::printf("NOTE refused %s: %s\n", entry, SDL_GetError());
    return false;
  }
  SDL_ReleaseGPUComputePipeline(Device, made);
  return true;
}

bool Shader(const std::string &source, const char *entry, SDL_GPUShaderStage stage,
            uint32_t samplers, uint32_t uniforms) {
  SDL_GPUShaderCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = entry;
  wanted.stage = stage;
  wanted.num_samplers = samplers;
  wanted.num_uniform_buffers = uniforms;
  SDL_GPUShader *const made = SDL_CreateGPUShader(Device, &wanted);
  if (made == nullptr) {
    std::printf("NOTE refused %s: %s\n", entry, SDL_GetError());
    return false;
  }
  SDL_ReleaseGPUShader(Device, made);
  return true;
}

bool Both(const std::string &source, const DrawShape &shape) {
  return Shader(source, "vs", SDL_GPU_SHADERSTAGE_VERTEX, shape.VertexSamplers,
                shape.VertexUniformBuffers) &&
         Shader(source, "fs", SDL_GPU_SHADERSTAGE_FRAGMENT, shape.FragmentSamplers,
                shape.FragmentUniformBuffers);
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  CHECK(SDL_InitSubSystem(SDL_INIT_VIDEO), "SDL video stands for the device");
  Device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, false, nullptr);
  CHECK(Device != nullptr, "a headless MSL device stands");
  if (Device == nullptr) { return Report(); }

  CHECK(Compute(MediumTransmittanceStage::KernelSource(), "mediumTransmittanceKernel", MediumTransmittanceStage::KernelShape),
        "**THE TRANSMITTANCE KERNEL THE ENGINE ASSEMBLES COMPILES ON THE DEVICE** -- the text "
        "a stage builds at runtime compiles in the SHAPE the stage declares -- source and "
        "binding counts are the stage's own statics, so neither can drift unseen");
  CHECK(Compute(MediumMultiScatterStage::KernelSource(), "mediumMultiScatterKernel", MediumMultiScatterStage::KernelShape),
        "so does the multi-scatter kernel");
  CHECK(Compute(MediumRadianceStage::KernelSource(), "mediumRadianceKernel", MediumRadianceStage::KernelShape),
        "and the radiance kernel");
  CHECK(Both(SkyStage::ShaderSource(), SkyStage::ShaderShape), "the sky's vertex and fragment compile");
  CHECK(Both(PresentStage::ShaderSource(), PresentStage::ShaderShape), "the present blit compiles");
  CHECK(Both(OverlayDraw::ShaderSource(), OverlayDraw::ShaderShape), "the overlay compiles");
  CHECK(Both(CompositeTransmissionStage::ShaderSource(), CompositeTransmissionStage::ShaderShape), "the transmission composite compiles");

  SDL_DestroyGPUDevice(Device);

  Covers("IV.8 every kernel and shader the engine assembles at runtime compiles on the device "
         "inside the fast gate -- an unbuildable source refuses in seconds, not in a "
         "five-minute driver run (the 1634 class); tonemap's optioned source and the subject "
         "unit's three follow in the next slice");
  return Report();
}
