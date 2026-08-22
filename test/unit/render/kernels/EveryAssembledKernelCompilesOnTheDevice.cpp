#include <cstdio>
#include <string>

#include <SDL3/SDL.h>

#include "Check.h"

#include "CompositeTransmissionStage.h"
#include "MediumMultiScatterStage.h"
#include "MediumRadianceStage.h"
#include "MediumTransmittanceStage.h"
#include "OverlayDraw.h"
#include "PresentStage.h"
#include "SkyStage.h"

using namespace outshine::Render;

namespace {

SDL_GPUDevice *Device = nullptr;

bool Compute(const std::string &source, const char *entry) {
  SDL_GPUComputePipelineCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = entry;
  wanted.num_readwrite_storage_textures = 1u;
  wanted.num_readonly_storage_textures = 2u;
  wanted.num_uniform_buffers = 1u;
  wanted.threadcount_x = 8u;
  wanted.threadcount_y = 8u;
  wanted.threadcount_z = 1u;
  SDL_GPUComputePipeline *const made = SDL_CreateGPUComputePipeline(Device, &wanted);
  if (made == nullptr) {
    std::printf("NOTE refused %s: %s\n", entry, SDL_GetError());
    return false;
  }
  SDL_ReleaseGPUComputePipeline(Device, made);
  return true;
}

bool Shader(const std::string &source, const char *entry, SDL_GPUShaderStage stage) {
  SDL_GPUShaderCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = entry;
  wanted.stage = stage;
  wanted.num_samplers = 4u;
  wanted.num_uniform_buffers = 2u;
  SDL_GPUShader *const made = SDL_CreateGPUShader(Device, &wanted);
  if (made == nullptr) {
    std::printf("NOTE refused %s: %s\n", entry, SDL_GetError());
    return false;
  }
  SDL_ReleaseGPUShader(Device, made);
  return true;
}

bool Both(const std::string &source) {
  return Shader(source, "vs", SDL_GPU_SHADERSTAGE_VERTEX) &&
         Shader(source, "fs", SDL_GPU_SHADERSTAGE_FRAGMENT);
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  CHECK(SDL_InitSubSystem(SDL_INIT_VIDEO), "SDL video stands for the device");
  Device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, false, nullptr);
  CHECK(Device != nullptr, "a headless MSL device stands");
  if (Device == nullptr) { return Report(); }

  CHECK(Compute(MediumTransmittanceStage::KernelSource(), "mediumTransmittanceKernel"),
        "**THE TRANSMITTANCE KERNEL THE ENGINE ASSEMBLES COMPILES ON THE DEVICE** -- the text "
        "a stage builds at runtime is source the gate never saw until today, and this is the "
        "kernel the pi sweep silently broke");
  CHECK(Compute(MediumMultiScatterStage::KernelSource(), "mediumMultiScatterKernel"),
        "so does the multi-scatter kernel");
  CHECK(Compute(MediumRadianceStage::KernelSource(), "mediumRadianceKernel"),
        "and the radiance kernel");
  CHECK(Both(SkyStage::ShaderSource()), "the sky's vertex and fragment compile");
  CHECK(Both(PresentStage::ShaderSource()), "the present blit compiles");
  CHECK(Both(OverlayDraw::ShaderSource()), "the overlay compiles");
  CHECK(Both(CompositeTransmissionStage::ShaderSource()), "the transmission composite compiles");

  SDL_DestroyGPUDevice(Device);

  Covers("IV.8 every kernel and shader the engine assembles at runtime compiles on the device "
         "inside the fast gate -- an unbuildable source refuses in seconds, not in a "
         "five-minute driver run (the 1634 class); tonemap's optioned source and the subject "
         "unit's three follow in the next slice");
  return Report();
}
