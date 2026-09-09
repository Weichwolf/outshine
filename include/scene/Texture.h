#ifndef OUTSHINE_TEXTURE_H
#define OUTSHINE_TEXTURE_H

#include <cstddef>
#include <cstdint>
#include <span>

#include "UvTransform.h"

namespace outshine {

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

  /// @return Whether dimensions are positive and the view contains every declared RGBA texel.
  /// Checks sizes without overflow, allocation or pixel access; cannot prove pointer lifetime.
  /// Extra trailing bytes are permitted. Geometry::addImage requires an exact byte count.
  [[nodiscard]] constexpr bool valid() const noexcept {
    return WidthPx > 0 && HeightPx > 0 &&
           Rgba.size() / 4u / static_cast<size_t>(HeightPx) >= static_cast<size_t>(WidthPx);
  }
};

}

#endif
