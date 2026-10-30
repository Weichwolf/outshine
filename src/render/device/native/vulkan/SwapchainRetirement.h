#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_SWAPCHAINRETIREMENT_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_SWAPCHAINRETIREMENT_H
#ifdef __linux__
#include "Commands.h"
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <pthread.h>

namespace outshine::Render::Native::Vulkan {
struct PresentState;
struct SwapchainState;

class SwapchainRetirement {
public:
  explicit SwapchainRetirement(PresentState &dispatcher) : Dispatcher_(dispatcher) {}

  ~SwapchainRetirement();
  SwapchainRetirement(const SwapchainRetirement &) = delete;
  SwapchainRetirement &operator=(const SwapchainRetirement &) = delete;

  [[nodiscard]] Result<void> Start();
  void Stop();
  // Only a sole owner enters the single pending-or-executing retirement slot.
  [[nodiscard]] bool Take(std::shared_ptr<SwapchainState> &target);
  [[nodiscard]] bool BlocksWindow(uint32_t windowId);

private:
  static void *Entry(void *self);
  void Run();

  // Owned last by Dispatcher_; Stop joins before its wakeup fields are destroyed.
  PresentState &Dispatcher_;
  std::mutex Gate_;
  std::condition_variable Changed_;
  std::shared_ptr<SwapchainState> Pending_;
  pthread_t Worker_{};
  uint32_t WindowId_ = 0;
  bool Started_ = false;
  bool Stopping_ = false;
  bool Busy_ = false;
  bool Awaited_ = false;
};
}
#endif
#endif
