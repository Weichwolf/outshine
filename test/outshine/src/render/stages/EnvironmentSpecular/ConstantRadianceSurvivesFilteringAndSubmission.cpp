#include "Check.h"
#include "EnvironmentSpecularLayout.h"
#include "EnvironmentSpecularStage.h"
#include "GpuOwned.h"
#include "IrradianceLayout.h"
#include "Readback.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <numbers>
#include <string>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

struct Sources {
  OwnedTexture Sky, Atlas;
  OwnedBuffer Irradiance;
  OwnedSampler Sample;

  explicit Sources(SDL_GPUDevice *device) {
    SDL_GPUTextureCreateInfo texture{};
    texture.type = SDL_GPU_TEXTURETYPE_2D;
    texture.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    texture.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    texture.width = 16;
    texture.height = 8;
    texture.layer_count_or_depth = texture.num_levels = 1;
    Sky = {device, SDL_CreateGPUTexture(device, &texture)};
    texture.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    texture.usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    texture.width = kEnvironmentWidth;
    texture.height = kEnvironmentHeight;
    Atlas = {device, SDL_CreateGPUTexture(device, &texture)};
    SDL_GPUBufferCreateInfo buffer{};
    buffer.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    buffer.size = kIrradianceFloats * sizeof(float);
    Irradiance = {device, SDL_CreateGPUBuffer(device, &buffer)};
    SDL_GPUSamplerCreateInfo sampler{};
    sampler.min_filter = sampler.mag_filter = SDL_GPU_FILTER_LINEAR;
    sampler.address_mode_u = sampler.address_mode_v = sampler.address_mode_w =
        SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    Sample = {device, SDL_CreateGPUSampler(device, &sampler)};
  }
};

bool UploadSources(SDL_GPUDevice *device, const Sources &source) {
  const std::array<float, kIrradianceFloats> sky{0.25f * std::numbers::pi_v<float>,
                                                 0.5f * std::numbers::pi_v<float>,
                                                 std::numbers::pi_v<float>,
                                                 0,
                                                 0,
                                                 0};
  SDL_GPUTransferBufferCreateInfo description{};
  description.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  description.size = sizeof(sky);
  OwnedTransfer transfer(device, SDL_CreateGPUTransferBuffer(device, &description));
  if (!transfer) { return false; }
  void *mapped = SDL_MapGPUTransferBuffer(device, transfer.Get(), false);
  if (!mapped) { return false; }
  std::memcpy(mapped, sky.data(), sizeof(sky));
  SDL_UnmapGPUTransferBuffer(device, transfer.Get());
  auto *commands = SDL_AcquireGPUCommandBuffer(device);
  if (!commands) { return false; }
  SDL_GPUColorTargetInfo colour{};
  colour.texture = source.Sky.Get();
  colour.clear_color = {0.25f, 0.5f, 1.0f, 1.0f};
  colour.load_op = SDL_GPU_LOADOP_CLEAR;
  colour.store_op = SDL_GPU_STOREOP_STORE;
  auto *raster = SDL_BeginGPURenderPass(commands, &colour, 1, nullptr);
  if (!raster) {
    SDL_CancelGPUCommandBuffer(commands);
    return false;
  }
  SDL_EndGPURenderPass(raster);
  auto *copy = SDL_BeginGPUCopyPass(commands);
  if (!copy) {
    SDL_CancelGPUCommandBuffer(commands);
    return false;
  }
  const SDL_GPUTransferBufferLocation from{.transfer_buffer = transfer.Get(), .offset = 0};
  const SDL_GPUBufferRegion into{
      .buffer = source.Irradiance.Get(), .offset = 0, .size = sizeof(sky)};
  SDL_UploadToGPUBuffer(copy, &from, &into, false);
  SDL_EndGPUCopyPass(copy);
  return SDL_SubmitGPUCommandBuffer(commands);
}

bool Filter(SDL_GPUDevice *device,
            const Sources &source,
            EnvironmentSpecularStage &stage,
            bool commit) {
  auto *commands = SDL_AcquireGPUCommandBuffer(device);
  if (!commands) { return false; }
  const SDL_GPUStorageTextureReadWriteBinding output{.texture = source.Atlas.Get()};
  auto *compute = SDL_BeginGPUComputePass(commands, &output, 1, nullptr, 0);
  if (!compute) {
    SDL_CancelGPUCommandBuffer(commands);
    return false;
  }
  StageSubmission submission;
  stage.Encode({.Commands = commands, .Dispatch = compute, .Submission = submission});
  SDL_EndGPUComputePass(compute);
  if (!commit) { return SDL_CancelGPUCommandBuffer(commands); }
  OwnedFence fence(device, SDL_SubmitGPUCommandBufferAndAcquireFence(commands));
  if (!fence) { return false; }
  submission.Commit();
  SDL_GPUFence *wait = fence.Get();
  return SDL_WaitForGPUFences(device, true, &wait, 1);
}

void CheckAtlas(SDL_GPUDevice *device, const Sources &source) {
  Readback read;
  CHECK(read.FromTexture(device,
                         source.Atlas.Get(),
                         {.WidthPx = kEnvironmentWidth, .HeightPx = kEnvironmentHeight},
                         8) == ReadState::Ready,
        "the actual GPU atlas reaches readback");
  if (!read.Rows()) { return; }
  constexpr std::array<uint16_t, 4> expected{0x3400, 0x3800, 0x3c00, 0x3c00};
  bool constant = true;
  for (uint32_t row = 0; row < kEnvironmentHeight; ++row) {
    for (uint32_t col = 0; col < kEnvironmentWidth; ++col) {
      std::array<uint16_t, 4> texel{};
      std::memcpy(texel.data(), read.Rows() + row * read.RowBytes() + col * 8u, sizeof(texel));
      for (size_t channel = 0; channel < texel.size(); ++channel) {
        const int error = static_cast<int>(texel[channel]) - expected[channel];
        constant &= error >= -1 && error <= 1;
      }
    }
  }
  CHECK(constant, "all roughness slices, edges and corners preserve constant incident radiance");
}

void CheckFiltering(SDL_GPUDevice *device) {
  Sources source(device);
  CHECK(source.Sky && source.Atlas && source.Irradiance && source.Sample,
        "the actual GPU owns independent source and filtered products");
  if (!source.Sky || !source.Atlas || !source.Irradiance || !source.Sample) { return; }
  CHECK(UploadSources(device, source), "the constant sky and Lambertian ground are submitted");
  EnvironmentSpecularStage stage;
  std::string error;
  CHECK(stage.Configure({.Device = device},
                        {.Sky = source.Sky.Get(),
                         .Irradiance = source.Irradiance.Get(),
                         .Sampler = source.Sample.Get(),
                         .Target = source.Atlas.Get()},
                        error),
        "the atlas compute pipeline opens");
  stage.Declare(kEarthAir, 0.5f, 0, {{1, 1, 1}});
  CHECK(Filter(device, source, stage, false) && !stage.Settled(),
        "cancelled compute leaves the product dirty");
  CHECK(Filter(device, source, stage, true) && stage.Settled(),
        "only accepted submission publishes the filtered product");
  CheckAtlas(device, source);
  stage.Declare(kEarthAir, 0.5f, 0, {{1, 1, 1}});
  CHECK(stage.Settled(), "an unchanged lighting state retains its filtered product");
  stage.Declare(kEarthAir, 0.5f, 0, {{0, 0, 0}});
  CHECK(!stage.Settled(), "changed ground radiance invalidates the product");
  CHECK(Filter(device, source, stage, true), "changed illumination is submitted");
  Readback read;
  CHECK(read.FromTexture(device,
                         source.Atlas.Get(),
                         {.WidthPx = kEnvironmentWidth, .HeightPx = kEnvironmentHeight},
                         8) == ReadState::Ready,
        "the changed atlas reaches readback");
  if (read.Rows()) {
    uint16_t ground = 1;
    std::memcpy(&ground, read.Rows() + read.RowBytes() + 8, sizeof(ground));
    CHECK(ground == 0, "a downward mirror ray sees the changed ground rather than sky");
  }
}
}

int main() {
  CHECK(SDL_SetHint(SDL_HINT_ASSERT, "abort"), "assertions fail without a dialog");
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video initializes");
  {
    OwnedDevice device(SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL |
                                               SDL_GPU_SHADERFORMAT_METALLIB,
                                           false,
                                           nullptr));
    CHECK(device, "an actual GPU backend is available");
    if (device) { CheckFiltering(device.Get()); }
  }
  SDL_Quit();
  return Report();
}
