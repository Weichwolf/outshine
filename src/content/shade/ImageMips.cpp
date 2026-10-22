#include "ImageMips.h"
#include "HalfTexels.h"
#include "TexelChain.h"
#include "math/Srgb.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include <utility>

namespace outshine::Core {
namespace {
constexpr float kByteSteps = 255.0f;

uint8_t EncodeChannel(float value, bool colour) {
  if (colour) { value = ColourSpace::SrgbFromLinear(value); }
  return static_cast<uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * kByteSteps));
}

std::optional<std::vector<float>> DecodeBase(ImageView image, ImageMipKind kind) {
  const size_t bytes = static_cast<size_t>(image.WidthPx) * image.HeightPx * 4u;
  std::vector<float> linear;
  if (image.Format == ImagePixelFormat::Rgba16Float) {
    auto decoded = UnpackHalfTexels({.WidthPx = static_cast<uint32_t>(image.WidthPx),
                                     .HeightPx = static_cast<uint32_t>(image.HeightPx)},
                                    image.Rgba);
    if (!decoded) { return std::nullopt; }
    linear = std::move(*decoded);
    if (kind == ImageMipKind::Normal) {
      for (size_t at = 3; at < bytes; at += 4) { linear[at] = 1.0f; }
    }
    return linear;
  }
  linear.resize(bytes);
  for (size_t at = 0; at < bytes; ++at) {
    const bool alpha = at % 4 == 3;
    const float value = static_cast<float>(image.Rgba[at]) / kByteSteps;
    linear[at] =
        kind == ImageMipKind::Colour && !alpha ? ColourSpace::LinearFromSrgb(value) : value;
    if (kind == ImageMipKind::Normal && alpha) { linear[at] = 1.0f; }
  }
  return linear;
}
}

std::optional<std::vector<uint8_t>> PrepareImageMips(ImageView image, ImageMipKind kind) {
  const auto bytes = LowerImageMipBytes(image.WidthPx, image.HeightPx, image.Format);
  if (!image.valid() || !bytes || static_cast<size_t>(kind) >= image.LowerMips.size()) {
    return std::nullopt;
  }
  std::vector<uint8_t> prepared;
  prepared.reserve(*bytes);
  if (*bytes == 0) { return prepared; }
  auto decoded = DecodeBase(image, kind);
  if (!decoded) { return std::nullopt; }
  auto linear = std::move(*decoded);
  Texels extent{.WidthPx = static_cast<uint32_t>(image.WidthPx),
                .HeightPx = static_cast<uint32_t>(image.HeightPx)};
  const auto filter = kind == ImageMipKind::Normal ? TexelKind::Direction : TexelKind::Value;
  std::vector<float> smaller;
  while (extent.WidthPx > 1 || extent.HeightPx > 1) {
    extent = HalveInPlace(linear, extent, smaller, filter);
    linear.swap(smaller);
    if (image.Format == ImagePixelFormat::Rgba16Float) {
      auto packed = PackHalfTexels(extent, linear);
      if (!packed) { return std::nullopt; }
      prepared.insert(prepared.end(), packed->begin(), packed->end());
      continue;
    }
    for (size_t at = 0; at < linear.size(); ++at) {
      prepared.push_back(EncodeChannel(linear[at], kind == ImageMipKind::Colour && at % 4 != 3));
    }
  }
  return prepared;
}
}
