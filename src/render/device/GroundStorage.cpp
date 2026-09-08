#include "GroundStorage.h"
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gpu.h>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <algorithm>
#include <optional>
#include <cstddef>
#include <cstring>
#include <limits>
#include <utility>

namespace outshine::Render {

namespace Says {
constexpr auto kGroundDeviceMissing = "ground storage requires an initialized GPU device";
constexpr auto kGroundStorageTooLarge = "ground storage exceeds SDL's upload byte range";
}

namespace {

constexpr uint32_t kMinimumStorageBytes = 4 * sizeof(uint32_t);
constexpr uint32_t kMaximumUploadBytes = std::numeric_limits<uint32_t>::max();
static_assert(sizeof(float) == sizeof(uint32_t));

constexpr std::optional<uint32_t> StorageBytes(size_t elements) {
  if (elements > kMaximumUploadBytes / sizeof(uint32_t)) { return std::nullopt; }
  return std::max(kMinimumStorageBytes, static_cast<uint32_t>(elements * sizeof(uint32_t)));
}

static_assert(StorageBytes(0) == kMinimumStorageBytes);
static_assert(StorageBytes(1) == kMinimumStorageBytes);
static_assert(StorageBytes(kMaximumUploadBytes / sizeof(uint32_t)).has_value());
static_assert(!StorageBytes(kMaximumUploadBytes / sizeof(uint32_t) + size_t{1}));
static_assert(!StorageBytes(std::numeric_limits<size_t>::max()));

OwnedBuffer Allocate(SDL_GPUDevice *device, uint32_t bytes) {
  SDL_GPUBufferCreateInfo info{};
  info.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
  info.size = bytes;
  return {device, SDL_CreateGPUBuffer(device, &info)};
}

void EncodeUpload(SDL_GPUCopyPass *pass,
                  SDL_GPUTransferBuffer *staging,
                  const SDL_GPUBufferRegion &target,
                  uint32_t sourceOffset) {
  const SDL_GPUTransferBufferLocation source{.transfer_buffer = staging, .offset = sourceOffset};
  SDL_UploadToGPUBuffer(pass, &source, &target, false);
}

}

std::expected<void, std::string> GroundStorage::Replace(SDL_GPUDevice *device,
                                                        std::span<const uint32_t> classes,
                                                        std::span<const float> palette,
                                                        const GpuSubmission &submission) {
  if (device == nullptr) { return std::unexpected(Says::kGroundDeviceMissing); }
  const auto classBytes = StorageBytes(classes.size());
  const auto paletteBytes = StorageBytes(palette.size());
  if (!classBytes || !paletteBytes || *classBytes > kMaximumUploadBytes - *paletteBytes) {
    return std::unexpected(Says::kGroundStorageTooLarge);
  }
  OwnedBuffer nextClasses = Allocate(device, *classBytes);
  if (!nextClasses) { return std::unexpected(SDL_GetError()); }
  OwnedBuffer nextPalette = Allocate(device, *paletteBytes);
  if (!nextPalette) { return std::unexpected(SDL_GetError()); }
  SDL_GPUTransferBufferCreateInfo info{};
  info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  info.size = *classBytes + *paletteBytes;
  const OwnedTransfer staging(device, SDL_CreateGPUTransferBuffer(device, &info));
  if (!staging) { return std::unexpected(SDL_GetError()); }
  auto *mapped =
      static_cast<std::byte *>(submission.MapUpload(submission.Context, device, staging.Get()));
  if (mapped == nullptr) { return std::unexpected(SDL_GetError()); }
  std::memset(mapped, 0, info.size);
  if (!classes.empty()) { std::memcpy(mapped, classes.data(), classes.size_bytes()); }
  if (!palette.empty()) { std::memcpy(mapped + *classBytes, palette.data(), palette.size_bytes()); }
  SDL_UnmapGPUTransferBuffer(device, staging.Get());
  SDL_GPUCommandBuffer *commands = submission.Acquire(submission.Context, device);
  if (commands == nullptr) { return std::unexpected(SDL_GetError()); }
  SDL_GPUCopyPass *pass = SDL_BeginGPUCopyPass(commands);
  if (pass == nullptr) {
    std::string error = SDL_GetError();
    (void)SDL_CancelGPUCommandBuffer(commands);
    return std::unexpected(std::move(error));
  }
  EncodeUpload(
      pass, staging.Get(), {.buffer = nextClasses.Get(), .offset = 0, .size = *classBytes}, 0);
  EncodeUpload(pass,
               staging.Get(),
               {.buffer = nextPalette.Get(), .offset = 0, .size = *paletteBytes},
               *classBytes);
  SDL_EndGPUCopyPass(pass);
  SDL_GPUFence *fence = submission.Submit(submission.Context, commands);
  if (fence == nullptr) { return std::unexpected(SDL_GetError()); }
  SDL_ReleaseGPUFence(device, fence);
  Classes_ = std::move(nextClasses);
  Palette_ = std::move(nextPalette);
  return {};
}

}
