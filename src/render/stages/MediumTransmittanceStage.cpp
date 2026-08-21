#include "MediumTransmittanceStage.h"

#include <cstdio>
#include <cstring>

#include "ShaderPrelude.h"

namespace outshine::Render {
namespace {

inline constexpr uint32_t kGroupWidth = 8;
inline constexpr uint32_t kGroupHeight = 8;

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
  Lut = lut;
  if (Lut == nullptr) {
    error = "the plan holds no transmittance table for this stage to write, so nothing downstream "
            "could read one";
    return false;
  }
  if (Pipe) { return true; }

  const std::string source = Kernel();
  SDL_GPUComputePipelineCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = "mediumTransmittanceKernel";
  wanted.num_readwrite_storage_textures = 1u;
  wanted.num_uniform_buffers = 1u;
  wanted.threadcount_x = kGroupWidth;
  wanted.threadcount_y = kGroupHeight;
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
  SDL_DispatchGPUCompute(into.Dispatch, (kTransmittanceLutWidth + kGroupWidth - 1u) / kGroupWidth,
                         (kTransmittanceLutHeight + kGroupHeight - 1u) / kGroupHeight, 1u);
  Settled_ = true;
}

}
