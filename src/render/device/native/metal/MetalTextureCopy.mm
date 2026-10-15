#include "MetalInternal.h"
#include <limits>

namespace outshine::Render::Native::Metal {
namespace {

bool FitsTransfer(const MetalBuffer &buffer,
                  const MetalTexture &texture,
                  TextureTransfer transfer) {
  const auto texels = Texels(texture.Description().Texels);
  if (!texels || !Inside(transfer.Region, texture.Description()) ||
      transfer.RowBytes < uint64_t{transfer.Region.Width} * texels->Bytes ||
      transfer.RowBytes % texels->Bytes != 0 || transfer.BufferOffset % texels->Bytes != 0 ||
      transfer.RowBytes > std::numeric_limits<uint64_t>::max() / transfer.Region.Height) {
    return false;
  }
  return Fits(
      {.Offset = transfer.BufferOffset, .Bytes = transfer.RowBytes * transfer.Region.Height},
      buffer.Description().Bytes);
}

bool Overlap(TextureRegion a, TextureRegion b) {
  return a.Mip == b.Mip && a.Layer == b.Layer && a.X < b.X + b.Width && b.X < a.X + a.Width &&
         a.Y < b.Y + b.Height && b.Y < a.Y + a.Height;
}

}

Result<void>
MetalCommands::Copy(const Buffer &source, Texture &destination, TextureTransfer region) {
  if (!OnOwner()) {
    return Failure(ErrorKind::Invalid, "Metal operation belongs to another thread");
  }
  @autoreleasepool {
    const auto *const from = dynamic_cast<const MetalBuffer *>(&source);
    const auto *const to = dynamic_cast<const MetalTexture *>(&destination);
    if (from == nullptr || to == nullptr || from->State->Device != Device || to->Device != Device ||
        !Has(source.Description().Use, BufferUse::CopySource) ||
        !Has(destination.Description().Use, TextureUse::CopyDestination) ||
        !FitsTransfer(*from, *to, region)) {
      return Failure(ErrorKind::Invalid, "invalid Metal buffer-to-texture transfer");
    }
    const auto held = Hold(from->State);
    if (!held) { return held; }
    Finish();
    const id<MTLBlitCommandEncoder> copy = [Native blitCommandEncoder];
    if (copy == nil) { return Failure(ErrorKind::Driver, "Metal texture upload encoder failed"); }
    [copy copyFromBuffer:from->State->Native
               sourceOffset:region.BufferOffset
          sourceBytesPerRow:region.RowBytes
        sourceBytesPerImage:region.RowBytes * region.Region.Height
                 sourceSize:MTLSizeMake(region.Region.Width, region.Region.Height, 1)
                  toTexture:to->Native
           destinationSlice:region.Region.Layer
           destinationLevel:region.Region.Mip
          destinationOrigin:MTLOriginMake(region.Region.X, region.Region.Y, 0)];
    [copy endEncoding];
    return {};
  }
}

Result<void>
MetalCommands::Copy(const Texture &source, Buffer &destination, TextureTransfer region) {
  if (!OnOwner()) {
    return Failure(ErrorKind::Invalid, "Metal operation belongs to another thread");
  }
  @autoreleasepool {
    const auto *const from = dynamic_cast<const MetalTexture *>(&source);
    const auto *const to = dynamic_cast<const MetalBuffer *>(&destination);
    if (from == nullptr || to == nullptr || from->Device != Device || to->State->Device != Device ||
        !Has(source.Description().Use, TextureUse::CopySource) ||
        !Has(destination.Description().Use, BufferUse::CopyDestination) ||
        !FitsTransfer(*to, *from, region)) {
      return Failure(ErrorKind::Invalid, "invalid Metal texture-to-buffer transfer");
    }
    const auto held = Hold(to->State);
    if (!held) { return held; }
    Finish();
    const id<MTLBlitCommandEncoder> copy = [Native blitCommandEncoder];
    if (copy == nil) { return Failure(ErrorKind::Driver, "Metal texture readback encoder failed"); }
    [copy copyFromTexture:from->Native
                     sourceSlice:region.Region.Layer
                     sourceLevel:region.Region.Mip
                    sourceOrigin:MTLOriginMake(region.Region.X, region.Region.Y, 0)
                      sourceSize:MTLSizeMake(region.Region.Width, region.Region.Height, 1)
                        toBuffer:to->State->Native
               destinationOffset:region.BufferOffset
          destinationBytesPerRow:region.RowBytes
        destinationBytesPerImage:region.RowBytes * region.Region.Height];
    [copy endEncoding];
    return {};
  }
}

Result<void> MetalCommands::Copy(const Texture &source,
                                 TextureRegion sourceRegion,
                                 Texture &destination,
                                 TextureRegion destinationRegion) {
  if (!OnOwner()) {
    return Failure(ErrorKind::Invalid, "Metal operation belongs to another thread");
  }
  @autoreleasepool {
    const auto *const from = dynamic_cast<const MetalTexture *>(&source);
    const auto *const to = dynamic_cast<const MetalTexture *>(&destination);
    if (from == nullptr || to == nullptr || from->Device != Device || to->Device != Device ||
        !Has(source.Description().Use, TextureUse::CopySource) ||
        !Has(destination.Description().Use, TextureUse::CopyDestination) ||
        source.Description().Texels != destination.Description().Texels ||
        sourceRegion.Width != destinationRegion.Width ||
        sourceRegion.Height != destinationRegion.Height ||
        !Inside(sourceRegion, source.Description()) ||
        !Inside(destinationRegion, destination.Description()) ||
        (from->Native == to->Native && Overlap(sourceRegion, destinationRegion))) {
      return Failure(ErrorKind::Invalid, "invalid Metal texture copy or overlapping subresources");
    }
    Finish();
    const id<MTLBlitCommandEncoder> copy = [Native blitCommandEncoder];
    if (copy == nil) { return Failure(ErrorKind::Driver, "Metal texture copy encoder failed"); }
    [copy copyFromTexture:from->Native
              sourceSlice:sourceRegion.Layer
              sourceLevel:sourceRegion.Mip
             sourceOrigin:MTLOriginMake(sourceRegion.X, sourceRegion.Y, 0)
               sourceSize:MTLSizeMake(sourceRegion.Width, sourceRegion.Height, 1)
                toTexture:to->Native
         destinationSlice:destinationRegion.Layer
         destinationLevel:destinationRegion.Mip
        destinationOrigin:MTLOriginMake(destinationRegion.X, destinationRegion.Y, 0)];
    [copy endEncoding];
    return {};
  }
}

}
