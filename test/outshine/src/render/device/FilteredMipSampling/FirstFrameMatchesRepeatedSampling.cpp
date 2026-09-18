#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <span>
#include <vector>

#include "Check.h"
#include "GpuOwned.h"
#include "KernelShape.h"
#include "ShaderFile.h"
#include "TexelChain.h"

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
  bool DescriptorTable = false;

  explicit Fixture(SDL_GPUDevice *device) : Device(device) {}
};

enum class MipMode { None, Nearest, Linear };

uint32_t Levels(MipMode mode);

std::vector<float> Pixels(uint32_t width) {
  std::vector<float> pixels(static_cast<size_t>(width) * width * kChannels);
  for (uint32_t y = 0; y < width; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      const size_t at = (static_cast<size_t>(y) * width + x) * kChannels;
      const float checker = ((x ^ y) & 1u) == 0 ? 0.0f : 1.0f;
      pixels[at] = checker;
      pixels[at + 1] = 1.0f - checker;
      pixels[at + 2] = static_cast<float>(x) / static_cast<float>(width - 1u);
      pixels[at + 3] = 1.0f;
    }
  }
  return pixels;
}

std::vector<uint8_t> Encode(std::span<const float> linear) {
  std::vector<uint8_t> encoded(linear.size());
  for (size_t at = 0; at < linear.size(); ++at) {
    encoded[at] = static_cast<uint8_t>(std::lround(std::clamp(linear[at], 0.0f, 1.0f) * 255.0f));
  }
  return encoded;
}

std::vector<std::vector<uint8_t>> Chain(MipMode mode) {
  std::vector<float> level = Pixels(kSourceWidth);
  std::vector<std::vector<uint8_t>> chain;
  uint32_t width = kSourceWidth;
  for (uint32_t mip = 0; mip < Levels(mode); ++mip) {
    chain.push_back(Encode(level));
    if (width > 1) {
      std::vector<float> next;
      HalveInPlace(level, {.WidthPx = width, .HeightPx = width}, next, TexelKind::Value);
      level.swap(next);
      width /= 2;
    }
  }
  return chain;
}

uint32_t Levels(MipMode mode) {
  if (mode == MipMode::None) { return 1; }
  uint32_t levels = 1;
  for (uint32_t width = kSourceWidth; width > 1; width /= 2) { ++levels; }
  return levels;
}

bool UploadChain(Fixture &fixture, MipMode mode, bool separateSubmits) {
  const uint32_t levels = Levels(mode);
  const auto chain = Chain(mode);
  uint32_t bytes = 0;
  for (const auto &level : chain) { bytes += static_cast<uint32_t>(level.size()); }
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
  if (separateSubmits) {
    uint32_t width = kSourceWidth;
    for (uint32_t mip = 0; mip < levels; ++mip) {
      SDL_GPUTransferBufferCreateInfo one{};
      one.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
      one.size = static_cast<uint32_t>(chain[mip].size());
      const OwnedTransfer staging(fixture.Device.Get(),
                                  SDL_CreateGPUTransferBuffer(fixture.Device.Get(), &one));
      if (!staging) { return false; }
      void *mapped = SDL_MapGPUTransferBuffer(fixture.Device.Get(), staging.Get(), false);
      if (mapped == nullptr) { return false; }
      std::memcpy(mapped, chain[mip].data(), chain[mip].size());
      SDL_UnmapGPUTransferBuffer(fixture.Device.Get(), staging.Get());
      SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(fixture.Device.Get());
      if (commands == nullptr) { return false; }
      SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
      if (copy == nullptr) {
        SDL_CancelGPUCommandBuffer(commands);
        return false;
      }
      SDL_GPUTextureTransferInfo source{
          .transfer_buffer = staging.Get(), .pixels_per_row = width, .rows_per_layer = width};
      SDL_GPUTextureRegion destination{
          .texture = fixture.Source.Get(), .mip_level = mip, .w = width, .h = width, .d = 1};
      SDL_UploadToGPUTexture(copy, &source, &destination, false);
      SDL_EndGPUCopyPass(copy);
      if (!SDL_SubmitGPUCommandBuffer(commands)) { return false; }
      width /= 2;
    }
    return true;
  }
  SDL_GPUTransferBufferCreateInfo transfer{};
  transfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  transfer.size = bytes;
  OwnedTransfer upload(fixture.Device.Get(),
                       SDL_CreateGPUTransferBuffer(fixture.Device.Get(), &transfer));
  if (!upload) { return false; }
  auto *mapped =
      static_cast<uint8_t *>(SDL_MapGPUTransferBuffer(fixture.Device.Get(), upload.Get(), false));
  if (mapped == nullptr) { return false; }
  uint32_t offset = 0;
  for (const auto &level : chain) {
    std::memcpy(mapped + offset, level.data(), level.size());
    offset += static_cast<uint32_t>(level.size());
  }
  SDL_UnmapGPUTransferBuffer(fixture.Device.Get(), upload.Get());
  const auto record = [&](uint32_t first, uint32_t count) {
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(fixture.Device.Get());
    if (commands == nullptr) { return false; }
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
    if (copy == nullptr) {
      SDL_CancelGPUCommandBuffer(commands);
      return false;
    }
    uint32_t levelOffset = 0;
    for (uint32_t prior = 0; prior < first; ++prior) {
      levelOffset += static_cast<uint32_t>(chain[prior].size());
    }
    uint32_t width = kSourceWidth >> first;
    for (uint32_t mip = first; mip < first + count; ++mip) {
      SDL_GPUTextureTransferInfo source{.transfer_buffer = upload.Get(),
                                        .offset = levelOffset,
                                        .pixels_per_row = width,
                                        .rows_per_layer = width};
      SDL_GPUTextureRegion destination{
          .texture = fixture.Source.Get(), .mip_level = mip, .w = width, .h = width, .d = 1};
      SDL_UploadToGPUTexture(copy, &source, &destination, false);
      levelOffset += static_cast<uint32_t>(chain[mip].size());
      width /= 2;
    }
    SDL_EndGPUCopyPass(copy);
    return SDL_SubmitGPUCommandBuffer(commands);
  };
  return record(0, levels);
}

bool Configure(Fixture &fixture, MipMode mode, bool separateSubmits, bool descriptorTable) {
  if (!UploadChain(fixture, mode, separateSubmits)) { return false; }
  fixture.DescriptorTable = descriptorTable;
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
  const DrawShape shape{.FragmentSamplers = descriptorTable ? 8u : 1u};
  const char *fragmentPath = descriptorTable ? "build/shaders/filteredMipDescriptorSample.frag.spv"
                                             : "build/shaders/filteredMipSample.frag.spv";
  const OwnedShader fragment(
      fixture.Device.Get(),
      ShaderFrom(fixture.Device.Get(), fragmentPath, SDL_GPU_SHADERSTAGE_FRAGMENT, shape, error));
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
  const std::array<SDL_GPUTextureSamplerBinding, 8> descriptors = {
      binding, binding, binding, binding, binding, binding, binding, binding};
  SDL_BindGPUFragmentSamplers(pass, 0, descriptors.data(), fixture.DescriptorTable ? 8u : 1u);
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

std::vector<uint8_t> Render(MipMode mode, bool separateSubmits, bool descriptorTable) {
  Fixture fixture(SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV |
                                          SDL_GPU_SHADERFORMAT_DXIL,
                                      false,
                                      nullptr));
  if (!fixture.Device || !Configure(fixture, mode, separateSubmits, descriptorTable)) { return {}; }
  return Draw(fixture);
}

void CheckMode(MipMode mode, bool separateSubmits, bool descriptorTable, const char *name) {
  Fixture fixture(SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV |
                                          SDL_GPU_SHADERFORMAT_DXIL,
                                      false,
                                      nullptr));
  CHECK(fixture.Device && Configure(fixture, mode, separateSubmits, descriptorTable),
        "the raw SDL fixture configures");
  if (!fixture.Device || !fixture.Pipeline) { return; }
  const auto first = Draw(fixture);
  const auto repeated = Draw(fixture);
  const auto fresh = Render(mode, separateSubmits, descriptorTable);
  CHECK(!first.empty(), "the raw SDL fixture renders its first filtered frame");
  CHECK(first == repeated, name);
  CHECK(first == fresh, "a fresh SDL device has the same first filtered frame");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video initializes for raw filtered sampling");
  CheckMode(MipMode::None, false, false, "base-only linear sampling repeats exactly");
  CheckMode(MipMode::Nearest, false, false, "batched nearest-mip sampling repeats exactly");
  CheckMode(MipMode::Linear, false, false, "batched linear-mip sampling repeats exactly");
  CheckMode(MipMode::Nearest, true, false, "per-level nearest-mip sampling repeats exactly");
  CheckMode(MipMode::Linear, true, false, "per-level linear-mip sampling repeats exactly");
  CheckMode(MipMode::Linear, true, true, "eight material sampler bindings repeat exactly");
  SDL_Quit();
  return Report();
}
