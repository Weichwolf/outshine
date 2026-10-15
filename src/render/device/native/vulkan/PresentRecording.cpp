#ifdef __linux__
#include "Recording.h"
#include <atomic>
#include <expected>
#include <memory>
#include <utility>

namespace outshine::Render::Native::Vulkan {

Result<void> Recording::Present(std::unique_ptr<PresentFrame> frame) {
  if (auto owner = CheckOwner(*Context_); !owner) { return owner; }
  if (Context_->Surface == VK_NULL_HANDLE && !frame) {
    return std::unexpected(Error{.Kind = ErrorKind::Unsupported,
                                 .Message = "headless Vulkan recording cannot present"});
  }
  auto *const native = dynamic_cast<VulkanPresentFrame *>(frame.get());
  if (native == nullptr || Render_ || Finished_ || Frame_ ||
      native->Lease()->Target->Owner != Context_ || !native->Lease()->Current()) {
    return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                 .Message = "present needs one owned lease and a closed pass"});
  }
  const auto image = ImageFrom(native->Image(), *Context_);
  if (!image) { return std::unexpected(image.error()); }
  if (auto held = TrackImage(*image, {.Image = &native->Image()}); !held) {
    return std::unexpected(held.error());
  }
  auto before = PresentPhase::Acquired;
  if (!native->Lease()->Phase.compare_exchange_strong(before, PresentPhase::Recorded)) {
    return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                 .Message = "present lease was already transferred or cancelled"});
  }
  Presented_ = native->Lease();
  Frame_ = std::move(frame);
  return {};
}

Result<void> Recording::FinalizePresentation() {
  for (const auto &tracked : Images_) {
    if (tracked.Image->Presentation && tracked.Image->Presentation != Presented_) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Invalid,
                .Message = "present image was used without transferring its frame lease"});
    }
  }
  if (!Presented_) { return {}; }
  if (!Presented_->Current() || Presented_->Phase.load() != PresentPhase::Recorded) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "recorded presentation lease is not live"});
  }
  const auto image = ImageFrom(Frame_->Image(), *Context_);
  if (!image) { return std::unexpected(image.error()); }
  return UseImage(*image,
                  {.Image = &Frame_->Image()},
                  {.Stage = VK_PIPELINE_STAGE_2_NONE,
                   .Access = VK_ACCESS_2_NONE,
                   .Layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR});
}

void Recording::BindPresentation(VkSubmitInfo2 &submit,
                                 VkSemaphoreSubmitInfo &wait,
                                 VkSemaphoreSubmitInfo &signal) const noexcept {
  if (!Presented_) { return; }
  wait.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  wait.semaphore = Presented_->Acquired;
  wait.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  signal.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  signal.semaphore = Presented_->Rendered;
  signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  submit.waitSemaphoreInfoCount = 1;
  submit.pWaitSemaphoreInfos = &wait;
  submit.signalSemaphoreInfoCount = 1;
  submit.pSignalSemaphoreInfos = &signal;
}

void Recording::CommitPresent() noexcept {
  if (!Presented_) { return; }
  Presented_->Phase.store(PresentPhase::Submitted);
  Presented_->Wake();
}

}
#endif
