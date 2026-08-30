#ifndef OUTSHINE_TEXTURE_H
#define OUTSHINE_TEXTURE_H

#include <cstdint>
#include <span>

#include "UvTransform.h"

namespace outshine {

enum class Filter : uint8_t { Nearest, Linear };
enum class MipFilter : uint8_t { None, Nearest, Linear };
enum class Wrap : uint8_t { ClampToEdge, MirroredRepeat, Repeat };

struct Sampler {
  Filter Magnify = Filter::Linear;
  Filter Minify = Filter::Linear;
  MipFilter Mip = MipFilter::Linear;
  Wrap WrapU = Wrap::Repeat;
  Wrap WrapV = Wrap::Repeat;

  [[nodiscard]] bool operator==(const Sampler &) const = default;
};

struct SurfaceMap {
  int Image = -1;
  UvSet Set = UvSet::First;
  Sampler Samples;
  UvTransformProperties Uv;

  [[nodiscard]] bool bound(void) const { return Image >= 0; }

  [[nodiscard]] bool operator==(const SurfaceMap &) const = default;
};

struct ImageView {
  int WidthPx = 0;
  int HeightPx = 0;
  std::span<const uint8_t> Rgba;

  [[nodiscard]] bool stands(void) const {
    return WidthPx > 0 && HeightPx > 0 && Rgba.size() >= (size_t)WidthPx * (size_t)HeightPx * 4u;
  }
};

}

#endif
