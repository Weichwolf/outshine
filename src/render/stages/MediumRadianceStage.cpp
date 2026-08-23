#include "MediumRadianceStage.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "ShaderFile.h"
#include "ShaderPrelude.h"

namespace outshine::Render {
namespace {


struct Pushed {
  Medium Declared;
  float CosSunZenith;
  float EyeRadiusKm;
  float Pad[2];
};

static_assert(sizeof(Pushed) == sizeof(Medium) + 16, "the push keeps the medium's alignment");

std::string Kernel(std::string &error) {
  char declared[512];
  std::snprintf(declared, sizeof declared,
                "constant uint kSkyViewSteps = %uu;\n"
                "constant float kMediumLuminanceSegment = %.9g;\n"
                "constant float kMediumGroundLiftKm = %.9g;\n",
                (unsigned)kSkyViewSteps, (double)kMediumLuminanceSegment,
                (double)kMediumGroundLiftKm);
  std::string core;
  std::string body;
  if (!ParticipatingMediumMsl(core, error) ||
      !LoadShaderText("src/render/shaders/mediumRadiance.msl", body, error)) {
    return std::string();
  }
  return MslPrelude() + declared + core + body;
}

}

bool MediumRadianceStage::Configure(const Gpu &gpu, SDL_GPUTexture *transmittance,
                                    SDL_GPUTexture *multiScatter, SDL_GPUSampler *lut,
                                    SDL_GPUTexture *into, std::string &error) {
  if (Into != into || Transmittance != transmittance || MultiScatter != multiScatter) {
    Settled_ = false;
  }
  Transmittance = transmittance;
  MultiScatter = multiScatter;
  Lut = lut;
  Into = into;
  if (Transmittance == nullptr || MultiScatter == nullptr || Lut == nullptr || Into == nullptr) {
    error = "the sky view needs both medium tables, their sampler and a table of its own, and the "
            "plan did not hold all four";
    return false;
  }
  if (Pipe) { return true; }

  const std::string source = KernelSource(error);
  if (source.empty()) { return false; }
  SDL_GPUComputePipelineCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = "mediumRadianceKernel";
  wanted.num_samplers = KernelShape.Samplers;
  wanted.num_readonly_storage_textures = KernelShape.ReadOnlyTextures;
  wanted.num_readwrite_storage_textures = KernelShape.ReadWriteTextures;
  wanted.num_uniform_buffers = KernelShape.UniformBuffers;
  wanted.threadcount_x = KernelShape.GroupX;
  wanted.threadcount_y = KernelShape.GroupY;
  wanted.threadcount_z = 1u;
  SDL_GPUComputePipeline *const made = SDL_CreateGPUComputePipeline(gpu.Device, &wanted);
  if (made == nullptr) {
    error = std::string("the sky view kernel was refused: ") + SDL_GetError();
    return false;
  }
  Pipe = OwnedComputePipeline(gpu.Device, made);
  Settled_ = false;
  return true;
}

void MediumRadianceStage::Declare(const Medium &medium, float cosSunZenith, float eyeHeightM) {
  Standing wanted;
  wanted.Declared = medium;
  wanted.CosSunZenith = cosSunZenith;
  wanted.EyeHeightM = eyeHeightM;
  // the memcmp is a fact, not a hope: no padding byte can differ
  if (Settled_ && std::memcmp(&Standing_, &wanted, sizeof wanted) == 0) { return; }
  Standing_ = wanted;
  Settled_ = false;
}

void MediumRadianceStage::Encode(const PassRecording &into) {
  if (!Pipe || Settled_ || into.Dispatch == nullptr) { return; }
  Pushed pushed{};
  pushed.Declared = Standing_.Declared;
  pushed.CosSunZenith = Standing_.CosSunZenith;
  pushed.EyeRadiusKm = Standing_.Declared.BottomRadiusKm + kMediumGroundLiftKm +
                       std::fmax(0.0f, Standing_.EyeHeightM) / 1000.0f;
  SDL_PushGPUComputeUniformData(into.Commands, 0, &pushed, (uint32_t)sizeof pushed);
  SDL_BindGPUComputePipeline(into.Dispatch, Pipe.Get());
  SDL_GPUTextureSamplerBinding bound[2] = {{Transmittance, Lut}, {MultiScatter, Lut}};
  SDL_BindGPUComputeSamplers(into.Dispatch, 0, bound, 2);
  SDL_DispatchGPUCompute(into.Dispatch, (kSkyViewLutWidth + KernelShape.GroupX - 1u) / KernelShape.GroupX,
                         (kSkyViewLutHeight + KernelShape.GroupY - 1u) / KernelShape.GroupY, 1u);
  Settled_ = true;
}

std::string MediumRadianceStage::KernelSource() {
  std::string ignored;
  return Kernel(ignored);
}

std::string MediumRadianceStage::KernelSource(std::string &error) { return Kernel(error); }

}
