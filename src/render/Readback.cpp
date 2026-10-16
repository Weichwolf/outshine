#include "Readback.h"
#include <chrono>
#include <span>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace outshine::Render {
namespace {
constexpr auto kInspectionBudget = std::chrono::seconds(5);
}

Readback::Readback(Readback &&from) noexcept
    : Transfer_(std::move(from.Transfer_)),
      Device_(std::exchange(from.Device_, nullptr)),
      Mapped_(std::exchange(from.Mapped_, {})),
      Error_(std::move(from.Error_)) {
  from.Transfer_.reset();
}

Readback &Readback::operator=(Readback &&from) noexcept {
  if (this == &from) { return *this; }
  Release();
  Transfer_ = std::move(from.Transfer_);
  Device_ = std::exchange(from.Device_, nullptr);
  Mapped_ = std::exchange(from.Mapped_, {});
  Error_ = std::move(from.Error_);
  from.Transfer_.reset();
  return *this;
}

void Readback::Release() {
  Mapped_ = {};
  Transfer_.reset();
  Device_ = nullptr;
}

ReadState Readback::Begin(Native::Device &device, Native::Result<ReadbackTransfer> transfer) {
  if (!transfer) {
    Error_ = std::move(transfer.error().Message);
    return ReadState::Failed;
  }
  Device_ = &device;
  Transfer_.emplace(std::move(*transfer));
  return ReadState::Pending;
}

ReadState Readback::FromTexture(Native::Device *device,
                                Native::Texture *texture,
                                Extent size,
                                uint32_t texelBytes) {
  Release();
  Error_.clear();
  if (device == nullptr || texture == nullptr || size.WidthPx <= 0 || size.HeightPx <= 0 ||
      texelBytes == 0) {
    Error_ = "readback requires a device, image and positive pixel extent";
    return ReadState::Failed;
  }
  const auto begun =
      Begin(*device,
            ReadbackTransfer::Start(*device,
                                    *texture,
                                    {.Width = static_cast<uint32_t>(size.WidthPx),
                                     .Height = static_cast<uint32_t>(size.HeightPx)}));
  if (begun == ReadState::Failed || !Transfer_) { return ReadState::Failed; }
  if (Transfer_->RowBytes() != uint64_t{static_cast<uint32_t>(size.WidthPx)} * texelBytes) {
    Error_ = "readback pixel size disagrees with the native image format";
    Release();
    return ReadState::Failed;
  }
  return Land();
}

ReadState Readback::FromBuffer(Native::Device *device, Native::Buffer *source, uint32_t bytes) {
  const auto begun = Enqueue(device, source, bytes);
  return begun == ReadState::Failed ? begun : Land();
}

ReadState Readback::Enqueue(Native::Device *device, Native::Buffer *source, uint32_t bytes) {
  Release();
  Error_.clear();
  if (device == nullptr || source == nullptr) {
    Error_ = "readback requires a native device and buffer";
    return ReadState::Failed;
  }
  return Begin(*device, ReadbackTransfer::Start(*device, *source, {.Bytes = bytes}));
}

ReadState Readback::Map(Native::Result<std::optional<std::span<const std::byte>>> result) {
  if (!result) {
    Error_ = std::move(result.error().Message);
    Release();
    return ReadState::Failed;
  }
  if (!*result) { return ReadState::Pending; }
  Mapped_ = **result;
  return ReadState::Ready;
}

ReadState Readback::Land() {
  if (!Transfer_ || Device_ == nullptr) { return ReadState::Failed; }
  return Map(Transfer_->Wait(*Device_, kInspectionBudget));
}

ReadState Readback::Poll() {
  if (!Transfer_ || Device_ == nullptr) { return ReadState::Failed; }
  return Map(Transfer_->Poll(*Device_));
}
}
