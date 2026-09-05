#include "Readback.h"

#include "Log.h"
#include <cstdint>

#include <Extent.h>

namespace outshine::Render {

ReadState Readback::Submit(SDL_GPUCommandBuffer *commands) {
  Fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
  if (Fence == nullptr) {
    Log::Error(LogTag::Render, "readback_submit_failed", {{"msg", SDL_GetError()}});
    Release();
    return ReadState::Failed;
  }
  return ReadState::Pending;
}

ReadState Readback::Land(SDL_GPUCommandBuffer *commands) {
  if (Submit(commands) == ReadState::Failed) { return ReadState::Failed; }
  const bool waited = SDL_WaitForGPUFences(Device, true, &Fence, 1);
  SDL_ReleaseGPUFence(Device, Fence);
  Fence = nullptr;
  if (!waited) {
    Log::Error(LogTag::Render, "readback_wait_failed", {{"msg", SDL_GetError()}});
    Release();
    return ReadState::Failed;
  }
  return Map();
}

ReadState Readback::Poll() {
  if (Mapped != nullptr) { return ReadState::Ready; }
  if (Fence == nullptr) { return ReadState::Failed; }
  if (!SDL_QueryGPUFence(Device, Fence)) { return ReadState::Pending; }
  SDL_ReleaseGPUFence(Device, Fence);
  Fence = nullptr;
  return Map();
}

ReadState Readback::Map() {
  Mapped = static_cast<const uint8_t *>(SDL_MapGPUTransferBuffer(Device, Transfer, false));
  if (Mapped == nullptr) {
    Log::Error(LogTag::Render, "readback_map_failed", {{"msg", SDL_GetError()}});
    Release();
    return ReadState::Failed;
  }
  return ReadState::Ready;
}

ReadState Readback::FromTexture(SDL_GPUDevice *device,
                                SDL_GPUTexture *texture,
                                Extent size,
                                uint32_t texelBytes) {
  const auto width = static_cast<uint32_t>(size.WidthPx);
  const auto height = static_cast<uint32_t>(size.HeightPx);
  Release();
  Device = device;
  Row = width * texelBytes;
  SDL_GPUTransferBufferCreateInfo wanted{};
  wanted.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
  wanted.size = Row * height;
  Transfer = SDL_CreateGPUTransferBuffer(device, &wanted);
  if (Transfer == nullptr) {
    Log::Error(LogTag::Render, "readback_buffer_failed", {{"msg", SDL_GetError()}});
    Release();
    return ReadState::Failed;
  }

  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device);
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
  SDL_GPUTextureRegion region{};
  region.texture = texture;
  region.w = width;
  region.h = height;
  region.d = 1;
  SDL_GPUTextureTransferInfo into{};
  into.transfer_buffer = Transfer;
  into.pixels_per_row = width;
  into.rows_per_layer = height;
  SDL_DownloadFromGPUTexture(copy, &region, &into);
  SDL_EndGPUCopyPass(copy);
  return Land(commands);
}

ReadState Readback::FromBuffer(SDL_GPUDevice *device, SDL_GPUBuffer *source, uint32_t bytes) {
  return Copies(device, source, bytes) == ReadState::Failed ? ReadState::Failed : Land(Commands);
}

ReadState Readback::Enqueue(SDL_GPUDevice *device, SDL_GPUBuffer *source, uint32_t bytes) {
  return Copies(device, source, bytes) == ReadState::Failed ? ReadState::Failed : Submit(Commands);
}

ReadState Readback::Copies(SDL_GPUDevice *device, SDL_GPUBuffer *source, uint32_t bytes) {
  Release();
  Device = device;
  Row = bytes;
  SDL_GPUTransferBufferCreateInfo wanted{};
  wanted.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
  wanted.size = bytes;
  Transfer = SDL_CreateGPUTransferBuffer(device, &wanted);
  if (Transfer == nullptr) {
    Log::Error(LogTag::Render, "readback_buffer_failed", {{"msg", SDL_GetError()}});
    Release();
    return ReadState::Failed;
  }

  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device);
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
  SDL_GPUBufferRegion region{};
  region.buffer = source;
  region.size = bytes;
  SDL_GPUTransferBufferLocation into{};
  into.transfer_buffer = Transfer;
  SDL_DownloadFromGPUBuffer(copy, &region, &into);
  SDL_EndGPUCopyPass(copy);
  Commands = commands;
  return ReadState::Pending;
}

void Readback::Release() {
  if (Fence != nullptr) {
    SDL_WaitForGPUFences(Device, true, &Fence, 1);
    SDL_ReleaseGPUFence(Device, Fence);
    Fence = nullptr;
  }
  Commands = nullptr;
  if (Mapped != nullptr) { SDL_UnmapGPUTransferBuffer(Device, Transfer); }
  if (Transfer != nullptr) { SDL_ReleaseGPUTransferBuffer(Device, Transfer); }
  Mapped = nullptr;
  Transfer = nullptr;
  Row = 0;
}

} // namespace outshine::Render
