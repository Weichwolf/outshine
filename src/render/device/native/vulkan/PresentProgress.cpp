#ifdef __linux__
#include "Presenter.h"
#include <atomic>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>

namespace outshine::Render::Native::Vulkan {
namespace {

Result<bool> Fence(const Context &context, VkFence fence, bool stopping) {
  const auto result =
      stopping ? vkWaitForFences(
                     context.Logical, 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max())
               : vkGetFenceStatus(context.Logical, fence);
  if (result == VK_NOT_READY) { return false; }
  if (result != VK_SUCCESS) {
    return std::unexpected(Failure("Vulkan presentation fence", result));
  }
  return true;
}

Result<void> Release(PresentLease &lease) {
  VkReleaseSwapchainImagesInfoEXT release{};
  release.sType = VK_STRUCTURE_TYPE_RELEASE_SWAPCHAIN_IMAGES_INFO_EXT;
  release.swapchain = lease.Target->Handle;
  release.imageIndexCount = 1;
  release.pImageIndices = &lease.Index;
  const auto result = lease.Target->Owner->ReleaseImages(lease.Target->Owner->Logical, &release);
  if (result != VK_SUCCESS) {
    return std::unexpected(Failure("vkReleaseSwapchainImagesEXT", result));
  }
  lease.Phase.store(PresentPhase::Released);
  return {};
}

Result<void> Queue(PresentLease &lease) {
  auto &target = *lease.Target;
  target.Layouts[lease.Index] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  VkSwapchainPresentFenceInfoEXT fence{};
  fence.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT;
  fence.swapchainCount = 1;
  fence.pFences = &lease.Presented;
  VkPresentInfoKHR present{};
  present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present.pNext = &fence;
  present.waitSemaphoreCount = 1;
  present.pWaitSemaphores = &lease.Rendered;
  present.swapchainCount = 1;
  present.pSwapchains = &target.Handle;
  present.pImageIndices = &lease.Index;
  const auto result = vkQueuePresentKHR(target.Owner->PresentQueue, &present);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    if (auto state = lease.Dispatcher.lock()) { InvalidatePresentation(*state, lease.Generation); }
  }
  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR &&
      result != VK_ERROR_SURFACE_LOST_KHR) {
    lease.Phase.store(PresentPhase::Cancelled);
    return std::unexpected(Failure("vkQueuePresentKHR", result));
  }
  lease.Phase.store(PresentPhase::Queued);
  if (result == VK_ERROR_SURFACE_LOST_KHR) {
    return std::unexpected(Failure("vkQueuePresentKHR", result));
  }
  return {};
}

}

Result<bool> AdvanceLease(const std::shared_ptr<PresentLease> &lease, bool stopping) {
  auto phase = lease->Phase.load();
  if (phase == PresentPhase::Acquiring) {
    const auto ready = Fence(*lease->Target->Owner, lease->Available, stopping);
    if (!ready) { return std::unexpected(ready.error()); }
    if (!*ready) { return false; }
    lease->Phase.store(stopping ? PresentPhase::Cancelled : PresentPhase::Ready);
    phase = lease->Phase.load();
  }
  if (stopping && (phase == PresentPhase::Ready || phase == PresentPhase::Acquired ||
                   phase == PresentPhase::Recorded || phase == PresentPhase::Submitted)) {
    lease->Phase.store(PresentPhase::Cancelled);
    phase = PresentPhase::Cancelled;
  }
  if (phase == PresentPhase::Cancelled && (stopping || lease->Recordings.load() == 0)) {
    if (auto released = Release(*lease); !released) { return std::unexpected(released.error()); }
    return true;
  }
  if (phase == PresentPhase::Submitted) {
    if (auto queued = Queue(*lease); !queued) { return std::unexpected(queued.error()); }
    phase = PresentPhase::Queued;
  }
  if (phase == PresentPhase::Queued) {
    const auto ready = Fence(*lease->Target->Owner, lease->Presented, stopping);
    if (!ready) { return std::unexpected(ready.error()); }
    if (*ready && (stopping || lease->Recordings.load() == 0)) {
      lease->Phase.store(PresentPhase::Released);
      return true;
    }
  }
  return phase == PresentPhase::Released;
}

}
#endif
