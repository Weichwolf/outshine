#include "GroundStorage.h"
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gpu.h>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <algorithm>
#include <chrono>
#include <optional>
#include <ratio>
#include <cstddef>
#include <cstring>
#include <limits>
#include <utility>

namespace outshine::Render {

namespace Says {
constexpr auto kGroundDeviceMissing = "ground storage requires an initialized GPU device";
constexpr auto kGroundStorageTooLarge = "ground storage exceeds SDL's upload byte range";
constexpr auto kGroundUploadAlreadyActive = "ground storage upload is already active";
constexpr auto kGroundUploadMissing = "ground storage upload has not begun";
}

namespace {

constexpr uint32_t kMinimumStorageBytes = 4 * sizeof(uint32_t);
constexpr uint32_t kMaximumUploadBytes = std::numeric_limits<uint32_t>::max();
constexpr uint32_t kTransferBytesMost = 1u << 20u;
static_assert(kTransferBytesMost % sizeof(uint32_t) == 0);
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

std::expected<void, std::string> GroundStorage::BeginReplace(SDL_GPUDevice *device,
                                                             std::span<const uint32_t> classes,
                                                             std::span<const float> palette,
                                                             GroundStorageUploadMetrics *metrics) {
  if (metrics != nullptr) { *metrics = {}; }
  if (Pending_) { return std::unexpected(Says::kGroundUploadAlreadyActive); }
  if (device == nullptr) { return std::unexpected(Says::kGroundDeviceMissing); }
  const auto classBytes = StorageBytes(classes.size());
  const auto paletteBytes = StorageBytes(palette.size());
  if (!classBytes || !paletteBytes) { return std::unexpected(Says::kGroundStorageTooLarge); }
  const auto allocationAt = std::chrono::steady_clock::now();
  PendingUpload pending;
  pending.Device = device;
  pending.ClassSource = classes;
  pending.PaletteSource = palette;
  pending.ClassBytes = *classBytes;
  pending.PaletteBytes = *paletteBytes;
  pending.TransferBytes = std::min(kTransferBytesMost, std::max(*classBytes, *paletteBytes));
  pending.Classes = Allocate(device, *classBytes);
  if (!pending.Classes) { return std::unexpected(SDL_GetError()); }
  pending.Palette = Allocate(device, *paletteBytes);
  if (!pending.Palette) { return std::unexpected(SDL_GetError()); }
  SDL_GPUTransferBufferCreateInfo info{};
  info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  info.size = pending.TransferBytes;
  pending.Transfer = OwnedTransfer(device, SDL_CreateGPUTransferBuffer(device, &info));
  if (!pending.Transfer) { return std::unexpected(SDL_GetError()); }
  Pending_.emplace(std::move(pending));
  if (metrics != nullptr) {
    metrics->AllocationMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - allocationAt)
            .count();
  }
  return {};
}

std::expected<bool, std::string> GroundStorage::AdvanceReplace(
    size_t bytesMost, const GpuSubmission &submission, GroundStorageUploadMetrics *metrics) {
  if (metrics != nullptr) { *metrics = {}; }
  if (!Pending_) { return std::unexpected(Says::kGroundUploadMissing); }
  PendingUpload &pending = *Pending_;
  if (pending.Fence) {
    if (!submission.QueryFence(submission.Context, pending.Device, pending.Fence.Get())) {
      return false;
    }
    pending.Fence.Reset();
  }
  if (pending.ClassOffset == pending.ClassBytes && pending.PaletteOffset == pending.PaletteBytes) {
    Classes_ = std::move(pending.Classes);
    Palette_ = std::move(pending.Palette);
    Pending_.reset();
    return true;
  }
  if (bytesMost < sizeof(uint32_t)) { return false; }
  const bool classRange = pending.ClassOffset != pending.ClassBytes;
  const uint32_t offset = classRange ? pending.ClassOffset : pending.PaletteOffset;
  const uint32_t rangeBytes = classRange ? pending.ClassBytes : pending.PaletteBytes;
  const size_t count = std::min({bytesMost,
                                 static_cast<size_t>(pending.TransferBytes),
                                 static_cast<size_t>(rangeBytes - offset)}) &
                       ~(sizeof(uint32_t) - 1u);
  if (count == 0) { return false; }
  const auto stagingAt = std::chrono::steady_clock::now();
  auto *mapped = static_cast<std::byte *>(
      submission.MapUpload(submission.Context, pending.Device, pending.Transfer.Get()));
  if (mapped == nullptr) {
    std::string error = SDL_GetError();
    Pending_.reset();
    return std::unexpected(std::move(error));
  }
  std::memset(mapped, 0, count);
  const std::span<const std::byte> source =
      classRange ? std::as_bytes(pending.ClassSource) : std::as_bytes(pending.PaletteSource);
  if (offset < source.size()) {
    std::memcpy(mapped, source.data() + offset, std::min(count, source.size() - offset));
  }
  SDL_UnmapGPUTransferBuffer(pending.Device, pending.Transfer.Get());
  if (metrics != nullptr) {
    metrics->StagingMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - stagingAt)
            .count();
  }
  const auto submitAt = std::chrono::steady_clock::now();
  SDL_GPUCommandBuffer *commands = submission.Acquire(submission.Context, pending.Device);
  if (commands == nullptr) {
    std::string error = SDL_GetError();
    Pending_.reset();
    return std::unexpected(std::move(error));
  }
  SDL_GPUCopyPass *pass = SDL_BeginGPUCopyPass(commands);
  if (pass == nullptr) {
    std::string error = SDL_GetError();
    (void)SDL_CancelGPUCommandBuffer(commands);
    Pending_.reset();
    return std::unexpected(std::move(error));
  }
  EncodeUpload(pass,
               pending.Transfer.Get(),
               {.buffer = classRange ? pending.Classes.Get() : pending.Palette.Get(),
                .offset = offset,
                .size = static_cast<uint32_t>(count)},
               0);
  SDL_EndGPUCopyPass(pass);
  SDL_GPUFence *fence = submission.Submit(submission.Context, commands);
  if (fence == nullptr) {
    std::string error = SDL_GetError();
    Pending_.reset();
    return std::unexpected(std::move(error));
  }
  pending.Fence = OwnedFence(pending.Device, fence);
  if (classRange) {
    pending.ClassOffset += static_cast<uint32_t>(count);
  } else {
    pending.PaletteOffset += static_cast<uint32_t>(count);
  }
  if (metrics != nullptr) {
    metrics->BytesSubmitted = count;
    metrics->SubmissionMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - submitAt)
            .count();
  }
  return false;
}

std::expected<void, std::string> GroundStorage::Replace(SDL_GPUDevice *device,
                                                        std::span<const uint32_t> classes,
                                                        std::span<const float> palette,
                                                        const GpuSubmission &submission,
                                                        GroundStorageUploadMetrics *metrics) {
  GroundStorageUploadMetrics allocation;
  auto began = BeginReplace(device, classes, palette, &allocation);
  if (metrics != nullptr) { *metrics = allocation; }
  if (!began) { return began; }
  while (true) {
    GroundStorageUploadMetrics slice;
    auto advanced = AdvanceReplace(kTransferBytesMost, submission, &slice);
    if (metrics != nullptr) {
      metrics->BytesSubmitted += slice.BytesSubmitted;
      metrics->StagingMs += slice.StagingMs;
      metrics->SubmissionMs += slice.SubmissionMs;
    }
    if (!advanced) { return std::unexpected(std::move(advanced.error())); }
    if (*advanced) { return {}; }
    if (Pending_ && Pending_->Fence) {
      SDL_GPUFence *fence = Pending_->Fence.Get();
      if (!submission.WaitFence(submission.Context, device, &fence, 1)) {
        std::string error = SDL_GetError();
        Pending_.reset();
        return std::unexpected(std::move(error));
      }
    }
  }
}

}
