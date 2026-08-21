#include "MediumMultiScatterStage.h"

#include <cstdio>
#include <cstring>

#include "ShaderPrelude.h"

namespace outshine::Render {
namespace {

inline constexpr uint32_t kGroupSize = 8;

std::string Kernel(void) {
  char declared[512];
  std::snprintf(declared, sizeof declared,
                "constant uint kMultiScatterSteps = %uu;\n"
                "constant uint kMultiScatterGrid = %uu;\n"
                "constant float kMediumLuminanceSegment = %.9g;\n"
                "constant float kMediumGroundLiftKm = %.9g;\n",
                (unsigned)kMultiScatterSteps, (unsigned)kMultiScatterGrid,
                (double)kMediumLuminanceSegment, (double)kMediumGroundLiftKm);
  return std::string(kMslPrelude) + declared + ParticipatingMediumMsl() + R"(
kernel void mediumMultiScatterKernel(uint2 at [[thread_position_in_grid]],
                                     texture2d<float> transmittance [[texture(0)]],
                                     texture2d<float, access::write> into [[texture(1)]],
                                     sampler lut [[sampler(0)]],
                                     constant Medium &medium [[buffer(0)]]) {
  uint2 extent = uint2(into.get_width(), into.get_height());
  if (at.x >= extent.x || at.y >= extent.y) { return; }
  float2 uv = (float2(at) + 0.5) / float2(extent);
  float3 luminance;
  float3 transfer;
  mediumMultiScatterTexel(medium, uv.x, uv.y, transmittance, lut, kMultiScatterSteps,
                          kMultiScatterGrid, kMediumLuminanceSegment, kMediumGroundLiftKm,
                          luminance, transfer);
  float3 everyOrder = luminance / (1.0 - transfer);
  into.write(float4(everyOrder, 1.0), at);
}
)";
}

}

bool MediumMultiScatterStage::Configure(const Gpu &gpu, SDL_GPUTexture *transmittance,
                                        SDL_GPUSampler *lut, SDL_GPUTexture *into,
                                        std::string &error) {
  Transmittance = transmittance;
  Lut = lut;
  Into = into;
  if (Transmittance == nullptr || Lut == nullptr || Into == nullptr) {
    error = "the multiple scattering table needs the transmittance table, its sampler and a "
            "table of its own, and the plan did not hold all three";
    return false;
  }
  if (Pipe) { return true; }

  const std::string source = Kernel();
  SDL_GPUComputePipelineCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = "mediumMultiScatterKernel";
  wanted.num_samplers = 1u;
  wanted.num_readwrite_storage_textures = 1u;
  wanted.num_uniform_buffers = 1u;
  wanted.threadcount_x = kGroupSize;
  wanted.threadcount_y = kGroupSize;
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
  SDL_DispatchGPUCompute(into.Dispatch, (kMultiScatterLutSize + kGroupSize - 1u) / kGroupSize,
                         (kMultiScatterLutSize + kGroupSize - 1u) / kGroupSize, 1u);
  Settled_ = true;
}

}
