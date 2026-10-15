#ifdef __linux__
#include "Presenter.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <expected>
#include <memory>
#include <limits>
#include <mutex>
#include <utility>

namespace outshine::Render::Native::Vulkan {
namespace {

bool Matches(const std::shared_ptr<SwapchainState> &target,
             const std::shared_ptr<SurfaceState> &surface) {
  return target && surface && target->Surface->WindowId == surface->WindowId;
}

Result<std::shared_ptr<SwapchainState>> Prepare(const std::shared_ptr<PresentState> &state,
                                                TargetRequest &request) {
  if (!request.Surface) { return std::shared_ptr<SwapchainState>{}; }
  std::erase_if(state->Inactive, [](const std::shared_ptr<SwapchainState> &target) {
    return target.use_count() == 1;
  });
  if (state->Active && state->Active->DriverRetired && state->Active.use_count() == 1) {
    state->Active.reset();
  }
  std::shared_ptr<SwapchainState> before;
  bool retired = false;
  const auto inspect = [&](const std::shared_ptr<SwapchainState> &target) {
    if (!Matches(target, request.Surface)) { return; }
    if (target->DriverRetired) {
      retired = true;
    } else {
      before = target;
    }
  };
  inspect(state->Active);
  for (const auto &target : state->Inactive) { inspect(target); }
  if (!before && retired) {
    return std::unexpected(Error{.Kind = ErrorKind::Busy,
                                 .Message = "the previous target generation is still retiring"});
  }
  if (before) {
    if (before == state->Active && state->Failed) { return std::unexpected(*state->Failed); }
    VkSurfaceCapabilitiesKHR caps{};
    const auto supported = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        state->Owner->Physical, before->Surface->Handle, &caps);
    if (supported != VK_SUCCESS) {
      return std::unexpected(Failure("vkGetPhysicalDeviceSurfaceCapabilitiesKHR", supported));
    }
    request.Surface = before->Surface;
    const bool currentExtent = caps.currentExtent.width == std::numeric_limits<uint32_t>::max() ||
                               (caps.currentExtent.width == before->Extent.width &&
                                caps.currentExtent.height == before->Extent.height);
    if (currentExtent && before->Extent.width == request.Extent.width &&
        before->Extent.height == request.Extent.height) {
      return before;
    }
  }
  auto prepared =
      CreateSwapchain(request.Surface, request.Extent, state->Generation.load() + 1, before);
  if (!prepared && prepared.error().Kind == ErrorKind::Busy &&
      (request.Extent.width == 0 || request.Extent.height == 0)) {
    return std::shared_ptr<SwapchainState>{};
  }
  return prepared;
}

}

void SwitchTarget(const std::shared_ptr<PresentState> &state) {
  std::shared_ptr<TargetRequest> request;
  {
    const std::scoped_lock lock(state->Gate);
    request = std::move(state->RequestedTarget);
    if (request && state->Failed && state->Failed->Kind == ErrorKind::DeviceLost) {
      request->Failure = state->Failed;
    }
  }
  if (!request) { return; }
  auto prepared = request->Failure
                      ? Result<std::shared_ptr<SwapchainState>>(std::unexpected(*request->Failure))
                      : Prepare(state, *request);
  if (!prepared) {
    request->Failure = prepared.error();
  } else {
    if (state->Active && state->Active != *prepared) {
      state->Inactive.push_back(std::move(state->Active));
    }
    std::erase(state->Inactive, *prepared);
    state->Attempt.reset();
    state->Active = std::move(*prepared);
    const std::scoped_lock lock(state->Gate);
    state->Surface = request->Surface;
    state->Width = request->Extent.width;
    state->Height = request->Extent.height;
    ++state->Generation;
    if (state->Active) { state->Active->Generation = state->Generation.load(); }
    state->Failed.reset();
    state->Wake();
  }
  request->Done.store(true);
  request->Done.notify_one();
}

}
#endif
