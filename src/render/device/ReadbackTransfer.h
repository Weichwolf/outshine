#ifndef OUTSHINE_RENDER_DEVICE_READBACKTRANSFER_H
#define OUTSHINE_RENDER_DEVICE_READBACKTRANSFER_H

#include "native/Device.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

namespace outshine::Render {

struct TextureReadback {
  const Native::Texture *Source = nullptr;
  Native::TextureRegion Region;
};

struct ReadbackSlice {
  Native::ByteRange Range;
  uint64_t RowBytes = 0;
};

class ReadbackTransfer final {
public:
  ReadbackTransfer(ReadbackTransfer &&) noexcept = default;
  ReadbackTransfer &operator=(ReadbackTransfer &&) noexcept = default;
  ReadbackTransfer(const ReadbackTransfer &) = delete;
  ReadbackTransfer &operator=(const ReadbackTransfer &) = delete;

  [[nodiscard]] static Native::Result<ReadbackTransfer>
  Start(Native::Device &device, const Native::Texture &source, Native::TextureRegion region);
  [[nodiscard]] static Native::Result<ReadbackTransfer>
  Start(Native::Device &device, const Native::Buffer &source, Native::ByteRange range);
  [[nodiscard]] static Native::Result<ReadbackTransfer>
  Start(Native::Device &device,
        std::span<const TextureReadback> sources,
        std::span<ReadbackSlice> slices);

  [[nodiscard]] Native::Result<std::optional<std::span<const std::byte>>>
  Poll(Native::Device &device);
  [[nodiscard]] Native::Result<std::optional<std::span<const std::byte>>>
  Wait(Native::Device &device, std::chrono::nanoseconds budget);

  [[nodiscard]] uint64_t RowBytes() const noexcept { return RowBytes_; }

private:
  ReadbackTransfer() = default;
  [[nodiscard]] static Native::Result<ReadbackTransfer> Allocate(Native::Device &device,
                                                                 uint64_t bytes);
  [[nodiscard]] Native::Result<std::optional<std::span<const std::byte>>>
  Map(Native::Device &device, Native::CompletionStatus status);

  std::unique_ptr<Native::Buffer> Buffer_;
  Native::Completion Completion_;
  std::span<const std::byte> Mapped_;
  uint64_t RowBytes_ = 0;
};

}
#endif
