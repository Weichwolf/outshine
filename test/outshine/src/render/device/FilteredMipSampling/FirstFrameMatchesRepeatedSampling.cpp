#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "Check.h"
#include "GpuOwned.h"
#include "KernelShape.h"
#include "ShaderFile.h"

namespace {
using namespace outshine::Render;
using namespace outshine::Test;

constexpr uint32_t kSourceWidth = 128;
constexpr uint32_t kTargetWidth = 64;
constexpr uint32_t kChannels = 4;

struct Fixture {
  OwnedDevice Device;
  OwnedTexture Source;
  OwnedSampler Sampler;
  OwnedPipeline Pipeline;
  OwnedTransfer Upload;

  explicit Fixture(SDL_GPUDevice *device) : Device(device) {}
};

enum class MipMode { None, Nearest, Linear };

std::vector<uint8_t> Pixels(uint32_t width) {
  std::vector<uint8_t> pixels(static_cast<size_t>(width) * width * kChannels);
  for (uint32_t y = 0; y < width; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      const size_t at = (static_cast<size_t>(y) * width + x) * kChannels;
      const uint8_t checker = ((x ^ y) & 1u) == 0 ? 0 : 255;
      pixels[at] = checker;
      pixels[at + 1] = static_cast<uint8_t>(255u - checker);
      pixels[at + 2] = static_cast<uint8_t>((x * 255u) / (width - 1u));
      pixels[at + 3] = 255;
    }
  }
  return pixels;
}

uint32_t Levels(MipMode mode) {
  if (mode == MipMode::None) { return 1; }
  uint32_t levels = 1;
  for (uint32_t width = kSourceWidth; width > 1; width /= 2) { ++levels; }
  return levels;
}

bool UploadChain(Fixture &fixture, MipMode mode) {
  const uint32_t levels = Levels(mode);
  std::vector<std::vector<uint8_t>> chain;
  uint32_t bytes = 0;
  for (uint32_t width = kSourceWidth; chain.size() < levels; width /= 2) {
    chain.push_back(Pixels(width));
    bytes += static_cast<uint32_t>(chain.back().size());
  }
  SDL_GPUTextureCreateInfo texture{};
  texture.type = SDL_GPU_TEXTURETYPE_2D;
  texture.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  texture.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  texture.width = kSourceWidth;
  texture.height = kSourceWidth;
  texture.layer_count_or_depth = 1;
  texture.num_levels = levels;
  texture.sample_count = SDL_GPU_SAMPLECOUNT_1;
  fixture.Source =
      OwnedTexture(fixture.Device.Get(), SDL_CreateGPUTexture(fixture.Device.Get(), &texture));
  if (!fixture.Source) { return false; }
  SDL_GPUTransferBufferCreateInfo transfer{};
  transfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  transfer.size = bytes;
  fixture.Upload = OwnedTransfer(fixture.Device.Get(),
                                 SDL_CreateGPUTransferBuffer(fixture.Device.Get(), &transfer));
  if (!fixture.Upload) { return false; }
  auto *mapped = static_cast<uint8_t *>(
      SDL_MapGPUTransferBuffer(fixture.Device.Get(), fixture.Upload.Get(), false));
  if (mapped == nullptr) { return false; }
  uint32_t offset = 0;
  for (const auto &level : chain) {
    std::memcpy(mapped + offset, level.data(), level.size());
    offset += static_cast<uint32_t>(level.size());
  }
  SDL_UnmapGPUTransferBuffer(fixture.Device.Get(), fixture.Upload.Get());
  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(fixture.Device.Get());
  if (commands == nullptr) { return false; }
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
  if (copy == nullptr) {
    SDL_CancelGPUCommandBuffer(commands);
    return false;
  }
  offset = 0;
  uint32_t width = kSourceWidth;
  for (uint32_t mip = 0; mip < levels; ++mip) {
    SDL_GPUTextureTransferInfo source{.transfer_buffer = fixture.Upload.Get(),
                                      .offset = offset,
                                      .pixels_per_row = width,
                                      .rows_per_layer = width};
    SDL_GPUTextureRegion destination{
        .texture = fixture.Source.Get(), .mip_level = mip, .w = width, .h = width, .d = 1};
    SDL_UploadToGPUTexture(copy, &source, &destination, false);
    offset += width * width * kChannels;
    width /= 2;
  }
  SDL_EndGPUCopyPass(copy);
  if (!SDL_SubmitGPUCommandBuffer(commands)) { return false; }
  return true;
}

bool Configure(Fixture &fixture, MipMode mode) {
  if (!UploadChain(fixture, mode)) { return false; }
  SDL_GPUSamplerCreateInfo sampler{};
  sampler.min_filter = SDL_GPU_FILTER_LINEAR;
  sampler.mag_filter = SDL_GPU_FILTER_LINEAR;
  sampler.mipmap_mode = mode == MipMode::Nearest ? SDL_GPU_SAMPLERMIPMAPMODE_NEAREST
                                                 : SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
  sampler.max_lod = static_cast<float>(Levels(mode) - 1u);
  fixture.Sampler =
      OwnedSampler(fixture.Device.Get(), SDL_CreateGPUSampler(fixture.Device.Get(), &sampler));
  if (!fixture.Sampler) { return false; }
  std::string error;
  const OwnedShader vertex(fixture.Device.Get(),
                           ShaderFrom(fixture.Device.Get(),
                                      "build/shaders/fullscreen.vert.spv",
                                      SDL_GPU_SHADERSTAGE_VERTEX,
                                      DrawShape{},
                                      error));
  const DrawShape shape{.FragmentSamplers = 1};
  const OwnedShader fragment(fixture.Device.Get(),
                             ShaderFrom(fixture.Device.Get(),
                                        "build/shaders/filteredMipSample.frag.spv",
                                        SDL_GPU_SHADERSTAGE_FRAGMENT,
                                        shape,
                                        error));
  if (!vertex || !fragment) {
    SDL_SetError("%s", error.c_str());
    return false;
  }
  SDL_GPUColorTargetDescription target{.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM};
  SDL_GPUGraphicsPipelineCreateInfo pipeline{};
  pipeline.vertex_shader = vertex.Get();
  pipeline.fragment_shader = fragment.Get();
  pipeline.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  pipeline.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
  pipeline.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  pipeline.target_info.color_target_descriptions = &target;
  pipeline.target_info.num_color_targets = 1;
  fixture.Pipeline = OwnedPipeline(fixture.Device.Get(),
                                   SDL_CreateGPUGraphicsPipeline(fixture.Device.Get(), &pipeline));
  return static_cast<bool>(fixture.Pipeline);
}

std::vector<uint8_t> Draw(Fixture &fixture) {
  SDL_GPUTextureCreateInfo target{};
  target.type = SDL_GPU_TEXTURETYPE_2D;
  target.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  target.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
  target.width = kTargetWidth;
  target.height = kTargetWidth;
  target.layer_count_or_depth = 1;
  target.num_levels = 1;
  target.sample_count = SDL_GPU_SAMPLECOUNT_1;
  OwnedTexture image(fixture.Device.Get(), SDL_CreateGPUTexture(fixture.Device.Get(), &target));
  if (!image) { return {}; }
  SDL_GPUTransferBufferCreateInfo transfer{.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
                                           .size = kTargetWidth * kTargetWidth * kChannels};
  OwnedTransfer download(fixture.Device.Get(),
                         SDL_CreateGPUTransferBuffer(fixture.Device.Get(), &transfer));
  if (!download) { return {}; }
  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(fixture.Device.Get());
  if (commands == nullptr) { return {}; }
  SDL_GPUColorTargetInfo attachment{.texture = image.Get(),
                                    .clear_color = {0, 0, 0, 0},
                                    .load_op = SDL_GPU_LOADOP_CLEAR,
                                    .store_op = SDL_GPU_STOREOP_STORE};
  SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(commands, &attachment, 1, nullptr);
  if (pass == nullptr) {
    SDL_CancelGPUCommandBuffer(commands);
    return {};
  }
  SDL_BindGPUGraphicsPipeline(pass, fixture.Pipeline.Get());
  const SDL_GPUTextureSamplerBinding binding{.texture = fixture.Source.Get(),
                                             .sampler = fixture.Sampler.Get()};
  SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
  SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
  SDL_EndGPURenderPass(pass);
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
  if (copy == nullptr) {
    SDL_CancelGPUCommandBuffer(commands);
    return {};
  }
  SDL_GPUTextureRegion source{.texture = image.Get(), .w = kTargetWidth, .h = kTargetWidth, .d = 1};
  SDL_GPUTextureTransferInfo destination{.transfer_buffer = download.Get(),
                                         .pixels_per_row = kTargetWidth,
                                         .rows_per_layer = kTargetWidth};
  SDL_DownloadFromGPUTexture(copy, &source, &destination);
  SDL_EndGPUCopyPass(copy);
  SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
  if (fence == nullptr) { return {}; }
  const bool ready = SDL_WaitForGPUFences(fixture.Device.Get(), true, &fence, 1);
  SDL_ReleaseGPUFence(fixture.Device.Get(), fence);
  if (!ready) { return {}; }
  const auto *mapped = static_cast<const uint8_t *>(
      SDL_MapGPUTransferBuffer(fixture.Device.Get(), download.Get(), false));
  if (mapped == nullptr) { return {}; }
  std::vector<uint8_t> pixels(mapped, mapped + transfer.size);
  SDL_UnmapGPUTransferBuffer(fixture.Device.Get(), download.Get());
  return pixels;
}

std::vector<uint8_t> Render(MipMode mode) {
  Fixture fixture(SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV |
                                          SDL_GPU_SHADERFORMAT_DXIL,
                                      false,
                                      nullptr));
  if (!fixture.Device || !Configure(fixture, mode)) { return {}; }
  return Draw(fixture);
}

void CheckMode(MipMode mode, const char *name) {
  Fixture fixture(SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV |
                                          SDL_GPU_SHADERFORMAT_DXIL,
                                      false,
                                      nullptr));
  CHECK(fixture.Device && Configure(fixture, mode), "the raw SDL fixture configures");
  if (!fixture.Device || !fixture.Pipeline) { return; }
  const auto first = Draw(fixture);
  const auto repeated = Draw(fixture);
  const auto fresh = Render(mode);
  CHECK(!first.empty(), "the raw SDL fixture renders its first filtered frame");
  CHECK(first == repeated, name);
  CHECK(first == fresh, "a fresh SDL device has the same first filtered frame");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video initializes for raw filtered sampling");
  CheckMode(MipMode::None, "base-only linear sampling repeats exactly");
  CheckMode(MipMode::Nearest, "nearest-mip linear sampling repeats exactly");
  CheckMode(MipMode::Linear, "linear-mip linear sampling repeats exactly");
  SDL_Quit();
  return Report();
}
