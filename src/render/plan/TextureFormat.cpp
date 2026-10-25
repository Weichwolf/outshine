#include "TextureFormat.h"

namespace outshine::Render {
Native::Format TextureFormatOf(TexelFormat declared) noexcept {
  switch (declared) {
    case TexelFormat::Handle:
    case TexelFormat::Table: return Native::Format::Count;
    case TexelFormat::Rgba16Float: return Native::Format::Rgba16Float;
    case TexelFormat::Rgba32Float: return Native::Format::Rgba32Float;
    case TexelFormat::Rg16Float: return Native::Format::Rg16Float;
    case TexelFormat::R8Unorm: return Native::Format::R8Unorm;
    case TexelFormat::Rgba8UnormSrgb: return Native::Format::Rgba8Srgb;
    case TexelFormat::Depth32Float: return Native::Format::Depth32Float;
  }
  return Native::Format::Count;
}
}
