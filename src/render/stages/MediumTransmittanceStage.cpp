#include "MediumTransmittanceStage.h"

#include <cstdio>
#include <cstring>

#include "ShaderFile.h"
#include "ShaderPrelude.h"

namespace outshine::Render {
namespace {


std::string Kernel(std::string &error) {
  char declared[256];
  std::snprintf(declared, sizeof declared,
                "constant uint kTransmittanceSteps = %uu;\n"
                "constant float kMediumSampleSegment = %.9g;\n",
                (unsigned)kTransmittanceSteps, (double)kMediumSampleSegment);
  std::string core;
  std::string body;
  if (!ParticipatingMediumMsl(core, error) ||
      !LoadShaderText("src/render/shaders/mediumTransmittance.msl", body, error)) {
    return std::string();
  }
  return MslPrelude() + declared + core + body;
}

}

bool MediumTransmittanceStage::Configure(const Gpu &gpu, SDL_GPUTexture *lut, std::string &error) {
  if (Lut != lut) { Settled_ = false; }
  Lut = lut;
  if (Lut == nullptr) {
    error = "the plan holds no transmittance table for this stage to write, so nothing downstream "
            "could read one";
    return false;
  }
  if (Pipe) { return true; }

  const std::string source = KernelSource(error);
  if (source.empty()) { return false; }
  SDL_GPUComputePipelineCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = "mediumTransmittanceKernel";
  wanted.num_samplers = KernelShape.Samplers;
  wanted.num_readonly_storage_textures = KernelShape.ReadOnlyTextures;
  wanted.num_readwrite_storage_textures = KernelShape.ReadWriteTextures;
  wanted.num_uniform_buffers = KernelShape.UniformBuffers;
  wanted.threadcount_x = KernelShape.GroupX;
  wanted.threadcount_y = KernelShape.GroupY;
  wanted.threadcount_z = 1u;
  SDL_GPUComputePipeline *const made = SDL_CreateGPUComputePipeline(gpu.Device, &wanted);
  if (made == nullptr) {
    error = std::string("the medium's transmittance kernel was refused: ") + SDL_GetError();
    return false;
  }
  Pipe = OwnedComputePipeline(gpu.Device, made);
  Settled_ = false;
  return true;
}

void MediumTransmittanceStage::Declare(const Medium &medium) {
  // the memcmp is a fact, not a hope: Medium static_asserts 80 = 20 named floats
  if (Settled_ && std::memcmp(&Declared_, &medium, sizeof medium) == 0) { return; }
  Declared_ = medium;
  Settled_ = false;
}

void MediumTransmittanceStage::Encode(const PassRecording &into) {
  if (!Pipe || Settled_ || into.Dispatch == nullptr) { return; }
  SDL_PushGPUComputeUniformData(into.Commands, 0, &Declared_, (uint32_t)sizeof Declared_);
  SDL_BindGPUComputePipeline(into.Dispatch, Pipe.Get());
  SDL_DispatchGPUCompute(into.Dispatch, (kTransmittanceLutWidth + KernelShape.GroupX - 1u) / KernelShape.GroupX,
                         (kTransmittanceLutHeight + KernelShape.GroupY - 1u) / KernelShape.GroupY, 1u);
  Settled_ = true;
}

std::string MediumTransmittanceStage::KernelSource() {
  std::string ignored;
  return Kernel(ignored);
}

std::string MediumTransmittanceStage::KernelSource(std::string &error) { return Kernel(error); }

}
