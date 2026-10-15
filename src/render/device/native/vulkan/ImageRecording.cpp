#ifdef __linux__
#include "Recording.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <utility>
#include <vector>

namespace outshine::Render::Native::Vulkan {

void Recording::CommitImages() noexcept {
  for (auto &tracked : Images_) {
    assert(tracked.Image->Reserved == this);
    tracked.Image->Layouts = std::move(tracked.Layouts);
    tracked.Image->Reserved = nullptr;
  }
}

Result<Recording::TrackedImage *> Recording::TrackImage(const std::shared_ptr<ImageState> &image,
                                                        TextureView view) {
  if (!ValidView(*image, view) || Finished_) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "invalid image view or recording state"});
  }
  if (image->Reserved != nullptr && image->Reserved != this) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Busy, .Message = "texture belongs to another open recording"});
  }
  if (image->Presentation) {
    const auto phase = image->Presentation->Phase.load();
    if (!image->Presentation->Current() ||
        (phase != PresentPhase::Acquired && phase != PresentPhase::Recorded)) {
      return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                   .Message = "present image lease is no longer recordable"});
    }
  }
  auto found = std::ranges::find_if(
      Images_, [&image](const TrackedImage &tracked) { return tracked.Image == image; });
  if (found == Images_.end()) {
    Images_.push_back({.Image = image, .Layouts = image->Layouts});
    image->Reserved = this;
    if (image->Presentation) { image->Presentation->Recordings.fetch_add(1); }
    found = Images_.end() - 1;
  }
  return &*found;
}

Result<void> Recording::UseImage(const std::shared_ptr<ImageState> &image,
                                 TextureView view,
                                 ImageAccess access) {
  const auto tracked = TrackImage(image, view);
  if (!tracked) { return std::unexpected(tracked.error()); }
  auto *const found = *tracked;
  std::vector<VkImageMemoryBarrier2> barriers;
  for (uint32_t mip = view.FirstMip; mip < view.FirstMip + view.Mips; ++mip) {
    uint32_t layer = view.FirstLayer;
    while (layer < view.FirstLayer + view.Layers) {
      const auto start = static_cast<std::size_t>(mip) * image->Description.Layers + layer;
      const auto before = found->Layouts[start];
      uint32_t count = 1;
      while (layer + count < view.FirstLayer + view.Layers &&
             found->Layouts[start + count] == before) {
        ++count;
      }
      VkImageMemoryBarrier2 barrier{};
      barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
      barrier.srcStageMask = before == VK_IMAGE_LAYOUT_UNDEFINED
                                 ? VK_PIPELINE_STAGE_2_NONE
                                 : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
      barrier.srcAccessMask = before == VK_IMAGE_LAYOUT_UNDEFINED
                                  ? 0
                                  : VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
      barrier.dstStageMask = access.Stage;
      barrier.dstAccessMask = access.Access;
      barrier.oldLayout = before;
      barrier.newLayout = access.Layout;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.image = image->Handle;
      barrier.subresourceRange = {.aspectMask = Aspect(image->Description.Texels),
                                  .baseMipLevel = mip,
                                  .levelCount = 1,
                                  .baseArrayLayer = layer,
                                  .layerCount = count};
      if (!barriers.empty() && barriers.back().oldLayout == before &&
          barriers.back().subresourceRange.baseArrayLayer == layer &&
          barriers.back().subresourceRange.layerCount == count &&
          barriers.back().subresourceRange.baseMipLevel +
                  barriers.back().subresourceRange.levelCount ==
              mip) {
        ++barriers.back().subresourceRange.levelCount;
      } else {
        barriers.push_back(barrier);
      }
      std::fill_n(
          found->Layouts.begin() + static_cast<std::ptrdiff_t>(start), count, access.Layout);
      layer += count;
    }
  }
  VkDependencyInfo dependency{};
  dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependency.imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
  dependency.pImageMemoryBarriers = barriers.data();
  vkCmdPipelineBarrier2(Commands_, &dependency);
  return {};
}

}
#endif
