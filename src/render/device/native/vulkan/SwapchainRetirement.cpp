#ifdef __linux__
#include "SwapchainRetirement.h"
#include "Presenter.h"
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <system_error>
#include <utility>

namespace outshine::Render::Native::Vulkan {
SwapchainRetirement::~SwapchainRetirement() {
  Stop();
}

Result<void> SwapchainRetirement::Start() {
  if (Started_) { return {}; }
  Stopping_ = false;
  const auto result = pthread_create(&Worker_, nullptr, Entry, this);
  if (result != 0) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Driver,
              .Message = std::error_code(result, std::generic_category()).message()});
  }
  Started_ = true;
  return {};
}

void SwapchainRetirement::Stop() {
  if (!Started_) { return; }
  {
    const std::scoped_lock lock(Gate_);
    Stopping_ = true;
  }
  Changed_.notify_one();
  pthread_join(Worker_, nullptr);
  Started_ = false;
}

bool SwapchainRetirement::Take(std::shared_ptr<SwapchainState> &target) {
  const std::scoped_lock lock(Gate_);
  if (target.use_count() != 1 || !Started_ || Stopping_) { return false; }
  if (Busy_) {
    Awaited_ = true;
    return false;
  }
  WindowId_ = target->Surface ? target->Surface->WindowId : 0;
  Pending_ = std::move(target);
  Busy_ = true;
  Changed_.notify_one();
  return true;
}

bool SwapchainRetirement::BlocksWindow(uint32_t windowId) {
  const std::scoped_lock lock(Gate_);
  const bool blocked = windowId != 0 && Busy_ && windowId == WindowId_;
  Awaited_ = Awaited_ || blocked;
  return blocked;
}

void *SwapchainRetirement::Entry(void *self) {
  static_cast<SwapchainRetirement *>(self)->Run();
  return nullptr;
}

void SwapchainRetirement::Run() {
  while (true) {
    std::shared_ptr<SwapchainState> target;
    {
      std::unique_lock lock(Gate_);
      Changed_.wait(lock, [&] { return Stopping_ || Pending_; });
      if (!Pending_) { break; }
      target = std::move(Pending_);
    }
    target.reset();
    bool wake = false;
    {
      const std::scoped_lock lock(Gate_);
      Busy_ = false;
      WindowId_ = 0;
      wake = std::exchange(Awaited_, false);
    }
    if (wake) { Dispatcher_.Wake(); }
  }
}

void RetireInactive(PresentState &state) {
  auto target = state.Inactive.begin();
  while (target != state.Inactive.end()) {
    if (target->use_count() == 1 && state.Retirement.Take(*target)) {
      target = state.Inactive.erase(target);
    } else {
      ++target;
    }
  }
}

bool CanRetainTarget(const PresentState &state) noexcept {
  return state.Inactive.size() < state.Slots.size() + 1;
}

bool CanReplaceTarget(const PresentState &state) noexcept {
  return !state.Active || CanRetainTarget(state);
}
}
#endif
