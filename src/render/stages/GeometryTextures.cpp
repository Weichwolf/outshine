#include "UploadQueue.h"
#include "GeometryResidency.h"
#include "ImageMips.h"
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
Native::Result<GeometryResidency::BoundImage> GeometryResidency::Bind(const RadianceImage &image,
                                                                      ImageUploadMode mode) {
  if (UploadCommands_ != nullptr) { mode = ImageUploadMode::Deferred; }
  if (Device_ == nullptr || image.Device != Device_ || !image.Texture || !image.Sampling ||
      image.Produced.Device == 0 || image.Produced.Serial == 0) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "resident image has no matching producer device"});
  }
  const auto &description = image.Texture->Description();
  if (description.Type != Native::TextureType::Image2D || description.Layers != 1 ||
      (description.Texels != Native::Format::Rgba16Float &&
       description.Texels != Native::Format::Rgba32Float) ||
      (static_cast<uint32_t>(description.Use) &
       static_cast<uint32_t>(Native::TextureUse::Sampled)) == 0) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "resident colour requires a sampled linear HDR image"});
  }
  if (mode == ImageUploadMode::Immediate) {
    const auto flushed = SubmitPendingUploads();
    if (!flushed) { return std::unexpected(flushed.error()); }
  }
  return BoundImage{.Image = image.Texture, .Sample = image.Sampling};
}

namespace {
Native::Address AddressOf(outshine::Wrap wrap) {
  switch (wrap) {
    case outshine::Wrap::ClampToEdge: return Native::Address::ClampToEdge;
    case outshine::Wrap::MirroredRepeat: return Native::Address::MirrorRepeat;
    case outshine::Wrap::Repeat: return Native::Address::Repeat;
  }
  return Native::Address::Repeat;
}

Native::Filter FilterOf(outshine::Filter filter) {
  return filter == outshine::Filter::Nearest ? Native::Filter::Nearest : Native::Filter::Linear;
}

std::optional<uint32_t> MipChainBytes(Texels extent, uint32_t levels, ImagePixelFormat format) {
  const auto pixelBytes = ImagePixelBytes(format);
  if (pixelBytes == 0) { return std::nullopt; }
  uint32_t width = extent.WidthPx;
  uint32_t height = extent.HeightPx;
  uint64_t total = 0;
  for (uint32_t level = 0; level < levels; ++level) {
    const uint64_t rowBytes = uint64_t{width} * pixelBytes;
    if (height == 0 || rowBytes > (std::numeric_limits<uint32_t>::max() - total) / height) {
      return std::nullopt;
    }
    total += rowBytes * height;
    width = std::max(1u, width / 2u);
    height = std::max(1u, height / 2u);
  }
  return static_cast<uint32_t>(total);
}
}

Native::Result<GeometryResidency::BoundImage> GeometryResidency::Upload(
    const MaterialTexture &texture, Transfer decode, TexelKind kind, ImageUploadMode mode) {
  if (UploadCommands_ != nullptr) { mode = ImageUploadMode::Deferred; }
  if (mode == ImageUploadMode::Immediate) {
    const auto flushed = SubmitPendingUploads();
    if (!flushed) { return std::unexpected(flushed.error()); }
  }
  if (Device_ == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "image upload needs a device"});
  }
  if (texture.Rgba != nullptr) { return UploadImage(texture, decode, kind, mode); }
  if (texture.Width > 1 || texture.Height > 1 || texture.Format != ImagePixelFormat::Rgba8Unorm) {
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
  auto bound = UploadImage(texture, decode, kind, mode);
  if (bound) { DefaultImages_.push_back({.Key = key, .Image = *bound}); }
  return bound;
}

Native::Result<GeometryResidency::BoundImage> GeometryResidency::UploadImage(
    const MaterialTexture &texture, Transfer decode, TexelKind kind, ImageUploadMode mode) {
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
  if (texture.Mip == outshine::MipFilter::None) { levels = 1; }
  if (!MipChainBytes({.WidthPx = width, .HeightPx = height}, levels, texture.Format)) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "texture mip chain exceeds transfer storage"});
  }
  const bool half = texture.Format == ImagePixelFormat::Rgba16Float;
  auto format = decode == Transfer::Srgb ? Native::Format::Rgba8Srgb : Native::Format::Rgba8Unorm;
  if (half) { format = Native::Format::Rgba16Float; }
  auto image = Device_->CreateTexture({.Width = width,
                                       .Height = height,
                                       .Mips = levels,
                                       .Texels = format,
                                       .Use = Native::TextureUse::Sampled |
                                              Native::TextureUse::CopyDestination |
                                              Native::TextureUse::CopySource});
  if (!image) { return std::unexpected(image.error()); }
  const ImageView pixels{
      .WidthPx = static_cast<int>(width),
      .HeightPx = static_cast<int>(height),
      .Format = texture.Format,
      .Rgba = {texels, static_cast<size_t>(width) * height * ImagePixelBytes(texture.Format)},
      .LowerMips = texture.LowerMips};
  auto interpretation =
      decode == Transfer::Srgb && !half ? ImageMipKind::Colour : ImageMipKind::Linear;
  if (kind == TexelKind::Direction) { interpretation = ImageMipKind::Normal; }
  std::shared_ptr<Native::Texture> shared = std::move(*image);
  const auto uploaded = UploadMipChain(shared, pixels, levels, interpretation, mode);
  if (!uploaded) { return std::unexpected(uploaded.error()); }
  auto sample = Device_->CreateSampler({.Min = FilterOf(texture.Minify),
                                        .Mag = FilterOf(texture.Magnify),
                                        .Mip = texture.Mip == outshine::MipFilter::Nearest
                                                   ? Native::Filter::Nearest
                                                   : Native::Filter::Linear,
                                        .U = AddressOf(texture.WrapU),
                                        .V = AddressOf(texture.WrapV),
                                        .MinLod = 0,
                                        .MaxLod = static_cast<float>(levels - 1u)});
  if (!sample) { return std::unexpected(sample.error()); }
  return BoundImage{.Image = std::move(shared),
                    .Sample = std::shared_ptr<Native::Sampler>(std::move(*sample))};
}

Native::Result<void>
GeometryResidency::UploadMipChain(const std::shared_ptr<Native::Texture> &image,
                                  ImageView pixels,
                                  uint32_t levels,
                                  ImageMipKind kind,
                                  ImageUploadMode mode) {
  const Texels extent{.WidthPx = static_cast<uint32_t>(pixels.WidthPx),
                      .HeightPx = static_cast<uint32_t>(pixels.HeightPx)};
  const auto chainBytes = MipChainBytes(extent, levels, pixels.Format);
  if (!pixels.valid() || !chainBytes || levels == 0) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "invalid image or prepared mip chain"});
  }
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
  const auto pixelBytes = ImagePixelBytes(pixels.Format);
  const auto baseBytes = size_t{extent.WidthPx} * extent.HeightPx * pixelBytes;
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
    constexpr uint16_t kHalfOne = 0x3c00;
    for (size_t at = 0; at < baseBytes; at += pixelBytes) {
      if (pixels.Format == ImagePixelFormat::Rgba16Float) {
        std::memcpy(mapped->data() + at + 3 * sizeof(uint16_t), &kHalfOne, sizeof(kHalfOne));
      } else {
        (*mapped)[at + 3] = std::byte{std::numeric_limits<uint8_t>::max()};
      }
    }
  }
  if (levels > 1 && lower) {
    std::memcpy(mapped->data() + baseBytes, lower->data(), lower->size());
  }
  const auto unmapped = Device_->Unmap(**staging);
  if (!unmapped) { return unmapped; }
  StagedImage staged{.Image = image,
                     .Staging = std::move(*staging),
                     .Levels = levels,
                     .PixelBytes = static_cast<uint32_t>(pixelBytes)};
  if (mode == ImageUploadMode::Deferred) {
    StagedImageAt_.emplace(image.get(), StagedImages_.size());
    StagedImages_.push_back(std::move(staged));
    return {};
  }
  auto commands = BeginUpload(*Device_, Uploads_);
  if (!commands) { return std::unexpected(commands.error()); }
  const auto copied = RecordImage(**commands, staged);
  if (!copied) { return copied; }
  const auto submitted = SubmitUpload(*Device_, Uploads_, std::move(*commands));
  if (!submitted) { return std::unexpected(submitted.error()); }
  UploadBytes_ += *chainBytes;
  return {};
}

Native::Result<void> GeometryResidency::RecordImage(Native::Commands &commands,
                                                    const StagedImage &image) {
  uint32_t offset = 0;
  auto size = Texels{.WidthPx = image.Image->Description().Width,
                     .HeightPx = image.Image->Description().Height};
  for (uint32_t mip = 0; mip < image.Levels; ++mip) {
    const auto copied =
        commands.Copy(*image.Staging,
                      *image.Image,
                      {.BufferOffset = offset,
                       .RowBytes = uint64_t{size.WidthPx} * image.PixelBytes,
                       .Region = {.Mip = mip, .Width = size.WidthPx, .Height = size.HeightPx}});
    if (!copied) { return copied; }
    offset += static_cast<uint32_t>(uint64_t{size.WidthPx} * size.HeightPx * image.PixelBytes);
    size.WidthPx = std::max(1u, size.WidthPx / 2u);
    size.HeightPx = std::max(1u, size.HeightPx / 2u);
  }
  return {};
}

void GeometryResidency::EraseStagedImage(size_t which) {
  StagedImageAt_.erase(StagedImages_[which].Image.get());
  if (which + 1 != StagedImages_.size()) {
    StagedImages_[which] = std::move(StagedImages_.back());
    StagedImageAt_[StagedImages_[which].Image.get()] = which;
  }
  StagedImages_.pop_back();
}

void GeometryResidency::DiscardUnusedImage(const std::shared_ptr<Native::Texture> &image) {
  if (!image || image.use_count() != 2) { return; }
  const auto found = StagedImageAt_.find(image.get());
  if (found != StagedImageAt_.end()) { EraseStagedImage(found->second); }
}

void GeometryResidency::DiscardUnreferencedImages() {
  size_t at = 0;
  while (at < StagedImages_.size()) {
    if (StagedImages_[at].Image.use_count() == 1) {
      EraseStagedImage(at);
    } else {
      ++at;
    }
  }
}

}
