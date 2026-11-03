#include "ClipmapHeightUpload.h"
#include "ClipmapHeightCache.h"
#include "device/native/Device.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>

namespace outshine::Render {
namespace {
uint32_t Wrap(int64_t node, uint32_t side) {
  const int64_t part = node % side;
  return static_cast<uint32_t>(part < 0 ? part + side : part);
}

Native::Result<void> CopyBand(Native::Commands &commands,
                              const Native::Buffer &source,
                              Native::Texture &target,
                              const ClipmapHeightRegion &region,
                              uint64_t byteOffset) {
  const uint32_t side = target.Description().Width;
  const uint32_t startX = Wrap(region.Low[0], side);
  const uint32_t startY = Wrap(region.Low[1], side);
  for (uint32_t y = 0; y < region.Height;) {
    const uint32_t destinationY = (startY + y) % side;
    const uint32_t height = std::min(region.Height - y, side - destinationY);
    for (uint32_t x = 0; x < region.Width;) {
      const uint32_t destinationX = (startX + x) % side;
      const uint32_t width = std::min(region.Width - x, side - destinationX);
      const auto copied = commands.Copy(
          source,
          target,
          {.BufferOffset = byteOffset + (uint64_t{y} * region.Width + x) * sizeof(float),
           .RowBytes = uint64_t{region.Width} * sizeof(float),
           .Region = {.Layer = region.Level,
                      .X = destinationX,
                      .Y = destinationY,
                      .Width = width,
                      .Height = height}});
      if (!copied) { return copied; }
      x += width;
    }
    y += height;
  }
  return {};
}
}

Native::Result<void> UploadClipmapHeights(Native::Device &device,
                                          Native::Commands &commands,
                                          Native::Texture &target,
                                          const ClipmapHeightCache &cache,
                                          const ClipmapHeightUpdate &update,
                                          size_t stagingBytesMost) {
  if (!cache.Validate(update)) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "clipmap height update is stale or invalid"});
  }
  if (update.Samples().empty()) { return {}; }
  const auto &description = target.Description();
  if (description.Type != Native::TextureType::Image2DArray ||
      description.Texels != Native::Format::R32Float || description.Width != cache.Side() ||
      description.Height != cache.Side() || description.Layers < cache.Grid().Levels) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "clipmap height texture does not match its grid"});
  }
  const size_t bytes = update.Samples().size_bytes();
  if (bytes > stagingBytesMost) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Busy,
                      .Message = "clipmap height upload exceeds staging admission"});
  }
  auto staging = device.CreateBuffer(
      {.Bytes = bytes, .Use = Native::BufferUse::CopySource, .Placement = Native::Memory::Upload});
  if (!staging) { return std::unexpected(staging.error()); }
  auto mapped = device.MapWrite(**staging, {.Bytes = bytes});
  if (!mapped) { return std::unexpected(mapped.error()); }
  std::memcpy(mapped->data(), update.Samples().data(), bytes);
  const auto unmapped = device.Unmap(**staging);
  if (!unmapped) { return unmapped; }
  uint64_t offset = 0;
  for (const auto &region : update.Regions()) {
    const auto copied = CopyBand(commands, **staging, target, region, offset);
    if (!copied) { return copied; }
    offset += uint64_t{region.Width} * region.Height * sizeof(float);
  }
  return {};
}
}
