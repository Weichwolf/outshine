#ifndef OUTSHINE_RENDER_DEVICE_GROUNDSTORAGE_H
#define OUTSHINE_RENDER_DEVICE_GROUNDSTORAGE_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include "GpuOwned.h"
#include "GpuSubmission.h"

namespace outshine::Render {

struct GroundStorageUploadMetrics {
  size_t BytesSubmitted = 0;
  double AllocationMs = 0.0;
  double StagingMs = 0.0;
  double SubmissionMs = 0.0;
};

class GroundStorage {
public:
  [[nodiscard]] std::expected<void, std::string>
  BeginReplace(SDL_GPUDevice *device,
               std::span<const uint32_t> classes,
               std::span<const float> palette,
               GroundStorageUploadMetrics *metrics = nullptr);

  [[nodiscard]] std::expected<bool, std::string>
  AdvanceReplace(size_t bytesMost,
                 const GpuSubmission &submission = {},
                 GroundStorageUploadMetrics *metrics = nullptr);

  [[nodiscard]] std::expected<void, std::string>
  Replace(SDL_GPUDevice *device,
          std::span<const uint32_t> classes,
          std::span<const float> palette,
          const GpuSubmission &submission = {},
          GroundStorageUploadMetrics *metrics = nullptr);

  [[nodiscard]] SDL_GPUBuffer *Classes() const noexcept { return Classes_.Get(); }

  [[nodiscard]] SDL_GPUBuffer *Palette() const noexcept { return Palette_.Get(); }

  [[nodiscard]] bool Ready() const noexcept { return Classes_ && Palette_; }

  [[nodiscard]] bool UploadActive() const noexcept { return Pending_.has_value(); }

private:
  struct PendingUpload {
    SDL_GPUDevice *Device = nullptr;
    OwnedBuffer Classes;
    OwnedBuffer Palette;
    OwnedTransfer Transfer;
    OwnedFence Fence;
    std::span<const uint32_t> ClassSource;
    std::span<const float> PaletteSource;
    uint32_t ClassBytes = 0;
    uint32_t PaletteBytes = 0;
    uint32_t TransferBytes = 0;
    uint32_t ClassOffset = 0;
    uint32_t PaletteOffset = 0;
  };

  OwnedBuffer Classes_;
  OwnedBuffer Palette_;
  std::optional<PendingUpload> Pending_;
};

}
#endif
