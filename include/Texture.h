#ifndef OUTSHINE_TEXTURE_H
#define OUTSHINE_TEXTURE_H

#include <cstdint>
#include <span>

#include "UvTransform.h"

namespace outshine {

// WHAT A SURFACE WEARS, WHICH THIS DOOR COULD NOT SAY. `Material` is a row of numbers and a
// `Geometry` carried no images, so a client loading somebody else's asset could see its geometry
// and its material constants and nothing about the PICTURES that make it look like the thing it
// is. Filament's answer is a `Texture` a `MaterialInstance` takes as a parameter; glTF's own
// material carries texture REFERENCES rather than pixels, and that split is the real one -- the
// door's `Material` names a map, and the engine keeps the raster it decoded for the device.
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

// THE PIXELS, RGBA AND ROW-MAJOR, borrowed rather than owned: they live in the `Geometry` that
// hands them out, and a view of them costs nothing to take.
struct ImageView {
  int WidthPx = 0;
  int HeightPx = 0;
  std::span<const uint8_t> Rgba;

  [[nodiscard]] bool stands(void) const {
    return WidthPx > 0 && HeightPx > 0 && Rgba.size() >= (size_t)WidthPx * (size_t)HeightPx * 4u;
  }
};

} // namespace outshine

#endif
