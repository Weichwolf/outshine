#ifndef OUTSHINE_BASE_SCENE_IMAGEFORMAT_H
#define OUTSHINE_BASE_SCENE_IMAGEFORMAT_H

#include <cstddef>
#include <cstdint>

namespace outshine {

enum class ImagePixelFormat : uint8_t { Rgba8Unorm, Rgba16Float };

[[nodiscard]] constexpr size_t ImagePixelBytes(ImagePixelFormat format) noexcept {
  switch (format) {
    case ImagePixelFormat::Rgba8Unorm: return 4;
    case ImagePixelFormat::Rgba16Float: return 8;
  }
  return 0;
}

}

#endif
