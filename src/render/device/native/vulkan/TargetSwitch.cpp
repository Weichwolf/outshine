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

Error RetiringTarget() {
  return {.Kind = ErrorKind::Busy, .Message = "previous presentation targets are still retiring"};
}

struct ExistingTargets {
  std::shared_ptr<SwapchainState> Usable;
  bool Retired = false;
};

ExistingTargets FindTargets(const PresentState &state,
                            const std::shared_ptr<SurfaceState> &surface) {
  ExistingTargets result;
  const auto inspect = [&](const std::shared_ptr<SwapchainState> &target) {
    if (!Matches(target, surface)) { return; }
    if (target->DriverRetired) {
      result.Retired = true;
    } else {
      result.Usable = target;
    }
  };
  inspect(state.Active);
  for (const auto &target : state.Inactive) { inspect(target); }
  return result;
}

Result<bool> ExtentMatches(const SwapchainState &target, VkExtent2D extent) {
  const auto supported = ReadSurfaceCapabilities(*target.Surface);
  if (!supported) { return std::unexpected(supported.error()); }
  const auto &caps = *supported;
  const bool current = caps.currentExtent.width == std::numeric_limits<uint32_t>::max() ||
                       (caps.currentExtent.width == target.Extent.width &&
                        caps.currentExtent.height == target.Extent.height);
  return current && target.Extent.width == extent.width && target.Extent.height == extent.height;
}

Result<std::shared_ptr<SwapchainState>> Prepare(const std::shared_ptr<PresentState> &state,
                                                TargetRequest &request) {
  if (!request.Surface) {
    if (!CanReplaceTarget(*state)) { return std::unexpected(RetiringTarget()); }
    return std::shared_ptr<SwapchainState>{};
  }
  const auto existing = FindTargets(*state, request.Surface);
  const auto &before = existing.Usable;
  if (!before && existing.Retired) {
    RetireInactive(*state);
    if (state->Active && state->Active->DriverRetired && state->Active.use_count() == 1) {
      (void)state->Retirement.Take(state->Active);
    }
    return std::unexpected(RetiringTarget());
  }
  if (!before && state->Retirement.BlocksWindow(request.Surface->WindowId)) {
    return std::unexpected(RetiringTarget());
  }
  if (before) {
    if (before == state->Active && state->Failed) { return std::unexpected(*state->Failed); }
    request.Surface = before->Surface;
    const auto sameExtent = ExtentMatches(*before, request.Extent);
    if (!sameExtent) { return std::unexpected(sameExtent.error()); }
    if (*sameExtent && before != state->Active && !CanReplaceTarget(*state)) {
      return std::unexpected(RetiringTarget());
    }
    if (*sameExtent) { return before; }
  }
  if (!CanReplaceTarget(*state)) { return std::unexpected(RetiringTarget()); }
  auto prepared = CreateSwapchain(
      *state, request.Surface, request.Extent, state->Generation.load() + 1, before);
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
    if (state->Active && state->Active->DriverRetired && prepared.error().Kind != ErrorKind::Busy) {
      FailPresentation(*state, prepared.error(), state->Generation.load());
    }
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
