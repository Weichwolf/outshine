#include "math/Vec2.h"
#include "IrradianceStage.h"

#include <array>
#include <cstdint>
#include <string>

#include <cstdio>
#include <cstring>

#include "ShaderFile.h"
#include "ShaderPrelude.h"

namespace outshine::Render {

namespace {

struct Pushed {
  Medium Declared;
  float CosSunZenith = 0.0f;
  float GroundRadiusKm = 0.0f;
  Vec2f Pad = {{0.0f, 0.0f}};
};

static_assert(sizeof(Pushed) == sizeof(Medium) + 16, "the push keeps the medium's alignment");

std::string Kernel(std::string &error) {
  std::array<char, 512> declared{};
  std::snprintf(declared.data(),
                declared.size(),
                "constant uint kSkyViewSteps = %uu;\n"
                "constant uint kTransmittanceSteps = %uu;\n"
                "constant uint kMultiScatterGrid = %uu;\n"
                "constant float kMediumSampleSegment = %.9g;\n"
                "constant float kMediumLuminanceSegment = %.9g;\n"
                "constant float kMediumGroundLiftKm = %.9g;\n",
                static_cast<unsigned>(kSkyViewSteps),
                static_cast<unsigned>(kTransmittanceSteps),
                static_cast<unsigned>(kMultiScatterGrid),
                static_cast<double>(kMediumSampleSegment),
                static_cast<double>(kMediumLuminanceSegment),
                static_cast<double>(kMediumGroundLiftKm));
  std::string core;
  std::string body;
  if (!ParticipatingMediumMsl(core, error) ||
      !LoadShaderText("src/render/shaders/irradiance.msl", body, error)) {
    return {};
  }
  return MslPrelude(error) + declared.data() + core + body;
}

} // namespace

bool IrradianceStage::Configure(const Gpu &gpu,
                                SDL_GPUTexture *transmittance,
                                SDL_GPUTexture *multiScatter,
                                SDL_GPUSampler *lut,
                                SDL_GPUBuffer *into,
                                std::string &error) {
  if (Into != into || Transmittance != transmittance || MultiScatter != multiScatter) {
    Settled_ = false;
  }
  Transmittance = transmittance;
  MultiScatter = multiScatter;
  Lut = lut;
  Into = into;
  if (Transmittance == nullptr || MultiScatter == nullptr || Lut == nullptr || Into == nullptr) {
    error = "the sky's irradiance needs both medium tables, their sampler and a buffer of its own, "
            "and the plan did not hold all four";
    return false;
  }
  if (Pipe) { return true; }

  const std::string source = KernelSource(error);
  if (source.empty()) { return false; }
  SDL_GPUComputePipelineCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = "irradianceKernel";
  wanted.num_samplers = KernelShape.Samplers;
  wanted.num_readwrite_storage_buffers = KernelShape.ReadWriteBuffers;
  wanted.num_uniform_buffers = KernelShape.UniformBuffers;
  wanted.threadcount_x = KernelShape.GroupX;
  wanted.threadcount_y = 1u;
  wanted.threadcount_z = 1u;
  SDL_GPUComputePipeline *const made = SDL_CreateGPUComputePipeline(gpu.Device, &wanted);
  if (made == nullptr) {
    error = std::string("the irradiance kernel was refused: ") + SDL_GetError();
    return false;
  }
  Pipe = OwnedComputePipeline(gpu.Device, made);
  Settled_ = false;
  return true;
}

void IrradianceStage::Declare(const Medium &medium, float cosSunZenith) {
  const Standing wanted = {.Declared = medium, .CosSunZenith = cosSunZenith};
  if (Settled_ && Standing_ == wanted) { return; }
  Standing_ = wanted;
  Settled_ = false;
}

void IrradianceStage::Encode(const PassRecording &into) {
  if (!Pipe || Settled_ || into.Dispatch == nullptr) { return; }
  Pushed pushed{};
  pushed.Declared = Standing_.Declared;
  pushed.CosSunZenith = Standing_.CosSunZenith;
  pushed.GroundRadiusKm = Standing_.Declared.BottomRadiusKm + kMediumGroundLiftKm;
  SDL_PushGPUComputeUniformData(into.Commands, 0, &pushed, static_cast<uint32_t>(sizeof pushed));
  SDL_BindGPUComputePipeline(into.Dispatch, Pipe.Get());
  const std::array<SDL_GPUTextureSamplerBinding, 2> bound = {
      {{.texture = Transmittance, .sampler = Lut}, {.texture = MultiScatter, .sampler = Lut}}};
  SDL_BindGPUComputeSamplers(into.Dispatch, 0, bound.data(), 2);
  SDL_DispatchGPUCompute(into.Dispatch, 1u, 1u, 1u);
  Settled_ = true;
}

std::string IrradianceStage::KernelSource() {
  std::string ignored;
  return Kernel(ignored);
}

std::string IrradianceStage::KernelSource(std::string &error) {
  return Kernel(error);
}

} // namespace outshine::Render
