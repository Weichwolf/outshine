#ifndef OUTSHINE_CONTENT_SHADE_IMAGE_H
#define OUTSHINE_CONTENT_SHADE_IMAGE_H

#include <cstdint>
#include <vector>
#include "Texture.h"

namespace outshine::Core {

struct Raster {
  int Width = 0;
  int Height = 0;
  ImagePixelFormat Format = ImagePixelFormat::Rgba8Unorm;
  std::vector<uint8_t> Rgba;
  ImageMipData LowerMips{};

  [[nodiscard]] ImageView View() const {
    return {.WidthPx = Width,
            .HeightPx = Height,
            .Format = Format,
            .Rgba = Rgba,
            .LowerMips = ViewImageMips(LowerMips)};
  }

  [[nodiscard]] bool Holds() const {
    return View().valid() && Rgba.size() == static_cast<size_t>(Width) *
                                                static_cast<size_t>(Height) *
                                                ImagePixelBytes(Format);
  }
};

}
#endif
