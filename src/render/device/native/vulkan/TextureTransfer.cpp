#ifdef __linux__
#include "Recording.h"
#include <algorithm>
#include <cstdint>
#include <expected>
#include <limits>

namespace outshine::Render::Native::Vulkan {
namespace {

template <typename Flag> bool Has(UseMask<Flag> use, Flag bit) {
  return (static_cast<uint32_t>(use) & static_cast<uint32_t>(bit)) != 0;
}

TextureView View(TextureRegion region) {
  return {.FirstMip = region.Mip, .Mips = 1, .FirstLayer = region.Layer, .Layers = 1};
}

VkImageSubresourceLayers Layers(const ImageState &image, TextureRegion region) {
  return {.aspectMask = Aspect(image.Description.Texels),
          .mipLevel = region.Mip,
          .baseArrayLayer = region.Layer,
          .layerCount = 1};
}

Result<ByteRange>
TransferRange(const ImageState &image, const BufferState &buffer, TextureTransfer transfer) {
  if (!ValidRegion(image, transfer.Region)) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "invalid texture transfer region"});
  }
  const uint64_t bytes = TexelBytes(image.Description.Texels);
  const uint64_t rowBytes = transfer.Region.Width * bytes;
  const uint64_t stride = transfer.RowBytes == 0 ? rowBytes : transfer.RowBytes;
  const uint64_t lastRow = transfer.Region.Height - 1u;
  if (transfer.BufferOffset % bytes != 0 || stride < rowBytes || stride % bytes != 0 ||
      stride > static_cast<uint64_t>(std::numeric_limits<int32_t>::max()) ||
      (lastRow != 0 && stride > (std::numeric_limits<uint64_t>::max() - rowBytes) / lastRow)) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "invalid texture transfer pitch or offset"});
  }
  const ByteRange range{.Offset = transfer.BufferOffset, .Bytes = stride * lastRow + rowBytes};
  if (!Contains(buffer.Description.Bytes, range)) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "texture transfer exceeds buffer"});
  }
  return range;
}

VkBufferImageCopy CopyRegion(const ImageState &image, TextureTransfer transfer) {
  return {.bufferOffset = transfer.BufferOffset,
          .bufferRowLength =
              static_cast<uint32_t>(transfer.RowBytes / TexelBytes(image.Description.Texels)),
          .bufferImageHeight = 0,
          .imageSubresource = Layers(image, transfer.Region),
          .imageOffset = {.x = static_cast<int32_t>(transfer.Region.X),
                          .y = static_cast<int32_t>(transfer.Region.Y),
                          .z = 0},
          .imageExtent = {
              .width = transfer.Region.Width, .height = transfer.Region.Height, .depth = 1}};
}

}

Result<void> Recording::Copy(const Buffer &source, Texture &destination, TextureTransfer region) {
  if (auto owner = CheckOwner(*Context_); !owner) { return owner; }
  if (Render_) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "transfer inside render pass"});
  }
  const auto from = BufferFrom(source, *Context_);
  const auto into = ImageFrom(destination, *Context_);
  if (!from) { return std::unexpected(from.error()); }
  if (!into) { return std::unexpected(into.error()); }
  if (!Has((*from)->Description.Use, BufferUse::CopySource) ||
      !Has((*into)->Description.Use, TextureUse::CopyDestination)) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "missing texture upload usage"});
  }
  const auto range = TransferRange(**into, **from, region);
  if (!range) { return std::unexpected(range.error()); }
  if (auto held = Hold(*from); !held) { return held; }
  if (auto used = UseImage(*into,
                           View(region.Region),
                           {.Stage = VK_PIPELINE_STAGE_2_COPY_BIT,
                            .Access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                            .Layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL});
      !used) {
    return used;
  }
  Use(**from,
      *range,
      {.Stage = VK_PIPELINE_STAGE_2_COPY_BIT, .Access = VK_ACCESS_2_TRANSFER_READ_BIT});
  const auto copy = CopyRegion(**into, region);
  vkCmdCopyBufferToImage(
      Commands_, (*from)->Handle, (*into)->Handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
  return {};
}

Result<void> Recording::Copy(const Texture &source, Buffer &destination, TextureTransfer region) {
  if (auto owner = CheckOwner(*Context_); !owner) { return owner; }
  if (Render_) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "transfer inside render pass"});
  }
  const auto from = ImageFrom(source, *Context_);
  const auto into = BufferFrom(destination, *Context_);
  if (!from) { return std::unexpected(from.error()); }
  if (!into) { return std::unexpected(into.error()); }
  if (!Has((*from)->Description.Use, TextureUse::CopySource) ||
      !Has((*into)->Description.Use, BufferUse::CopyDestination)) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "missing texture readback usage"});
  }
  const auto range = TransferRange(**from, **into, region);
  if (!range) { return std::unexpected(range.error()); }
  if (auto held = Hold(*into); !held) { return held; }
  if (auto used = UseImage(*from,
                           View(region.Region),
                           {.Stage = VK_PIPELINE_STAGE_2_COPY_BIT,
                            .Access = VK_ACCESS_2_TRANSFER_READ_BIT,
                            .Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL});
      !used) {
    return used;
  }
  Use(**into,
      *range,
      {.Stage = VK_PIPELINE_STAGE_2_COPY_BIT, .Access = VK_ACCESS_2_TRANSFER_WRITE_BIT});
  const auto copy = CopyRegion(**from, region);
  vkCmdCopyImageToBuffer(
      Commands_, (*from)->Handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, (*into)->Handle, 1, &copy);
  if ((*into)->Description.Placement == Memory::Readback) {
    Use(**into,
        *range,
        {.Stage = VK_PIPELINE_STAGE_2_HOST_BIT, .Access = VK_ACCESS_2_HOST_READ_BIT});
  }
  return {};
}

Result<void> Recording::Copy(const Texture &source,
                             TextureRegion sourceRegion,
                             Texture &destination,
                             TextureRegion destinationRegion) {
  if (auto owner = CheckOwner(*Context_); !owner) { return owner; }
  if (Render_) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "transfer inside render pass"});
  }
  const auto from = ImageFrom(source, *Context_);
  const auto into = ImageFrom(destination, *Context_);
  if (!from) { return std::unexpected(from.error()); }
  if (!into) { return std::unexpected(into.error()); }
  if (!Has((*from)->Description.Use, TextureUse::CopySource) ||
      !Has((*into)->Description.Use, TextureUse::CopyDestination) ||
      (*from)->Description.Texels != (*into)->Description.Texels ||
      !ValidRegion(**from, sourceRegion) || !ValidRegion(**into, destinationRegion) ||
      sourceRegion.Width != destinationRegion.Width ||
      sourceRegion.Height != destinationRegion.Height) {
    return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                 .Message = "invalid texture copy regions, formats or usage"});
  }
  const bool same = *from == *into && sourceRegion.Mip == destinationRegion.Mip &&
                    sourceRegion.Layer == destinationRegion.Layer;
  if (same && sourceRegion.X < destinationRegion.X + destinationRegion.Width &&
      destinationRegion.X < sourceRegion.X + sourceRegion.Width &&
      sourceRegion.Y < destinationRegion.Y + destinationRegion.Height &&
      destinationRegion.Y < sourceRegion.Y + sourceRegion.Height) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "overlapping texture self-copy"});
  }
  const auto sourceLayout = same ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  const auto destinationLayout =
      same ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  if (auto used =
          UseImage(*from,
                   View(sourceRegion),
                   {.Stage = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .Access = same ? VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT
                                   : VK_ACCESS_2_TRANSFER_READ_BIT,
                    .Layout = sourceLayout});
      !used) {
    return used;
  }
  if (!same) {
    if (auto used = UseImage(*into,
                             View(destinationRegion),
                             {.Stage = VK_PIPELINE_STAGE_2_COPY_BIT,
                              .Access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                              .Layout = destinationLayout});
        !used) {
      return used;
    }
  }
  const VkImageCopy copy{
      .srcSubresource = Layers(**from, sourceRegion),
      .srcOffset = {.x = static_cast<int32_t>(sourceRegion.X),
                    .y = static_cast<int32_t>(sourceRegion.Y),
                    .z = 0},
      .dstSubresource = Layers(**into, destinationRegion),
      .dstOffset = {.x = static_cast<int32_t>(destinationRegion.X),
                    .y = static_cast<int32_t>(destinationRegion.Y),
                    .z = 0},
      .extent = {.width = sourceRegion.Width, .height = sourceRegion.Height, .depth = 1}};
  vkCmdCopyImage(
      Commands_, (*from)->Handle, sourceLayout, (*into)->Handle, destinationLayout, 1, &copy);
  return {};
}

}
#endif
