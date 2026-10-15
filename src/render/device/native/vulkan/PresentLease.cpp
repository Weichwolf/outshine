#ifdef __linux__
#include "Presenter.h"
#include <atomic>
#include <expected>
#include <memory>
#include <utility>

namespace outshine::Render::Native::Vulkan {

PresentLease::~PresentLease() {
  auto *const device = Target->Owner->Logical;
  if (Presented != VK_NULL_HANDLE) { vkDestroyFence(device, Presented, nullptr); }
  if (Available != VK_NULL_HANDLE) { vkDestroyFence(device, Available, nullptr); }
  if (Rendered != VK_NULL_HANDLE) { vkDestroySemaphore(device, Rendered, nullptr); }
  if (Acquired != VK_NULL_HANDLE) { vkDestroySemaphore(device, Acquired, nullptr); }
}

bool PresentLease::Current() const noexcept {
  const auto state = Dispatcher.lock();
  return state && !state->Stop.load() && state->Generation.load() == Generation;
}

void PresentLease::Wake() const noexcept {
  if (auto state = Dispatcher.lock()) { state->Wake(); }
}

VulkanPresentFrame::VulkanPresentFrame(std::shared_ptr<PresentLease> lease)
    : Lease_(std::move(lease)) {
  auto image = std::make_shared<ImageState>();
  image->Owner = Lease_->Target->Owner;
  image->Handle = Lease_->Target->Images[Lease_->Index];
  image->Description = {.Width = Lease_->Target->Extent.width,
                        .Height = Lease_->Target->Extent.height,
                        .Texels = Lease_->Target->Texels,
                        .Use = Lease_->Target->Use};
  image->Layouts = {Lease_->InitialLayout};
  image->Presentation = Lease_;
  Image_ = std::make_unique<VulkanTexture>(std::move(image));
}

VulkanPresentFrame::~VulkanPresentFrame() {
  auto before = PresentPhase::Acquired;
  if (Lease_->Phase.compare_exchange_strong(before, PresentPhase::Cancelled)) { Lease_->Wake(); }
  before = PresentPhase::Recorded;
  if (Lease_->Phase.compare_exchange_strong(before, PresentPhase::Cancelled)) { Lease_->Wake(); }
}

namespace {

Result<void> InitializeLease(PresentLease &lease) {
  auto *const device = lease.Target->Owner->Logical;
  VkSemaphoreCreateInfo semaphore{};
  semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  auto result = vkCreateSemaphore(device, &semaphore, nullptr, &lease.Acquired);
  if (result != VK_SUCCESS) {
    return std::unexpected(Failure("vkCreateSemaphore(acquire)", result));
  }
  result = vkCreateSemaphore(device, &semaphore, nullptr, &lease.Rendered);
  if (result != VK_SUCCESS) {
    return std::unexpected(Failure("vkCreateSemaphore(render)", result));
  }
  VkFenceCreateInfo fence{};
  fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  result = vkCreateFence(device, &fence, nullptr, &lease.Available);
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkCreateFence(acquire)", result)); }
  result = vkCreateFence(device, &fence, nullptr, &lease.Presented);
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkCreateFence(present)", result)); }
  return {};
}

}

Result<std::shared_ptr<PresentLease>> AcquireLease(const std::shared_ptr<PresentState> &state,
                                                   const std::shared_ptr<SwapchainState> &target) {
  if (!state->Attempt || state->Attempt->Target != target) {
    auto candidate = std::make_shared<PresentLease>();
    candidate->Target = target;
    candidate->Dispatcher = state;
    if (auto initialized = InitializeLease(*candidate); !initialized) {
      return std::unexpected(initialized.error());
    }
    state->Attempt = std::move(candidate);
  }
  auto lease = state->Attempt;
  lease->Generation = target->Generation;
  const auto result = vkAcquireNextImageKHR(
      target->Owner->Logical, target->Handle, 0, lease->Acquired, lease->Available, &lease->Index);
  if (result == VK_NOT_READY || result == VK_TIMEOUT) { return std::shared_ptr<PresentLease>{}; }
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    InvalidatePresentation(*state, lease->Generation);
    return std::shared_ptr<PresentLease>{};
  }
  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    return std::unexpected(Failure("vkAcquireNextImageKHR", result));
  }
  state->Attempt.reset();
  lease->InitialLayout = target->Layouts[lease->Index];
  if (result == VK_SUBOPTIMAL_KHR) { InvalidatePresentation(*state, lease->Generation); }
  return lease;
}

}
#endif
