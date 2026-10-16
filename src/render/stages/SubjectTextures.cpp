#include "UploadQueue.h"
#include "SubjectResidency.h"
#include "shade/ImageMips.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Render {
namespace {
constexpr size_t kRgbaChannels = 4;

Native::Address AddressOf(SubjectWrap wrap) {
  switch (wrap) {
    case SubjectWrap::ClampToEdge: return Native::Address::ClampToEdge;
    case SubjectWrap::MirroredRepeat: return Native::Address::MirrorRepeat;
    case SubjectWrap::Repeat: return Native::Address::Repeat;
  }
  return Native::Address::Repeat;
}

Native::Filter FilterOf(SubjectFilter filter) {
  return filter == SubjectFilter::Nearest ? Native::Filter::Nearest : Native::Filter::Linear;
}

std::optional<uint32_t> MipChainBytes(Texels extent, uint32_t levels) {
  uint32_t width = extent.WidthPx;
  uint32_t height = extent.HeightPx;
  uint64_t total = 0;
  for (uint32_t level = 0; level < levels; ++level) {
    total += uint64_t{width} * height * kRgbaChannels;
    if (total > std::numeric_limits<uint32_t>::max()) { return std::nullopt; }
    width = std::max(1u, width / 2u);
    height = std::max(1u, height / 2u);
  }
  return static_cast<uint32_t>(total);
}
}

Native::Result<SubjectResidency::BoundImage>
SubjectResidency::Upload(const SubjectTexture &texture, Transfer decode, TexelKind kind) const {
  if (Device_ == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "image upload needs a device"});
  }
  if (texture.Rgba != nullptr) { return UploadImage(texture, decode, kind); }
  if (texture.Width > 1 || texture.Height > 1) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "a sized texture has no texel data"});
  }
  const DefaultImageKey key{.Decode = decode,
                            .Kind = kind,
                            .WrapU = texture.WrapU,
                            .WrapV = texture.WrapV,
                            .Magnify = texture.Magnify,
                            .Minify = texture.Minify,
                            .Mip = texture.Mip};
  const auto held = std::ranges::find(DefaultImages_, key, &DefaultImage::Key);
  if (held != DefaultImages_.end()) { return held->Image; }
  auto bound = UploadImage(texture, decode, kind);
  if (bound) { DefaultImages_.push_back({.Key = key, .Image = *bound}); }
  return bound;
}

Native::Result<SubjectResidency::BoundImage> SubjectResidency::UploadImage(
    const SubjectTexture &texture, Transfer decode, TexelKind kind) const {
  static const std::array<uint8_t, 4> white = {{255, 255, 255, 255}};
  const uint32_t width = texture.Width > 0 ? texture.Width : 1;
  const uint32_t height = texture.Height > 0 ? texture.Height : 1;
  const uint8_t *texels = texture.Rgba != nullptr ? texture.Rgba : white.data();
  if (width > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
      height > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
      (kind == TexelKind::Direction && decode != Transfer::Linear)) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "invalid texture extent or transfer"});
  }
  uint32_t levels = 1;
  for (uint32_t extent = std::max(width, height); extent > 1; extent /= 2) { ++levels; }
  if (texture.Mip == SubjectMip::None) { levels = 1; }
  if (!MipChainBytes({.WidthPx = width, .HeightPx = height}, levels)) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "texture mip chain exceeds transfer storage"});
  }
  auto image = Device_->CreateTexture(
      {.Width = width,
       .Height = height,
       .Mips = levels,
       .Texels = decode == Transfer::Srgb ? Native::Format::Rgba8Srgb : Native::Format::Rgba8Unorm,
       .Use = Native::TextureUse::Sampled | Native::TextureUse::CopyDestination |
              Native::TextureUse::CopySource});
  if (!image) { return std::unexpected(image.error()); }
  const ImageView pixels{.WidthPx = static_cast<int>(width),
                         .HeightPx = static_cast<int>(height),
                         .Rgba = {texels, static_cast<size_t>(width) * height * kRgbaChannels},
                         .LowerMips = texture.LowerMips};
  auto interpretation = decode == Transfer::Srgb ? ImageMipKind::Colour : ImageMipKind::Linear;
  if (kind == TexelKind::Direction) { interpretation = ImageMipKind::Normal; }
  const auto uploaded = UploadMipChain(*image, pixels, levels, interpretation);
  if (!uploaded) { return std::unexpected(uploaded.error()); }
  auto sample = Device_->CreateSampler(
      {.Min = FilterOf(texture.Minify),
       .Mag = FilterOf(texture.Magnify),
       .Mip = texture.Mip == SubjectMip::Nearest ? Native::Filter::Nearest : Native::Filter::Linear,
       .U = AddressOf(texture.WrapU),
       .V = AddressOf(texture.WrapV),
       .MinLod = 0,
       .MaxLod = static_cast<float>(levels - 1u)});
  if (!sample) { return std::unexpected(sample.error()); }
  return BoundImage{.Image = std::shared_ptr<Native::Texture>(std::move(*image)),
                    .Sample = std::shared_ptr<Native::Sampler>(std::move(*sample))};
}

Native::Result<void> SubjectResidency::UploadMipChain(std::unique_ptr<Native::Texture> &image,
                                                      ImageView pixels,
                                                      uint32_t levels,
                                                      ImageMipKind kind) const {
  const Texels extent{.WidthPx = static_cast<uint32_t>(pixels.WidthPx),
                      .HeightPx = static_cast<uint32_t>(pixels.HeightPx)};
  const auto chainBytes = MipChainBytes(extent, levels);
  if (!pixels.valid() || !chainBytes || levels == 0) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "invalid image or prepared mip chain"});
  }
  auto commands = BeginUpload(*Device_, Uploads_);
  if (!commands) { return std::unexpected(commands.error()); }
  auto lower = pixels.LowerMips[static_cast<size_t>(kind)];
  std::optional<std::vector<uint8_t>> generated;
  if (levels > 1 && !lower) {
    generated = Core::PrepareImageMips(pixels, kind);
    if (!generated) {
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                           .Message = "texture mip preparation failed"});
    }
    lower = std::span<const uint8_t>(*generated);
  }
  const auto baseBytes = size_t{extent.WidthPx} * extent.HeightPx * kRgbaChannels;
  if (levels > 1 && (!lower || lower->size() != *chainBytes - baseBytes)) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "mip levels do not match transfer storage"});
  }
  ++StagingAttempts_;
  ++UploadAttempts_;
  ++TotalUploadAttempts_;
  auto staging = Device_->CreateBuffer({.Bytes = *chainBytes,
                                        .Use = Native::BufferUse::CopySource,
                                        .Placement = Native::Memory::Upload});
  if (!staging) { return std::unexpected(staging.error()); }
  const auto mapped = Device_->MapWrite(**staging, {.Bytes = *chainBytes});
  if (!mapped) { return std::unexpected(mapped.error()); }
  std::memcpy(mapped->data(), pixels.Rgba.data(), baseBytes);
  if (kind == ImageMipKind::Normal) {
    for (size_t at = 3; at < baseBytes; at += kRgbaChannels) {
      (*mapped)[at] = std::byte{std::numeric_limits<uint8_t>::max()};
    }
  }
  if (levels > 1 && lower) {
    std::memcpy(mapped->data() + baseBytes, lower->data(), lower->size());
  }
  const auto unmapped = Device_->Unmap(**staging);
  if (!unmapped) { return unmapped; }
  uint32_t offset = 0;
  Texels size = extent;
  for (uint32_t mip = 0; mip < levels; ++mip) {
    const auto copied =
        (*commands)->Copy(**staging,
                          *image,
                          {.BufferOffset = offset,
                           .RowBytes = uint64_t{size.WidthPx} * kRgbaChannels,
                           .Region = {.Mip = mip, .Width = size.WidthPx, .Height = size.HeightPx}});
    if (!copied) { return copied; }
    offset += static_cast<uint32_t>(uint64_t{size.WidthPx} * size.HeightPx * kRgbaChannels);
    size.WidthPx = std::max(1u, size.WidthPx / 2u);
    size.HeightPx = std::max(1u, size.HeightPx / 2u);
  }
  const auto submitted = SubmitUpload(*Device_, Uploads_, std::move(*commands));
  if (!submitted) { return std::unexpected(submitted.error()); }
  UploadBytes_ += *chainBytes;
  return {};
}

}
