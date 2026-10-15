#include "MetalInternal.h"
#include <algorithm>
#include <cmath>

namespace outshine::Render::Native::Metal {
namespace {

Result<AttachmentState> Attachment(TextureView view, TextureUse use, uint64_t device) {
  const auto *const texture = dynamic_cast<const MetalTexture *>(view.Image);
  if (texture == nullptr || texture->Device != device || view.Mips != 1 || view.Layers != 1 ||
      !Has(texture->Description().Use, use) ||
      !Inside({.Mip = view.FirstMip, .Layer = view.FirstLayer, .Width = 1, .Height = 1},
              texture->Description())) {
    return Failure(ErrorKind::Invalid, "invalid Metal render attachment or subresource");
  }
  const auto &description = texture->Description();
  return AttachmentState{.Native = texture->Native,
                         .Recording = texture->Recording,
                         .Texels = description.Texels,
                         .Mip = view.FirstMip,
                         .Layer = view.FirstLayer,
                         .Width = std::max(1u, description.Width >> view.FirstMip),
                         .Height = std::max(1u, description.Height >> view.FirstMip)};
}

Result<void> Configure(MTLRenderPassAttachmentDescriptor *target,
                       const AttachmentState &attachment,
                       Load begin,
                       Store end) {
  switch (begin) {
    case Load::Load: target.loadAction = MTLLoadActionLoad; break;
    case Load::Clear: target.loadAction = MTLLoadActionClear; break;
    case Load::Discard: target.loadAction = MTLLoadActionDontCare; break;
    default: return Failure(ErrorKind::Invalid, "unknown Metal attachment load action");
  }
  switch (end) {
    case Store::Store: target.storeAction = MTLStoreActionStore; break;
    case Store::Discard: target.storeAction = MTLStoreActionDontCare; break;
    default: return Failure(ErrorKind::Invalid, "unknown Metal attachment store action");
  }
  target.texture = attachment.Native;
  target.level = attachment.Mip;
  target.slice = attachment.Layer;
  return {};
}

bool Compatible(const RenderState &pass, const AttachmentState &attachment) {
  if (pass.Width != 0 && (pass.Width != attachment.Width || pass.Height != attachment.Height)) {
    return false;
  }
  return std::ranges::none_of(pass.Colours, [&](const AttachmentState &existing) {
    return existing.Native == attachment.Native && existing.Mip == attachment.Mip &&
           existing.Layer == attachment.Layer;
  });
}

}

Result<void> MetalCommands::BeginRenderPass(const RenderPassDescription &description) {
  if (!OnOwner() || Render) {
    return Failure(ErrorKind::Invalid, "Metal render pass requires its owner and no open pass");
  }
  if (description.Colours.size() > 8 || (description.Colours.empty() && !description.Depth)) {
    return Failure(ErrorKind::Invalid, "Metal render pass requires one to eight valid targets");
  }
  @autoreleasepool {
    MTLRenderPassDescriptor *const descriptor = [MTLRenderPassDescriptor new];
    RenderState state{};
    state.Colours.reserve(description.Colours.size());
    for (size_t slot = 0; slot < description.Colours.size(); ++slot) {
      const auto &colour = description.Colours[slot];
      const auto attachment = Attachment(colour.Image, TextureUse::ColourAttachment, Device);
      if (!attachment) { return std::unexpected(attachment.error()); }
      if (!Compatible(state, *attachment)) {
        return Failure(ErrorKind::Invalid, "Metal render targets overlap or differ in extent");
      }
      const auto configured =
          Configure(descriptor.colorAttachments[slot], *attachment, colour.Begin, colour.End);
      if (!configured) { return configured; }
      const auto clear = colour.Clear;
      descriptor.colorAttachments[slot].clearColor =
          MTLClearColorMake(clear[0], clear[1], clear[2], clear[3]);
      state.Width = attachment->Width;
      state.Height = attachment->Height;
      state.Colours.push_back(*attachment);
    }
    if (description.Depth) {
      const auto &depth = *description.Depth;
      const auto attachment = Attachment(depth.Image, TextureUse::DepthAttachment, Device);
      if (!attachment) { return std::unexpected(attachment.error()); }
      if (!Compatible(state, *attachment) || !std::isfinite(depth.Clear) || depth.Clear < 0 ||
          depth.Clear > 1) {
        return Failure(ErrorKind::Invalid, "invalid Metal depth extent or clear value");
      }
      const auto configured =
          Configure(descriptor.depthAttachment, *attachment, depth.Begin, depth.End);
      if (!configured) { return configured; }
      descriptor.depthAttachment.clearDepth = depth.Clear;
      state.Width = attachment->Width;
      state.Height = attachment->Height;
      state.Depth = *attachment;
    }
    std::vector<std::shared_ptr<TextureRecording>> images;
    images.reserve(state.Colours.size() + (state.Depth ? 1 : 0));
    for (const auto &colour : state.Colours) { images.push_back(colour.Recording); }
    if (state.Depth) { images.push_back(state.Depth->Recording); }
    const auto held = HoldImages(images);
    if (!held) { return held; }
    Finish();
    state.Encoder = [Native renderCommandEncoderWithDescriptor:descriptor];
    if (state.Encoder == nil) {
      return Failure(ErrorKind::Driver, "Metal render encoder creation failed");
    }
    Render = std::move(state);
    return {};
  }
}

Result<void> MetalCommands::EndRenderPass() {
  if (!OnOwner() || !Render) {
    return Failure(ErrorKind::Invalid, "Metal render pass end requires its owner and an open pass");
  }
  [Render->Encoder endEncoding];
  Render.reset();
  Accesses.clear();
  return {};
}

}
