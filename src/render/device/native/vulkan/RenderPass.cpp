#ifdef __linux__
#include "Recording.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <expected>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace outshine::Render::Native::Vulkan {

bool Overlaps(const TextureView &left, const TextureView &right) noexcept {
  return uint64_t{left.FirstMip} < uint64_t{right.FirstMip} + right.Mips &&
         uint64_t{right.FirstMip} < uint64_t{left.FirstMip} + left.Mips &&
         uint64_t{left.FirstLayer} < uint64_t{right.FirstLayer} + right.Layers &&
         uint64_t{right.FirstLayer} < uint64_t{left.FirstLayer} + left.Layers;
}

namespace {

bool Compatible(const RenderState &pass, const RenderAttachment &attachment) {
  const auto width = std::max(1u, attachment.Image->Description.Width >> attachment.Range.FirstMip);
  const auto height =
      std::max(1u, attachment.Image->Description.Height >> attachment.Range.FirstMip);
  if (pass.Width != 0 && (pass.Width != width || pass.Height != height)) { return false; }
  const auto aliases = [&](const RenderAttachment &other) {
    return other.Image == attachment.Image && Overlaps(other.Range, attachment.Range);
  };
  return !std::ranges::any_of(pass.Colours, aliases) && (!pass.Depth || !aliases(*pass.Depth));
}

}

Recording::ImageAccess Recording::AttachmentAccess(bool depth) noexcept {
  return depth ? ImageAccess{.Stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                                      VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                             .Access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                       VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                             .Layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL}
               : ImageAccess{.Stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                             .Access = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
                                       VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                             .Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
}

Result<RenderAttachment>
Recording::PrepareAttachment(TextureView view, Load begin, Store end, bool depth) {
  if (view.Image == nullptr || view.Mips != 1 || view.Layers != 1 ||
      (begin != Load::Load && begin != Load::Clear && begin != Load::Discard) ||
      (end != Store::Store && end != Store::Discard)) {
    return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                 .Message = "invalid attachment view or load/store action"});
  }
  auto image = ImageFrom(*view.Image, *Context_);
  if (!image) { return std::unexpected(image.error()); }
  const auto use = depth ? TextureUse::DepthAttachment : TextureUse::ColourAttachment;
  if (!ValidView(**image, view) ||
      ((*image)->Description.Texels == Format::Depth32Float) != depth ||
      (static_cast<uint32_t>((*image)->Description.Use) & static_cast<uint32_t>(use)) == 0) {
    return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                 .Message = "invalid attachment format, usage or subresource"});
  }
  const auto access = AttachmentAccess(depth);
  if (auto used = UseImage(*image, view, access); !used) { return std::unexpected(used.error()); }
  auto native = Storage_.ImageView(**image, view);
  if (!native) { return std::unexpected(native.error()); }
  RenderAttachment attachment{.Image = std::move(*image), .Range = view};
  attachment.Range.Image = nullptr;
  attachment.Native.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  attachment.Native.imageView = *native;
  attachment.Native.imageLayout = access.Layout;
  attachment.Native.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  if (begin == Load::Clear) { attachment.Native.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; }
  if (begin == Load::Load) { attachment.Native.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; }
  attachment.Native.storeOp =
      end == Store::Store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
  return attachment;
}

Result<void> Recording::BeginRenderPass(const RenderPassDescription &description) {
  if (auto owner = CheckOwner(*Context_); !owner) { return owner; }
  if (Finished_ || Render_ ||
      description.Colours.size() > std::min(8u, Context_->Limits.MaxColourAttachments) ||
      (description.Colours.empty() && !description.Depth)) {
    return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                 .Message = "invalid render pass targets or command state"});
  }
  RenderState pass;
  for (const auto &colour : description.Colours) {
    auto attachment = PrepareAttachment(colour.Image, colour.Begin, colour.End, false);
    if (!attachment) { return std::unexpected(attachment.error()); }
    if (!Compatible(pass, *attachment)) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Invalid, .Message = "attachments overlap or differ in extent"});
    }
    std::ranges::copy(colour.Clear, attachment->Native.clearValue.color.float32);
    pass.Width = std::max(1u, attachment->Image->Description.Width >> colour.Image.FirstMip);
    pass.Height = std::max(1u, attachment->Image->Description.Height >> colour.Image.FirstMip);
    pass.Colours.push_back(std::move(*attachment));
  }
  if (description.Depth) {
    const auto &depth = *description.Depth;
    if (!std::isfinite(depth.Clear) || depth.Clear < 0 || depth.Clear > 1) {
      return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                   .Message = "depth clear must be finite in zero to one"});
    }
    auto attachment = PrepareAttachment(depth.Image, depth.Begin, depth.End, true);
    if (!attachment) { return std::unexpected(attachment.error()); }
    if (!Compatible(pass, *attachment)) {
      return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                   .Message = "depth target overlaps or differs in extent"});
    }
    attachment->Native.clearValue.depthStencil.depth = depth.Clear;
    pass.Width = std::max(1u, attachment->Image->Description.Width >> depth.Image.FirstMip);
    pass.Height = std::max(1u, attachment->Image->Description.Height >> depth.Image.FirstMip);
    pass.Depth = std::move(*attachment);
  }
  Render_ = std::move(pass);
  return {};
}

}
#endif
