#ifdef __linux__
#include "Recording.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>

namespace outshine::Render::Native::Vulkan {

void Recording::ContinueRenderPass(RenderState &pass) {
  std::array<VkImageMemoryBarrier2, 9> barriers{};
  uint32_t count = 0;
  const auto preserve = [&](RenderAttachment &attachment, bool depth) {
    const auto access = AttachmentAccess(depth);
    auto &barrier = barriers[count++];
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = access.Stage;
    barrier.srcAccessMask = access.Access;
    barrier.dstStageMask = access.Stage;
    barrier.dstAccessMask = access.Access;
    barrier.oldLayout = attachment.Native.imageLayout;
    barrier.newLayout = attachment.Native.imageLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = attachment.Image->Handle;
    barrier.subresourceRange = {.aspectMask = Aspect(attachment.Image->Description.Texels),
                                .baseMipLevel = attachment.Range.FirstMip,
                                .levelCount = attachment.Range.Mips,
                                .baseArrayLayer = attachment.Range.FirstLayer,
                                .layerCount = attachment.Range.Layers};
    attachment.Native.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  };
  for (auto &colour : pass.Colours) { preserve(colour, false); }
  if (pass.Depth) { preserve(*pass.Depth, true); }
  VkDependencyInfo dependency{};
  dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependency.imageMemoryBarrierCount = count;
  dependency.pImageMemoryBarriers = barriers.data();
  vkCmdPipelineBarrier2(Commands_, &dependency);
}

void Recording::EncodeRenderPass(RenderState &pass, bool final) {
  std::array<VkRenderingAttachmentInfo, 8> colours{};
  for (std::size_t at = 0; at < pass.Colours.size(); ++at) {
    colours[at] = pass.Colours[at].Native;
    if (!final) { colours[at].storeOp = VK_ATTACHMENT_STORE_OP_STORE; }
  }
  VkRenderingAttachmentInfo depth{};
  if (pass.Depth) {
    depth = pass.Depth->Native;
    if (!final) { depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE; }
  }
  VkRenderingInfo rendering{};
  rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  rendering.renderArea.extent = {.width = pass.Width, .height = pass.Height};
  rendering.layerCount = 1;
  rendering.colorAttachmentCount = static_cast<uint32_t>(pass.Colours.size());
  rendering.pColorAttachments = colours.data();
  rendering.pDepthAttachment = pass.Depth ? &depth : nullptr;
  vkCmdBeginRendering(Commands_, &rendering);
  VkPipeline bound = VK_NULL_HANDLE;
  for (const auto &draw : pass.Draws) {
    const auto &pipeline = draw.Graphics->Pipeline->Handle;
    if (bound != pipeline) {
      vkCmdBindPipeline(Commands_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
      bound = pipeline;
    }
    EncodeDraw(draw);
  }
  vkCmdEndRendering(Commands_);
  pass.Draws.clear();
  if (final) {
    Render_.reset();
  } else {
    ContinueRenderPass(pass);
  }
}

Result<void> Recording::EndRenderPass() {
  if (auto owner = CheckOwner(*Context_); !owner) { return owner; }
  if (Finished_ || !Render_) {
    return std::unexpected(Error{.Kind = ErrorKind::Invalid, .Message = "no open render pass"});
  }
  EncodeRenderPass(*Render_, true);
  return {};
}

}
#endif
