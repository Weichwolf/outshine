#include "MediumMultiScatterStage.h"

#include <cstdio>
#include <cstring>

#include "ShaderFile.h"
#include "ShaderPrelude.h"

namespace outshine::Render {
namespace {


std::string Kernel(std::string &error) {
  char declared[512];
  std::snprintf(declared, sizeof declared,
                "constant uint kMultiScatterSteps = %uu;\n"
                "constant uint kMultiScatterGrid = %uu;\n"
                "constant float kMediumLuminanceSegment = %.9g;\n"
                "constant float kMediumGroundLiftKm = %.9g;\n",
                (unsigned)kMultiScatterSteps, (unsigned)kMultiScatterGrid,
                (double)kMediumLuminanceSegment, (double)kMediumGroundLiftKm);
  std::string core;
  std::string body;
  if (!ParticipatingMediumMsl(core, error) ||
      !LoadShaderText("src/render/shaders/mediumMultiScatter.msl", body, error)) {
    return std::string();
  }
  return MslPrelude(error) + declared + core + body;
}

}

bool MediumMultiScatterStage::Configure(const Gpu &gpu, SDL_GPUTexture *transmittance,
                                        SDL_GPUSampler *lut, SDL_GPUTexture *into,
                                        std::string &error) {
  if (Into != into || Transmittance != transmittance) { Settled_ = false; }
  Transmittance = transmittance;
  Lut = lut;
  Into = into;
  if (Transmittance == nullptr || Lut == nullptr || Into == nullptr) {
    error = "the multiple scattering table needs the transmittance table, its sampler and a "
            "table of its own, and the plan did not hold all three";
    return false;
  }
  if (Pipe) { return true; }

  const std::string source = KernelSource(error);
  if (source.empty()) { return false; }
  SDL_GPUComputePipelineCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = "mediumMultiScatterKernel";
  wanted.num_samplers = KernelShape.Samplers;
  wanted.num_readonly_storage_textures = KernelShape.ReadOnlyTextures;
  wanted.num_readwrite_storage_textures = KernelShape.ReadWriteTextures;
  wanted.num_uniform_buffers = KernelShape.UniformBuffers;
  wanted.threadcount_x = KernelShape.GroupX;
  wanted.threadcount_y = KernelShape.GroupY;
  wanted.threadcount_z = 1u;
  SDL_GPUComputePipeline *const made = SDL_CreateGPUComputePipeline(gpu.Device, &wanted);
  if (made == nullptr) {
    error = std::string("the medium's multiple scattering kernel was refused: ") + SDL_GetError();
    return false;
  }
  Pipe = OwnedComputePipeline(gpu.Device, made);
  Settled_ = false;
  return true;
}

void MediumMultiScatterStage::Declare(const Medium &medium) {
  if (Settled_ && std::memcmp(&Declared_, &medium, sizeof medium) == 0) { return; }
  Declared_ = medium;
  Settled_ = false;
}

void MediumMultiScatterStage::Encode(const PassRecording &into) {
  if (!Pipe || Settled_ || into.Dispatch == nullptr) { return; }
  SDL_PushGPUComputeUniformData(into.Commands, 0, &Declared_, (uint32_t)sizeof Declared_);
  SDL_BindGPUComputePipeline(into.Dispatch, Pipe.Get());
  SDL_GPUTextureSamplerBinding bound{Transmittance, Lut};
  SDL_BindGPUComputeSamplers(into.Dispatch, 0, &bound, 1);
  SDL_DispatchGPUCompute(into.Dispatch, (kMultiScatterLutSize + KernelShape.GroupX - 1u) / KernelShape.GroupX,
                         (kMultiScatterLutSize + KernelShape.GroupY - 1u) / KernelShape.GroupY, 1u);
  Settled_ = true;
}

std::string MediumMultiScatterStage::KernelSource() {
  std::string ignored;
  return Kernel(ignored);
}

std::string MediumMultiScatterStage::KernelSource(std::string &error) { return Kernel(error); }

}
