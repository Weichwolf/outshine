#ifndef OUTSHINE_TEXTURE_H
#define OUTSHINE_TEXTURE_H

#include <cstddef>
#include <cstdint>
#include <array>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include "UvTransform.h"

namespace outshine {

/// Filtering interpretation of prepared RGBA8 mip levels.
enum class ImageMipKind : uint8_t {
  Linear, ///< Area-filtered linear channels, including alpha.
  Colour, ///< Linear-light filtering with sRGB-encoded RGB and linear alpha.
  Normal  ///< Unit directions with resultant length in alpha.
};

/// Owned levels below the base image, tightly packed from level one to one texel.
/// A missing entry needs preparation; an engaged empty entry completes a one-texel image.
using ImageMipData = std::array<std::optional<std::vector<uint8_t>>, 3>;
/// Borrowed counterpart of ImageMipData; backing allocations must outlive every use.
using ImageMipViews = std::array<std::optional<std::span<const uint8_t>>, 3>;

/// @return Borrowed views of each prepared interpretation; no allocation or pixel access.
[[nodiscard]] inline ImageMipViews ViewImageMips(const ImageMipData &data) noexcept {
  ImageMipViews views;
  for (size_t i = 0; i < views.size(); ++i) {
    const auto &levels = data[i];
    if (levels) { views[i] = std::span<const uint8_t>(*levels); }
  }
  return views;
}

/// @return Independent copies of all supplied prepared levels; may allocate.
[[nodiscard]] inline ImageMipData CopyImageMips(const ImageMipViews &views) {
  ImageMipData data;
  for (size_t i = 0; i < data.size(); ++i) {
    const auto &levels = views[i];
    if (levels) { data[i].emplace(levels->begin(), levels->end()); }
  }
  return data;
}

/// @return Exact bytes below the RGBA8 base, or no value for invalid dimensions/overflow.
/// Each dimension halves with floor rounding and a minimum of one; no allocation.
[[nodiscard]] constexpr std::optional<size_t> LowerImageMipBytes(int width, int height) noexcept {
  if (width <= 0 || height <= 0) { return std::nullopt; }
  size_t bytes = 0;
  while (width > 1 || height > 1) {
    width = width > 1 ? width / 2 : 1;
    height = height > 1 ? height / 2 : 1;
    if (static_cast<size_t>(width) >
        (std::numeric_limits<size_t>::max() - bytes) / 4u / static_cast<size_t>(height)) {
      return std::nullopt;
    }
    bytes += static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
  }
  return bytes;
}

/// Spatial filtering within one texture mip level.
enum class Filter : uint8_t {
  Nearest, ///< Select the nearest texel.
  Linear   ///< Interpolate neighbouring texels.
};
/// Selection and interpolation between texture mip levels.
enum class MipFilter : uint8_t {
  None,    ///< Sample only the base level.
  Nearest, ///< Select the nearest mip level.
  Linear   ///< Interpolate adjacent mip levels.
};
/// Addressing for normalized texture coordinates outside [0, 1].
enum class Wrap : uint8_t {
  ClampToEdge,    ///< Extend the edge texels.
  MirroredRepeat, ///< Repeat with alternating orientation at each integer boundary.
  Repeat          ///< Repeat at each integer boundary.
};

/// Value-only texture sampling policy; contains no GPU resource or owner reference.
/// Copies are independent. Concurrent reads are safe; synchronize mutation externally.
struct Sampler {
  Filter Magnify = Filter::Linear;   ///< Spatial filter when magnifying the texture.
  Filter Minify = Filter::Linear;    ///< Spatial filter when minifying the texture.
  MipFilter Mip = MipFilter::Linear; ///< Mip selection when minifying.
  Wrap WrapU = Wrap::Repeat;         ///< Horizontal addressing after the UV transform.
  Wrap WrapV = Wrap::Repeat;         ///< Vertical addressing after the UV transform.

  /// @return Whether all sampling settings match; constant time, no allocation.
  [[nodiscard]] constexpr bool operator==(const Sampler &) const noexcept = default;
};

/// Material texture binding into its owning geometry's image table.
/// The integer reference does not keep the owner alive. Copying between geometries
/// requires remapping Image. Reads are safe while the value and owner are immutable.
/// Colour-space and channel interpretation come from the material slot, not this binding.
struct SurfaceMap {
  int Image = -1;            ///< Owner-local image index; any negative value means unbound.
  UvSet Set = UvSet::Uv0;    ///< Vertex UV attribute used for sampling.
  outshine::Sampler Sampler; ///< Sampling policy applied to the transformed UVs.
  UvTransformProperties Uv;  ///< Scale, rotation and translation before addressing.

  /// @return Whether an image index is assigned; does not validate owner or index range.
  /// Constant time, no allocation or access to image storage.
  [[nodiscard]] constexpr bool bound() const noexcept { return Image >= 0; }

  /// @return Whether image index, UV selection, sampling and transform values match.
  /// Compares values only; does not compare the referenced image contents.
  [[nodiscard]] bool operator==(const SurfaceMap &) const = default;
};

/// Borrowed, tightly packed RGBA8 base-level image, with no row padding.
/// Channels occur in R, G, B, A order. Rows are stored consecutively; this view performs
/// no origin, alpha or colour-space conversion. Material usage determines interpretation.
/// The byte owner must outlive every use; its reallocation or destruction invalidates Rgba.
/// Concurrent reads require immutable backing bytes. Construction and copying allocate
/// nothing, copy no pixels and do not validate storage; call valid() before consumption.
struct ImageView {
  int WidthPx = 0;  ///< Width in pixels; nonpositive values describe an invalid image.
  int HeightPx = 0; ///< Height in pixels; nonpositive values describe an invalid image.
  std::span<const uint8_t> Rgba; ///< Borrowed bytes; any trailing bytes are outside this image.
  ImageMipViews LowerMips{};     ///< Optional prepared levels indexed by ImageMipKind.

  /// @return Whether dimensions are positive and the view contains every declared RGBA texel.
  /// Checks sizes without overflow, allocation or pixel access; cannot prove pointer lifetime.
  /// Extra trailing bytes are permitted. Geometry::addImage requires an exact byte count.
  [[nodiscard]] constexpr bool valid() const noexcept {
    if (WidthPx <= 0 || HeightPx <= 0 ||
        Rgba.size() / 4u / static_cast<size_t>(HeightPx) < static_cast<size_t>(WidthPx)) {
      return false;
    }
    std::optional<size_t> bytes;
    for (const auto &levels : LowerMips) {
      if (!levels) { continue; }
      if (!bytes) { bytes = LowerImageMipBytes(WidthPx, HeightPx); }
      if (!bytes || levels->size() != *bytes) { return false; }
    }
    return true;
  }
};

}

#endif
