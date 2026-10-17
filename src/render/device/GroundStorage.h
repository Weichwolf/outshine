#ifndef OUTSHINE_RENDER_DEVICE_GROUNDSTORAGE_H
#define OUTSHINE_RENDER_DEVICE_GROUNDSTORAGE_H

#include "native/Device.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

namespace outshine::Render {
class UploadQueue;

struct GroundStorageUploadMetrics {
  size_t BytesSubmitted = 0;
  double AllocationMs = 0.0;
  double StagingMs = 0.0;
  double SubmissionMs = 0.0;
};

class GroundStorage {
public:
  void UploadsFrom(UploadQueue *queue) noexcept { Uploads_ = queue; }

  [[nodiscard]] Native::Result<void> BeginReplace(Native::Device *device,
                                                  std::span<const uint32_t> classes,
                                                  std::span<const float> palette,
                                                  GroundStorageUploadMetrics *metrics = nullptr);

  [[nodiscard]] Native::Result<bool> AdvanceReplace(size_t bytesMost,
                                                    GroundStorageUploadMetrics *metrics = nullptr);

  [[nodiscard]] Native::Result<void> Replace(Native::Device *device,
                                             std::span<const uint32_t> classes,
                                             std::span<const float> palette,
                                             std::chrono::nanoseconds budget,
                                             GroundStorageUploadMetrics *metrics = nullptr);

  [[nodiscard]] Native::Buffer *Classes() const noexcept { return Classes_.get(); }

  [[nodiscard]] Native::Buffer *Palette() const noexcept { return Palette_.get(); }

  [[nodiscard]] bool Ready() const noexcept { return Classes_ && Palette_; }

  [[nodiscard]] bool UploadActive() const noexcept { return Pending_.has_value(); }

private:
  [[nodiscard]] Native::Result<bool> Fail(Native::Error error);

  struct PendingUpload {
    Native::Device *Device = nullptr;
    UploadQueue *Uploads = nullptr;
    std::unique_ptr<Native::Buffer> Classes;
    std::unique_ptr<Native::Buffer> Palette;
    std::unique_ptr<Native::Buffer> Transfer;
    std::optional<Native::Completion> Fence;
    std::span<const uint32_t> ClassSource;
    std::span<const float> PaletteSource;
    uint32_t ClassBytes = 0;
    uint32_t PaletteBytes = 0;
    uint32_t TransferBytes = 0;
    uint32_t ClassOffset = 0;
    uint32_t PaletteOffset = 0;
  };

  [[nodiscard]] static Native::Result<bool> PollRange(PendingUpload &pending);

  [[nodiscard]] static Native::Result<void> Stage(PendingUpload &pending,
                                                  bool classes,
                                                  uint32_t offset,
                                                  size_t count,
                                                  GroundStorageUploadMetrics *metrics);

  UploadQueue *Uploads_ = nullptr;
  std::unique_ptr<Native::Buffer> Classes_;
  std::unique_ptr<Native::Buffer> Palette_;
  std::optional<PendingUpload> Pending_;
};

}
#endif
