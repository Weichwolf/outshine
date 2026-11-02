#include "R8FieldTexture.h"
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <span>
#include <utility>

namespace outshine::Render {
namespace {
constexpr uint64_t kRowAlignment = 256;

uint64_t RowBytes(uint32_t side) noexcept {
  return ((uint64_t{side} + kRowAlignment - 1) / kRowAlignment) * kRowAlignment;
}
}

Native::Result<R8FieldStorage> R8FieldTextureStorage(std::span<const R8FieldMip> mips) {
  if (mips.empty() || !std::has_single_bit(mips.front().Side) ||
      std::cmp_greater(mips.size(), std::bit_width(mips.front().Side))) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "R8 field requires a dyadic mip range"});
  }
  uint32_t side = mips.front().Side;
  R8FieldStorage bytes;
  for (const auto &mip : mips) {
    const uint64_t texels = uint64_t{side} * side;
    if (mip.Side != side || mip.Pixels.size() != texels) {
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                           .Message = "R8 field mip bytes or order are invalid"});
    }
    bytes.TexelBytes += texels;
    bytes.TransferBytes += RowBytes(side) * side;
    side >>= 1;
  }
  return bytes;
}

Native::Result<R8FieldTexture> UploadR8FieldTexture(Native::Device &device,
                                                    Native::Commands &commands,
                                                    std::span<const R8FieldMip> mips,
                                                    R8FieldStorage limits) {
  const auto bytes = R8FieldTextureStorage(mips);
  if (!bytes) { return std::unexpected(bytes.error()); }
  constexpr auto use = Native::TextureUse::Sampled | Native::TextureUse::CopyDestination |
                       Native::TextureUse::CopySource;
  if (bytes->TexelBytes > limits.TexelBytes || bytes->TransferBytes > limits.TransferBytes ||
      mips.front().Side > device.Limits().MaxTextureExtent ||
      !device.Supports(Native::Format::R8Unorm, use, true)) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "R8 field exceeds admitted device storage"});
  }
  auto image = device.CreateTexture({.Width = mips.front().Side,
                                     .Height = mips.front().Side,
                                     .Mips = static_cast<uint32_t>(mips.size()),
                                     .Texels = Native::Format::R8Unorm,
                                     .Use = use});
  if (!image) { return std::unexpected(image.error()); }
  auto sampler = device.CreateSampler({.MaxLod = static_cast<float>(mips.size() - 1)});
  if (!sampler) { return std::unexpected(sampler.error()); }
  auto transfer = device.CreateBuffer({.Bytes = bytes->TransferBytes,
                                       .Use = Native::BufferUse::CopySource,
                                       .Placement = Native::Memory::Upload});
  if (!transfer) { return std::unexpected(transfer.error()); }
  auto mapped = device.MapWrite(**transfer, {.Bytes = bytes->TransferBytes});
  if (!mapped) { return std::unexpected(mapped.error()); }
  std::memset(mapped->data(), 0, mapped->size());
  size_t offset = 0;
  for (const auto &mip : mips) {
    const auto rowBytes = RowBytes(mip.Side);
    for (uint32_t row = 0; row < mip.Side; ++row) {
      std::memcpy(mapped->data() + offset + row * rowBytes,
                  mip.Pixels.data() + size_t{row} * mip.Side,
                  mip.Side);
    }
    offset += rowBytes * mip.Side;
  }
  const auto unmapped = device.Unmap(**transfer);
  if (!unmapped) { return std::unexpected(unmapped.error()); }
  uint64_t at = 0;
  for (size_t mip = 0; mip < mips.size(); ++mip) {
    const auto side = mips[mip].Side;
    const auto rowBytes = RowBytes(side);
    const auto copied = commands.Copy(
        **transfer,
        **image,
        {.BufferOffset = at,
         .RowBytes = rowBytes,
         .Region = {.Mip = static_cast<uint32_t>(mip), .Width = side, .Height = side}});
    if (!copied) { return std::unexpected(copied.error()); }
    at += rowBytes * side;
  }
  return R8FieldTexture{
      .Image = std::move(*image), .Sampling = std::move(*sampler), .Storage = *bytes};
}
}
