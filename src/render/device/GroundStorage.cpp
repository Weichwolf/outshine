#include "UploadQueue.h"
#include "GroundStorage.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <ratio>
#include <span>
#include <utility>

namespace outshine::Render {
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

Native::Result<std::unique_ptr<Native::Buffer>> Allocate(Native::Device &device, uint32_t bytes) {
  return device.CreateBuffer({.Bytes = bytes,
                              .Use = Native::BufferUse::Storage |
                                     Native::BufferUse::CopyDestination |
                                     Native::BufferUse::CopySource});
}
}

Native::Result<void> GroundStorage::BeginReplace(Native::Device *device,
                                                 std::span<const uint32_t> classes,
                                                 std::span<const float> palette,
                                                 GroundStorageUploadMetrics *metrics) {
  if (metrics != nullptr) { *metrics = {}; }
  if (Pending_ || device == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "ground upload is active or has no device"});
  }
  const auto classBytes = StorageBytes(classes.size());
  const auto paletteBytes = StorageBytes(palette.size());
  if (!classBytes || !paletteBytes) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "ground storage exceeds its 32-bit indexing"});
  }
  const auto start = metrics != nullptr ? std::chrono::steady_clock::now()
                                        : std::chrono::steady_clock::time_point{};
  PendingUpload pending;
  pending.Device = device;
  pending.Uploads = Uploads_;
  pending.ClassSource = classes;
  pending.PaletteSource = palette;
  pending.ClassBytes = *classBytes;
  pending.PaletteBytes = *paletteBytes;
  pending.TransferBytes = std::min(kTransferBytesMost, std::max(*classBytes, *paletteBytes));
  auto nextClasses = Allocate(*device, *classBytes);
  if (!nextClasses) { return std::unexpected(nextClasses.error()); }
  pending.Classes = std::move(*nextClasses);
  auto nextPalette = Allocate(*device, *paletteBytes);
  if (!nextPalette) { return std::unexpected(nextPalette.error()); }
  pending.Palette = std::move(*nextPalette);
  auto transfer = device->CreateBuffer({.Bytes = pending.TransferBytes,
                                        .Use = Native::BufferUse::CopySource,
                                        .Placement = Native::Memory::Upload});
  if (!transfer) { return std::unexpected(transfer.error()); }
  pending.Transfer = std::move(*transfer);
  Pending_.emplace(std::move(pending));
  if (metrics != nullptr) {
    metrics->AllocationMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  }
  return {};
}

Native::Result<void> GroundStorage::Stage(PendingUpload &pending,
                                          bool classes,
                                          uint32_t offset,
                                          size_t count,
                                          GroundStorageUploadMetrics *metrics) {
  const auto start = metrics != nullptr ? std::chrono::steady_clock::now()
                                        : std::chrono::steady_clock::time_point{};
  const auto mapped = pending.Device->MapWrite(*pending.Transfer, {.Bytes = count});
  if (!mapped) { return std::unexpected(mapped.error()); }
  std::memset(mapped->data(), 0, count);
  const auto source =
      classes ? std::as_bytes(pending.ClassSource) : std::as_bytes(pending.PaletteSource);
  if (offset < source.size()) {
    std::memcpy(mapped->data(), source.data() + offset, std::min(count, source.size() - offset));
  }
  const auto unmapped = pending.Device->Unmap(*pending.Transfer);
  if (!unmapped) { return unmapped; }
  if (metrics != nullptr) {
    metrics->StagingMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  }
  return {};
}

Native::Result<bool> GroundStorage::Fail(Native::Error error) {
  Pending_.reset();
  return std::unexpected(std::move(error));
}

Native::Result<bool> GroundStorage::PollRange(PendingUpload &pending) {
  if (!pending.Fence) { return true; }
  const auto ready = pending.Device->Poll(*pending.Fence);
  if (!ready) { return std::unexpected(ready.error()); }
  if (*ready == Native::CompletionStatus::Pending) { return false; }
  pending.Fence.reset();
  return true;
}

Native::Result<bool> GroundStorage::AdvanceReplace(size_t bytesMost,
                                                   GroundStorageUploadMetrics *metrics) {
  if (metrics != nullptr) { *metrics = {}; }
  if (!Pending_) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "ground upload has not begun"});
  }
  auto &pending = *Pending_;
  const auto ready = PollRange(pending);
  if (!ready) { return Fail(ready.error()); }
  if (!*ready) { return false; }
  if (pending.ClassOffset == pending.ClassBytes && pending.PaletteOffset == pending.PaletteBytes) {
    Classes_ = std::move(pending.Classes);
    Palette_ = std::move(pending.Palette);
    Pending_.reset();
    return true;
  }
  if (bytesMost < sizeof(uint32_t)) { return false; }
  const bool classes = pending.ClassOffset != pending.ClassBytes;
  const auto offset = classes ? pending.ClassOffset : pending.PaletteOffset;
  const auto rangeBytes = classes ? pending.ClassBytes : pending.PaletteBytes;
  const auto count =
      std::min({bytesMost, size_t{pending.TransferBytes}, size_t{rangeBytes - offset}}) &
      ~(sizeof(uint32_t) - 1u);
  if (count == 0) { return false; }
  const auto start = metrics != nullptr ? std::chrono::steady_clock::now()
                                        : std::chrono::steady_clock::time_point{};
  auto commands = BeginUpload(*pending.Device, pending.Uploads);
  if (!commands) {
    if (commands.error().Kind == Native::ErrorKind::Busy) { return false; }
    return Fail(commands.error());
  }
  const auto staged = Stage(pending, classes, offset, count, metrics);
  if (!staged) {
    if (staged.error().Kind == Native::ErrorKind::Busy) { return false; }
    return Fail(staged.error());
  }
  const auto copied = (*commands)->Copy(*pending.Transfer,
                                        classes ? *pending.Classes : *pending.Palette,
                                        {.DestinationOffset = offset, .Bytes = count});
  if (!copied) { return Fail(copied.error()); }
  const auto submitted = SubmitUpload(*pending.Device, pending.Uploads, std::move(*commands));
  if (!submitted) {
    if (submitted.error().Kind == Native::ErrorKind::Busy) { return false; }
    return Fail(submitted.error());
  }
  pending.Fence = *submitted;
  if (classes) {
    pending.ClassOffset += static_cast<uint32_t>(count);
  } else {
    pending.PaletteOffset += static_cast<uint32_t>(count);
  }
  if (metrics != nullptr) {
    metrics->BytesSubmitted = count;
    metrics->SubmissionMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
            .count() -
        metrics->StagingMs;
  }
  return false;
}

Native::Result<void> GroundStorage::Replace(Native::Device *device,
                                            std::span<const uint32_t> classes,
                                            std::span<const float> palette,
                                            std::chrono::nanoseconds budget,
                                            GroundStorageUploadMetrics *metrics) {
  if (budget <= std::chrono::nanoseconds::zero()) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "offline ground upload requires a wait budget"});
  }
  const auto started = std::chrono::steady_clock::now();
  GroundStorageUploadMetrics allocation;
  const auto began =
      BeginReplace(device, classes, palette, metrics != nullptr ? &allocation : nullptr);
  if (metrics != nullptr) { *metrics = allocation; }
  if (!began) { return began; }
  while (Pending_) {
    const auto elapsed = std::chrono::steady_clock::now() - started;
    if (elapsed >= budget) {
      Pending_.reset();
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Busy,
                                           .Message = "offline ground upload exceeded its budget"});
    }
    GroundStorageUploadMetrics slice;
    const auto advanced = AdvanceReplace(kTransferBytesMost, metrics != nullptr ? &slice : nullptr);
    if (metrics != nullptr) {
      metrics->BytesSubmitted += slice.BytesSubmitted;
      metrics->StagingMs += slice.StagingMs;
      metrics->SubmissionMs += slice.SubmissionMs;
    }
    if (!advanced) { return std::unexpected(advanced.error()); }
    if (*advanced) { return {}; }
    if (Pending_ && !Pending_->Fence) {
      Pending_.reset();
      return std::unexpected(
          Native::Error{.Kind = Native::ErrorKind::Busy,
                        .Message = "device back pressure prevents ground upload"});
    }
    if (Pending_ && Pending_->Fence) {
      const auto remaining = budget - (std::chrono::steady_clock::now() - started);
      const auto waited =
          device->Wait(*Pending_->Fence, std::max(remaining, decltype(remaining)::zero()));
      if (!waited) {
        auto error = waited.error();
        Pending_.reset();
        return std::unexpected(std::move(error));
      }
    }
  }
  return {};
}

}
