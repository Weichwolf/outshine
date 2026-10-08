#include "Check.h"
#include "GpuOwned.h"
#include "SubjectResidency.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <optional>
#include <vector>

using namespace outshine;
using namespace outshine::Render;
using outshine::Test::Report;

namespace {
std::optional<std::vector<uint8_t>>
ReadLevel(SDL_GPUDevice *device, SDL_GPUTexture *image, uint32_t level, uint32_t size) {
  const SDL_GPUTransferBufferCreateInfo wanted{.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
                                               .size = size * size * 4u};
  const OwnedTransfer transfer(device, SDL_CreateGPUTransferBuffer(device, &wanted));
  if (!transfer) { return std::nullopt; }
  auto *commands = SDL_AcquireGPUCommandBuffer(device);
  if (!commands) { return std::nullopt; }
  auto *copy = SDL_BeginGPUCopyPass(commands);
  if (!copy) {
    SDL_CancelGPUCommandBuffer(commands);
    return std::nullopt;
  }
  const SDL_GPUTextureRegion source{
      .texture = image, .mip_level = level, .w = size, .h = size, .d = 1};
  const SDL_GPUTextureTransferInfo destination{
      .transfer_buffer = transfer.Get(), .pixels_per_row = size, .rows_per_layer = size};
  SDL_DownloadFromGPUTexture(copy, &source, &destination);
  SDL_EndGPUCopyPass(copy);
  const OwnedFence fence(device, SDL_SubmitGPUCommandBufferAndAcquireFence(commands));
  auto *handle = fence.Get();
  if (!fence || !SDL_WaitForGPUFences(device, true, &handle, 1)) { return std::nullopt; }
  const auto *mapped =
      static_cast<const uint8_t *>(SDL_MapGPUTransferBuffer(device, transfer.Get(), false));
  if (!mapped) { return std::nullopt; }
  std::vector<uint8_t> bytes(mapped, mapped + wanted.size);
  SDL_UnmapGPUTransferBuffer(device, transfer.Get());
  return bytes;
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    const OwnedDevice device(SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL,
        false,
        nullptr));
    CHECK(device, "GPU device opens");
    if (!device) { return Report(); }
    SubjectResidency residency;
    residency.StandsOn(device.Get());
    const std::array<uint8_t, 16> base{
        255, 255, 255, 7, 255, 255, 255, 7, 255, 255, 255, 7, 255, 255, 255, 7};
    ImageMipData prepared;
    prepared[1] = std::vector<uint8_t>{20, 40, 60, 80};
    prepared[2] = std::vector<uint8_t>{120, 130, 255, 100};
    SubjectTexture texture{
        .Rgba = base.data(), .Width = 2, .Height = 2, .LowerMips = ViewImageMips(prepared)};
    const auto colour =
        residency.Upload(texture, SubjectResidency::Transfer::Srgb, TexelKind::Value);
    CHECK(colour, "prepared colour image uploads");
    if (!colour) { return Report(); }
    const auto colourMip = ReadLevel(device.Get(), colour->Image.Get(), 1, 1);
    CHECK(colourMip && *colourMip == *prepared[1],
          "GPU receives stored colour levels rather than re-filtering the white base");
    const auto normal =
        residency.Upload(texture, SubjectResidency::Transfer::Linear, TexelKind::Direction);
    CHECK(normal, "prepared normal image uploads");
    if (!normal) { return Report(); }
    const auto normalMip = ReadLevel(device.Get(), normal->Image.Get(), 1, 1);
    CHECK(normalMip && *normalMip == *prepared[2],
          "normal confidence bytes reach the GPU unchanged");
    const auto normalBase = ReadLevel(device.Get(), normal->Image.Get(), 0, 2);
    CHECK(normalBase && (*normalBase)[3] == 255 && (*normalBase)[15] == 255,
          "normal base texels retain the existing unit-confidence contract");
    static_cast<void>(residency.TakeUploadBytes());
    texture.Mip = SubjectMip::None;
    const auto single =
        residency.Upload(texture, SubjectResidency::Transfer::Srgb, TexelKind::Value);
    CHECK(single && residency.TakeUploadBytes() == 16,
          "disabled mip sampling uploads only the base");
    prepared[1]->pop_back();
    texture.LowerMips = ViewImageMips(prepared);
    CHECK(!residency.Upload(texture, SubjectResidency::Transfer::Srgb, TexelKind::Value),
          "malformed prepared levels cannot publish a sampled texture");
  }
  SDL_Quit();
  return Report();
}
