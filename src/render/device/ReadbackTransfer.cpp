#include "ReadbackTransfer.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <utility>

namespace outshine::Render {

namespace {

uint64_t TexelBytes(Native::Format format) {
  using enum Native::Format;
  switch (format) {
    case R8Unorm: return 1;
    case Rgba8Unorm:
    case Rgba8Srgb:
    case Bgra8Unorm:
    case Bgra8Srgb:
    case Rg16Float:
    case R32Float:
    case Depth32Float: return sizeof(uint32_t);
    case Rgba16Float: return 4 * sizeof(uint16_t);
    case Rgba32Float: return 4 * sizeof(float);
    case Count: return 0;
  }
  return 0;
}

Native::Result<uint64_t> TextureRow(const Native::Texture &source, Native::TextureRegion region) {
  const auto &image = source.Description();
  const auto pixelBytes = TexelBytes(image.Texels);
  if (pixelBytes == 0 || region.Mip >= image.Mips ||
      region.Mip >= std::numeric_limits<uint32_t>::digits || region.Layer >= image.Layers ||
      region.Width == 0 || region.Height == 0 ||
      (static_cast<uint32_t>(image.Use) & static_cast<uint32_t>(Native::TextureUse::CopySource)) ==
          0) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "invalid readback image region"});
  }
  const auto width = std::max(1u, image.Width >> region.Mip);
  const auto height = std::max(1u, image.Height >> region.Mip);
  if (region.X > width || region.Width > width - region.X || region.Y > height ||
      region.Height > height - region.Y) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "readback image region exceeds mip"});
  }
  return uint64_t{region.Width} * pixelBytes;
}

}

Native::Result<ReadbackTransfer> ReadbackTransfer::Allocate(Native::Device &device,
                                                            uint64_t bytes) {
  if (bytes == 0 || bytes > std::numeric_limits<size_t>::max()) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "readback byte size is invalid"});
  }
  auto buffer = device.CreateBuffer({.Bytes = bytes,
                                     .Use = Native::BufferUse::CopyDestination,
                                     .Placement = Native::Memory::Readback});
  if (!buffer) { return std::unexpected(buffer.error()); }
  ReadbackTransfer result;
  result.Buffer_ = std::move(*buffer);
  result.RowBytes_ = bytes;
  return result;
}

Native::Result<ReadbackTransfer> ReadbackTransfer::Start(Native::Device &device,
                                                         const Native::Texture &source,
                                                         Native::TextureRegion region) {
  const auto row = TextureRow(source, region);
  if (!row) { return std::unexpected(row.error()); }
  if (*row > std::numeric_limits<uint64_t>::max() / region.Height) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "readback image size overflow"});
  }
  auto transfer = Allocate(device, *row * region.Height);
  if (!transfer) { return std::unexpected(transfer.error()); }
  transfer->RowBytes_ = *row;
  auto commands = device.Begin();
  if (!commands) { return std::unexpected(commands.error()); }
  const auto copied =
      (*commands)->Copy(source, *transfer->Buffer_, {.RowBytes = *row, .Region = region});
  if (!copied) { return std::unexpected(copied.error()); }
  const auto submitted = device.Submit(std::move(*commands));
  if (!submitted) { return std::unexpected(submitted.error()); }
  transfer->Completion_ = *submitted;
  return std::move(*transfer);
}

Native::Result<ReadbackTransfer> ReadbackTransfer::Start(Native::Device &device,
                                                         const Native::Buffer &source,
                                                         Native::ByteRange range) {
  if (range.Bytes == 0 || range.Offset > source.Description().Bytes ||
      range.Bytes > source.Description().Bytes - range.Offset ||
      (static_cast<uint32_t>(source.Description().Use) &
       static_cast<uint32_t>(Native::BufferUse::CopySource)) == 0) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "invalid readback buffer range"});
  }
  auto transfer = Allocate(device, range.Bytes);
  if (!transfer) { return std::unexpected(transfer.error()); }
  auto commands = device.Begin();
  if (!commands) { return std::unexpected(commands.error()); }
  const auto copied = (*commands)->Copy(
      source, *transfer->Buffer_, {.SourceOffset = range.Offset, .Bytes = range.Bytes});
  if (!copied) { return std::unexpected(copied.error()); }
  const auto submitted = device.Submit(std::move(*commands));
  if (!submitted) { return std::unexpected(submitted.error()); }
  transfer->Completion_ = *submitted;
  return std::move(*transfer);
}

Native::Result<std::optional<std::span<const std::byte>>>
ReadbackTransfer::Map(Native::Device &device, Native::CompletionStatus status) {
  if (status == Native::CompletionStatus::Pending) { return std::nullopt; }
  if (Mapped_.data() == nullptr) {
    const auto mapped = device.MapRead(*Buffer_, {.Bytes = Buffer_->Description().Bytes});
    if (!mapped) { return std::unexpected(mapped.error()); }
    Mapped_ = *mapped;
  }
  return Mapped_;
}

Native::Result<std::optional<std::span<const std::byte>>>
ReadbackTransfer::Poll(Native::Device &device) {
  if (!Buffer_) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = "readback has been moved"});
  }
  const auto status = device.Poll(Completion_);
  if (!status) { return std::unexpected(status.error()); }
  return Map(device, *status);
}

Native::Result<std::optional<std::span<const std::byte>>>
ReadbackTransfer::Wait(Native::Device &device, std::chrono::nanoseconds budget) {
  if (!Buffer_) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = "readback has been moved"});
  }
  const auto status = device.Wait(Completion_, budget);
  if (!status) { return std::unexpected(status.error()); }
  return Map(device, *status);
}

}
