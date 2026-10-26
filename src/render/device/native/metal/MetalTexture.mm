#include "MetalInternal.h"
#include <algorithm>
#include <bit>
#include <utility>

namespace outshine::Render::Native::Metal {

std::optional<TexelDescription> Texels(Format format) {
  constexpr std::array table{TexelDescription{.Native = MTLPixelFormatR8Unorm, .Bytes = 1},
                             TexelDescription{.Native = MTLPixelFormatRGBA8Unorm, .Bytes = 4},
                             TexelDescription{.Native = MTLPixelFormatRGBA8Unorm_sRGB, .Bytes = 4},
                             TexelDescription{.Native = MTLPixelFormatBGRA8Unorm, .Bytes = 4},
                             TexelDescription{.Native = MTLPixelFormatBGRA8Unorm_sRGB, .Bytes = 4},
                             TexelDescription{.Native = MTLPixelFormatRG16Float, .Bytes = 4},
                             TexelDescription{.Native = MTLPixelFormatRGBA16Float, .Bytes = 8},
                             TexelDescription{.Native = MTLPixelFormatR32Float, .Bytes = 4},
                             TexelDescription{.Native = MTLPixelFormatRGBA32Float, .Bytes = 16},
                             TexelDescription{.Native = MTLPixelFormatDepth32Float, .Bytes = 4}};
  const auto index = static_cast<size_t>(format);
  return index < table.size() ? std::optional(table[index]) : std::nullopt;
}

bool MetalDevice::Supports(Format format, UseMask<TextureUse> use, bool filtered) const {
  if (!OnOwner()) { return false; }
  constexpr uint32_t knownUses = (1u << 6u) - 1;
  const auto bits = static_cast<uint32_t>(use);
  if (!Texels(format) || bits == 0 || (bits & ~knownUses) != 0) { return false; }
  const bool depth = format == Format::Depth32Float;
  const bool srgb = format == Format::Rgba8Srgb || format == Format::Bgra8Srgb;
  const bool bgra = format == Format::Bgra8Unorm || format == Format::Bgra8Srgb;
  if ((depth && Has(use, TextureUse::ColourAttachment)) ||
      (!depth && Has(use, TextureUse::DepthAttachment))) {
    return false;
  }
  if (Has(use, TextureUse::Storage)) {
    if (depth || srgb || bgra) { return false; }
    const auto tier = Native_.readWriteTextureSupport;
    if (tier == MTLReadWriteTextureTierNone ||
        (tier == MTLReadWriteTextureTier1 && format != Format::R32Float)) {
      return false;
    }
  }
  if (filtered && (format == Format::R32Float || format == Format::Rgba32Float) &&
      !Native_.supports32BitFloatFiltering) {
    return false;
  }
  return true;
}

Result<std::unique_ptr<Texture>> MetalDevice::CreateTexture(TextureDescription description,
                                                            std::string_view label) {
  if (!OnOwner()) {
    return Failure(ErrorKind::Invalid, "Metal operation belongs to another thread");
  }
  @autoreleasepool {
    const auto texels = Texels(description.Texels);
    if (!texels || description.Width == 0 || description.Height == 0 ||
        description.Width > Limits_.MaxTextureExtent ||
        description.Height > Limits_.MaxTextureExtent || description.Mips == 0 ||
        std::cmp_greater(description.Mips,
                         std::bit_width(std::max(description.Width, description.Height))) ||
        description.Layers == 0 || description.Layers > Limits_.MaxTextureLayers ||
        (description.Type == TextureType::Image2D && description.Layers != 1)) {
      return Failure(ErrorKind::Invalid, "invalid Metal texture dimensions, mips or format");
    }
    if (description.Type != TextureType::Image2D && description.Type != TextureType::Image2DArray) {
      return Failure(ErrorKind::Invalid, "invalid Metal texture type");
    }
    if (!Supports(description.Texels, description.Use, false)) {
      return Failure(ErrorKind::Unsupported, "unsupported Metal texture usage combination");
    }
    MTLTextureDescriptor *const info = [MTLTextureDescriptor new];
    info.textureType =
        description.Type == TextureType::Image2D ? MTLTextureType2D : MTLTextureType2DArray;
    info.pixelFormat = texels->Native;
    info.width = description.Width;
    info.height = description.Height;
    info.mipmapLevelCount = description.Mips;
    info.arrayLength = description.Layers;
    info.storageMode = MTLStorageModePrivate;
    MTLTextureUsage usage = MTLTextureUsageUnknown;
    if (Has(description.Use, TextureUse::Sampled) || Has(description.Use, TextureUse::Storage)) {
      usage |= MTLTextureUsageShaderRead;
    }
    if (Has(description.Use, TextureUse::Storage)) { usage |= MTLTextureUsageShaderWrite; }
    if (Has(description.Use, TextureUse::ColourAttachment) ||
        Has(description.Use, TextureUse::DepthAttachment)) {
      usage |= MTLTextureUsageRenderTarget;
    }
    info.usage = usage;
    const id<MTLTexture> texture = [Native_ newTextureWithDescriptor:info];
    if (texture == nil) {
      return Failure(ErrorKind::OutOfMemory, "Metal texture allocation failed");
    }
    texture.label = Text(label);
    return std::make_unique<MetalTexture>(description, texture, Identity_);
  }
}

bool Inside(TextureRegion region, TextureDescription description) {
  if (region.Mip >= description.Mips || region.Layer >= description.Layers || region.Width == 0 ||
      region.Height == 0) {
    return false;
  }
  const uint32_t width = std::max(1u, description.Width >> region.Mip);
  const uint32_t height = std::max(1u, description.Height >> region.Mip);
  return region.X <= width && region.Width <= width - region.X && region.Y <= height &&
         region.Height <= height - region.Y;
}

Result<id<MTLTexture>> View(TextureView view, uint64_t device) {
  const auto *const texture = dynamic_cast<const MetalTexture *>(view.Image);
  if (texture == nullptr || texture->Device != device) {
    return Failure(ErrorKind::Invalid, "Metal texture view belongs to a different device");
  }
  const auto &description = texture->Description();
  if (view.Mips == 0 || view.Layers == 0 || view.FirstMip >= description.Mips ||
      view.Mips > description.Mips - view.FirstMip || view.FirstLayer >= description.Layers ||
      view.Layers > description.Layers - view.FirstLayer) {
    return Failure(ErrorKind::Invalid, "Metal texture view exceeds its subresource range");
  }
  if (view.FirstMip == 0 && view.Mips == description.Mips && view.FirstLayer == 0 &&
      view.Layers == description.Layers) {
    return texture->Native;
  }
  const id<MTLTexture> selected =
      [texture->Native newTextureViewWithPixelFormat:texture->Native.pixelFormat
                                         textureType:texture->Native.textureType
                                              levels:NSMakeRange(view.FirstMip, view.Mips)
                                              slices:NSMakeRange(view.FirstLayer, view.Layers)];
  if (selected == nil) {
    return Failure(ErrorKind::Driver, "Metal subresource view creation failed");
  }
  return selected;
}

}
