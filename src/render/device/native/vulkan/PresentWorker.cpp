#ifdef __linux__
#include "Presenter.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>

namespace outshine::Render::Native::Vulkan {
namespace {

struct Request {
  std::shared_ptr<SurfaceState> Surface;
  VkExtent2D Extent{};
  uint64_t Generation = 0;
  bool Failed = false;
};

Request Requested(PresentState &state) {
  const std::scoped_lock lock(state.Gate);
  return {.Surface = state.Surface,
          .Extent = {.width = state.Width, .height = state.Height},
          .Generation = state.Generation.load(),
          .Failed = state.Failed.has_value()};
}

bool Target(const std::shared_ptr<PresentState> &state, const Request &request) {
  if (!request.Surface || request.Failed || request.Extent.width == 0 ||
      request.Extent.height == 0) {
    return false;
  }
  if (state->Active && state->Active->Generation == request.Generation) { return true; }
  const auto before =
      state->Active && state->Active->Surface == request.Surface ? state->Active : nullptr;
  auto made = CreateSwapchain(request.Surface, request.Extent, request.Generation, before);
  if (!made) {
    if (made.error().Kind != ErrorKind::Busy) {
      FailPresentation(*state, made.error(), request.Generation);
    }
    return false;
  }
  if (state->Active) { state->Inactive.push_back(std::move(state->Active)); }
  state->Attempt.reset();
  state->Active = std::move(*made);
  return true;
}

bool Progress(const std::shared_ptr<PresentState> &state, const Request &request, bool stopping) {
  bool pending = false;
  for (std::size_t slot = 0; slot < state->Slots.size(); ++slot) {
    std::shared_ptr<PresentLease> lease;
    {
      const std::scoped_lock lock(state->Gate);
      lease = state->Slots[slot];
    }
    if (!lease) { continue; }
    if (lease->Generation != request.Generation) {
      auto before = PresentPhase::Ready;
      lease->Phase.compare_exchange_strong(before, PresentPhase::Cancelled);
    }
    const auto advanced = AdvanceLease(lease, stopping);
    if (!advanced) {
      FailPresentation(*state, advanced.error(), lease->Generation);
      pending = true;
      continue;
    }
    if (lease->Generation != request.Generation) {
      auto before = PresentPhase::Ready;
      lease->Phase.compare_exchange_strong(before, PresentPhase::Cancelled);
    }
    if (*advanced) {
      const std::scoped_lock lock(state->Gate);
      state->Slots[slot].reset();
    } else {
      const auto phase = lease->Phase.load();
      pending = pending || phase == PresentPhase::Acquiring || phase == PresentPhase::Submitted ||
                phase == PresentPhase::Queued || phase == PresentPhase::Cancelled;
    }
  }
  return pending;
}

bool Fill(const std::shared_ptr<PresentState> &state) {
  bool acquired = false;
  for (std::size_t slot = 0; slot < state->Slots.size(); ++slot) {
    {
      const std::scoped_lock lock(state->Gate);
      if (state->Slots[slot]) { continue; }
    }
    auto lease = AcquireLease(state, state->Active);
    if (!lease) {
      FailPresentation(*state, lease.error(), state->Active->Generation);
      return false;
    }
    if (!*lease) { return true; }
    const std::scoped_lock lock(state->Gate);
    state->Slots[slot] = std::move(*lease);
    acquired = true;
  }
  return acquired;
}

}

void RunPresenter(const std::shared_ptr<PresentState> &state) {
  while (!state->Stop.load()) {
    const auto revision = state->Revision.load();
    SwitchTarget(state);
    const auto request = Requested(*state);
    bool pending = Progress(state, request, false);
    if (Target(state, request)) { pending = Fill(state) || pending; }
    std::erase_if(state->Inactive, [](const std::shared_ptr<SwapchainState> &target) {
      return target.use_count() == 1;
    });
    std::unique_lock lock(state->Gate);
    if (state->Stop.load()) { break; }
    const bool waitingTarget =
        state->Surface && state->Width > 0 && state->Height > 0 && !state->Failed &&
        (!state->Active || state->Active->Generation != state->Generation.load());
    if (pending || waitingTarget) {
      state->Changed.wait_for(lock, std::chrono::milliseconds(1), [&] {
        return state->Stop.load() || state->Revision.load() != revision;
      });
    } else {
      lock.unlock();
      state->Revision.wait(revision);
    }
  }
}

void DrainPresenter(const std::shared_ptr<PresentState> &state) {
  const auto request = Requested(*state);
  Progress(state, request, true);
  state->Attempt.reset();
  state->Active.reset();
  state->Inactive.clear();
  state->Surface.reset();
}

}
#endif
