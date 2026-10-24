#ifndef OUTSHINE_BASE_SCENE_TEXTURE_H
#define OUTSHINE_BASE_SCENE_TEXTURE_H

#include <cstddef>
#include <cstdint>
#include <array>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include "UvTransform.h"
#include "ImageFormat.h"

namespace outshine {

enum class ImageMipKind : uint8_t { Linear, Colour, Normal };

using ImageMipData = std::array<std::optional<std::vector<uint8_t>>, 3>;

using ImageMipViews = std::array<std::optional<std::span<const uint8_t>>, 3>;

[[nodiscard]] inline ImageMipViews ViewImageMips(const ImageMipData &data) noexcept {
  ImageMipViews views;
  for (size_t i = 0; i < views.size(); ++i) {
    const auto &levels = data[i];
    if (levels) { views[i] = std::span<const uint8_t>(*levels); }
  }
  return views;
}

[[nodiscard]] inline ImageMipData CopyImageMips(const ImageMipViews &views) {
  ImageMipData data;
  for (size_t i = 0; i < data.size(); ++i) {
    const auto &levels = views[i];
    if (levels) { data[i].emplace(levels->begin(), levels->end()); }
  }
  return data;
}

[[nodiscard]] constexpr std::optional<size_t> LowerImageMipBytes(
    int width, int height, ImagePixelFormat format = ImagePixelFormat::Rgba8Unorm) noexcept {
  const size_t pixelBytes = ImagePixelBytes(format);
  if (width <= 0 || height <= 0 || pixelBytes == 0) { return std::nullopt; }
  size_t bytes = 0;
  while (width > 1 || height > 1) {
    width = width > 1 ? width / 2 : 1;
    height = height > 1 ? height / 2 : 1;
    if (static_cast<size_t>(width) >
        (std::numeric_limits<size_t>::max() - bytes) / pixelBytes / static_cast<size_t>(height)) {
      return std::nullopt;
    }
    bytes += static_cast<size_t>(width) * static_cast<size_t>(height) * pixelBytes;
  }
  return bytes;
}

enum class Filter : uint8_t { Nearest, Linear };

enum class MipFilter : uint8_t { None, Nearest, Linear };

enum class Wrap : uint8_t { ClampToEdge, MirroredRepeat, Repeat };

struct Sampler {
  Filter Magnify = Filter::Linear;
  Filter Minify = Filter::Linear;
  MipFilter Mip = MipFilter::Linear;
  Wrap WrapU = Wrap::Repeat;
  Wrap WrapV = Wrap::Repeat;

  [[nodiscard]] constexpr bool operator==(const Sampler &) const noexcept = default;
};

struct SurfaceMap {
  int Image = -1;
  UvSet Set = UvSet::Uv0;
  outshine::Sampler Sampler;
  UvTransformProperties Uv;

  [[nodiscard]] constexpr bool bound() const noexcept { return Image >= 0; }

  [[nodiscard]] bool operator==(const SurfaceMap &) const = default;
};

struct ImageView {
  int WidthPx = 0;
  int HeightPx = 0;
  ImagePixelFormat Format = ImagePixelFormat::Rgba8Unorm;
  std::span<const uint8_t> Rgba;
  ImageMipViews LowerMips{};

  [[nodiscard]] constexpr bool valid() const noexcept {
    const size_t pixelBytes = ImagePixelBytes(Format);
    if (WidthPx <= 0 || HeightPx <= 0 || pixelBytes == 0 ||
        Rgba.size() / pixelBytes / static_cast<size_t>(HeightPx) < static_cast<size_t>(WidthPx)) {
      return false;
    }
    std::optional<size_t> bytes;
    for (const auto &levels : LowerMips) {
      if (!levels) { continue; }
      if (!bytes) { bytes = LowerImageMipBytes(WidthPx, HeightPx, Format); }
      if (!bytes || levels->size() != *bytes) { return false; }
    }
    return true;
  }
};

}

#endif
