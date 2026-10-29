#include "Device.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <dlfcn.h>
#include <memory>
#include <mutex>
#include <print>
#include <thread>
#include <utility>

namespace {
using namespace outshine::Render::Native;
std::atomic<bool> failImages{true};
std::atomic<VkSwapchainKHR> failedSwapchain{VK_NULL_HANDLE};
std::atomic<VkDevice> failedDevice{VK_NULL_HANDLE};
std::mutex gate;
std::condition_variable changed;
bool entered = false;
bool released = false;
bool timedOut = false;

template <class T> T Require(Result<T> value) {
  CHECK(value, "the actual Vulkan operation succeeds");
  if (!value) {
    std::println(stderr, "{}", value.error().Message);
    std::_Exit(outshine::Test::Report());
  }
  return std::move(*value);
}

void Exercise(SDL_Window *first, SDL_Window *second) {
  auto owner = Require(CreateDevice({.Validation = true}));
  auto other = Require(CreateDevice({.Validation = true}));
  const auto failed = owner->SetPresentTarget(first);
  CHECK(!failed && failed.error().Kind == ErrorKind::OutOfMemory,
        "image enumeration failure preserves the native allocation error");
  {
    std::unique_lock lock(gate);
    CHECK(changed.wait_for(lock, std::chrono::seconds(5), [] { return entered; }) && !timedOut,
          "the failed native claim returns while its real partial swapchain deletion is blocked");
  }
  CHECK(owner->SetPresentTarget(second),
        "the same actual presenter handles another window before failed deletion completes");
  const auto busy = other->SetPresentTarget(first);
  CHECK(!busy && busy.error().Kind == ErrorKind::Busy,
        "another device cannot acquire the surface claim during actual pending deletion");
  {
    const std::scoped_lock lock(gate);
    released = true;
  }
  changed.notify_all();
  const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  auto acquired = other->SetPresentTarget(first);
  while (!acquired && acquired.error().Kind == ErrorKind::Busy &&
         std::chrono::steady_clock::now() < end) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    acquired = other->SetPresentTarget(first);
  }
  CHECK(acquired, "the real window claim becomes available after actual driver deletion");
}
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetSwapchainImagesKHR(VkDevice device,
                                                                  VkSwapchainKHR swapchain,
                                                                  uint32_t *count,
                                                                  VkImage *images) {
  if (failImages.exchange(false)) {
    failedDevice.store(device);
    failedSwapchain.store(swapchain);
    return VK_ERROR_OUT_OF_HOST_MEMORY;
  }
  static const auto call =
      reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(dlsym(RTLD_NEXT, "vkGetSwapchainImagesKHR"));
  if (call == nullptr) { std::abort(); }
  return call(device, swapchain, count, images);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroySwapchainKHR(
    VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks *allocator) {
  if (device == failedDevice.load() && swapchain == failedSwapchain.load()) {
    std::unique_lock lock(gate);
    entered = true;
    changed.notify_all();
    timedOut = !changed.wait_for(lock, std::chrono::seconds(3), [] { return released; });
  }
  static const auto call =
      reinterpret_cast<PFN_vkDestroySwapchainKHR>(dlsym(RTLD_NEXT, "vkDestroySwapchainKHR"));
  if (call == nullptr) { std::abort(); }
  call(device, swapchain, allocator);
}

int main() {
  const bool video = SDL_Init(SDL_INIT_VIDEO);
  CHECK(video, "SDL video starts for the real Vulkan window case");
  if (!video) { return outshine::Test::Report(); }
  {
    using Window = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>;
    Window first(SDL_CreateWindow("partial swapchain", 64, 64, 0), SDL_DestroyWindow);
    Window second(SDL_CreateWindow("valid swapchain", 64, 64, 0), SDL_DestroyWindow);
    CHECK(first && second, "both borrowed native windows are alive during the entire case");
    if (first && second) { Exercise(first.get(), second.get()); }
  }
  SDL_Quit();
  outshine::Test::Covers("real Vulkan image enumeration failure after successful swapchain "
                         "creation, blocked actual destruction, independent presenter progress, "
                         "exclusive surface claim and joined device shutdown");
  return outshine::Test::Report();
}
