#include "MediumRadianceStage.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "ShaderPrelude.h"

namespace outshine::Render {
namespace {

inline constexpr uint32_t kGroupSize = 8;

struct Pushed {
  Medium Declared;
  float CosSunZenith;
  float EyeRadiusKm;
  float Pad[2];
};

static_assert(sizeof(Pushed) == sizeof(Medium) + 16, "the push keeps the medium's alignment");

std::string Kernel(void) {
  char declared[512];
  std::snprintf(declared, sizeof declared,
                "constant uint kSkyViewSteps = %uu;\n"
                "constant float kMediumLuminanceSegment = %.9g;\n"
                "constant float kMediumGroundLiftKm = %.9g;\n",
                (unsigned)kSkyViewSteps, (double)kMediumLuminanceSegment,
                (double)kMediumGroundLiftKm);
  return std::string(kMslPrelude) + declared + ParticipatingMediumMsl() + R"(
struct Pushed {
  Medium declared;
  float cosSunZenith;
  float eyeRadiusKm;
  float2 pad;
};

kernel void mediumRadianceKernel(uint2 at [[thread_position_in_grid]],
                                 texture2d<float> transmittance [[texture(0)]],
                                 texture2d<float> multiScatter [[texture(1)]],
                                 texture2d<float, access::write> into [[texture(2)]],
                                 sampler lut [[sampler(0)]],
                                 constant Pushed &pushed [[buffer(0)]]) {
  uint2 extent = uint2(into.get_width(), into.get_height());
  if (at.x >= extent.x || at.y >= extent.y) { return; }
  float2 uv = (float2(at) + 0.5) / float2(extent);
  float cosView = 0.0;
  float lightViewCos = 0.0;
  skyViewParams(pushed.declared, pushed.eyeRadiusKm, uv.x, uv.y, float(extent.x), float(extent.y),
                cosView, lightViewCos);
  float3 luminance = mediumSkyRay(pushed.declared, pushed.eyeRadiusKm, cosView, lightViewCos,
                                  pushed.cosSunZenith, transmittance, multiScatter, lut,
                                  kSkyViewSteps, kMediumLuminanceSegment, kMediumGroundLiftKm);
  into.write(float4(luminance, 1.0), at);
}
)";
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

  const std::string source = Kernel();
  SDL_GPUComputePipelineCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = "mediumRadianceKernel";
  wanted.num_samplers = 2u;
  wanted.num_readwrite_storage_textures = 1u;
  wanted.num_uniform_buffers = 1u;
  wanted.threadcount_x = kGroupSize;
  wanted.threadcount_y = kGroupSize;
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
  SDL_DispatchGPUCompute(into.Dispatch, (kSkyViewLutWidth + kGroupSize - 1u) / kGroupSize,
                         (kSkyViewLutHeight + kGroupSize - 1u) / kGroupSize, 1u);
  Settled_ = true;
}

}
