#include "MediumTransmittanceStage.h"

#include <cstdio>
#include <cstring>

#include "ShaderPrelude.h"

namespace outshine::Render {
namespace {


std::string Kernel(void) {
  char declared[256];
  std::snprintf(declared, sizeof declared,
                "constant uint kTransmittanceSteps = %uu;\n"
                "constant float kMediumSampleSegment = %.9g;\n",
                (unsigned)kTransmittanceSteps, (double)kMediumSampleSegment);
  return std::string(kMslPrelude) + declared + ParticipatingMediumMsl() + R"(
kernel void mediumTransmittanceKernel(uint2 at [[thread_position_in_grid]],
                                      texture2d<float, access::write> lut [[texture(0)]],
                                      constant Medium &medium [[buffer(0)]]) {
  uint2 extent = uint2(lut.get_width(), lut.get_height());
  if (at.x >= extent.x || at.y >= extent.y) { return; }
  float2 uv = (float2(at) + 0.5) / float2(extent);
  float radiusKm = 0.0;
  float cosZenith = 0.0;
  mediumTransmittanceParams(medium, uv.x, uv.y, radiusKm, cosZenith);
  float3 through =
      mediumTransmittance(medium, radiusKm, cosZenith, kTransmittanceSteps, kMediumSampleSegment);
  lut.write(float4(through, 1.0), at);
}
)";
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

  const std::string source = KernelSource();
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

std::string MediumTransmittanceStage::KernelSource(void) { return Kernel(); }

}
