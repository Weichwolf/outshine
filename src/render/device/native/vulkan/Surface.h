#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_SURFACE_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_SURFACE_H
#ifdef __linux__
#include "Context.h"
#include <memory>
#include <cstdint>

namespace outshine::Render::Native::Vulkan {

struct SurfaceState {
  std::shared_ptr<Context> Owner;
  VkSurfaceKHR Handle = VK_NULL_HANDLE;
  uint32_t WindowId = 0;
  ~SurfaceState();
};

[[nodiscard]] bool HasPresentExtensions(VkPhysicalDevice physical);
[[nodiscard]] Result<void> PrepareInstanceExtensions(Context &context, SDL_Window *window);
[[nodiscard]] Result<void> CreateSurface(Context &context, SDL_Window *window);
[[nodiscard]] Result<std::shared_ptr<SurfaceState>>
PrepareSurface(const std::shared_ptr<Context> &context, SDL_Window *window);
[[nodiscard]] Result<VkSurfaceKHR> X11Surface(Context &context, SDL_Window *window);
[[nodiscard]] Result<VkSurfaceKHR> WaylandSurface(Context &context, SDL_Window *window);

}
#endif
#endif
