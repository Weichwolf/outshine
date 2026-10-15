#ifdef __linux__
#include "Surface.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <cstring>
#include <expected>
#include <memory>
#include <utility>

namespace outshine::Render::Native::Vulkan {

SurfaceState::~SurfaceState() {
  if (Handle != VK_NULL_HANDLE) { vkDestroySurfaceKHR(Owner->Instance, Handle, nullptr); }
}

namespace {

Result<VkSurfaceKHR> SdlSurface(Context &context, SDL_Window *window) {
  if (!SDL_Vulkan_LoadLibrary(nullptr)) {
    return std::unexpected(Error{.Kind = ErrorKind::Unsupported, .Message = SDL_GetError()});
  }
  Uint32 count = 0;
  const auto *const required = SDL_Vulkan_GetInstanceExtensions(&count);
  bool enabled = required != nullptr;
  for (Uint32 at = 0; enabled && at < count; ++at) {
    enabled = std::ranges::find(context.InstanceExtensions, required[at]) !=
              context.InstanceExtensions.end();
  }
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  Result<VkSurfaceKHR> result = std::unexpected(
      Error{.Kind = ErrorKind::Unsupported,
            .Message = "window extensions were not enabled for this Vulkan instance"});
  if (enabled) {
    if (SDL_Vulkan_CreateSurface(window, context.Instance, nullptr, &surface)) {
      result = surface;
    } else {
      result = std::unexpected(Error{.Kind = ErrorKind::Unsupported, .Message = SDL_GetError()});
    }
  }
  SDL_Vulkan_UnloadLibrary();
  return result;
}

Result<VkSurfaceKHR> MakeSurface(Context &context, SDL_Window *window) {
  if (!SDL_IsMainThread() || SDL_WasInit(SDL_INIT_VIDEO) == 0 || SDL_GetWindowID(window) == 0) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid,
              .Message = "Vulkan target needs a live SDL window on the main thread"});
  }
  if ((SDL_GetWindowFlags(window) & SDL_WINDOW_VULKAN) != 0) { return SdlSurface(context, window); }
  const auto *const driver = SDL_GetCurrentVideoDriver();
  if (driver != nullptr && std::strcmp(driver, "x11") == 0 &&
      std::ranges::find(context.InstanceExtensions, "VK_KHR_xlib_surface") !=
          context.InstanceExtensions.end()) {
    return X11Surface(context, window);
  }
  if (driver != nullptr && std::strcmp(driver, "wayland") == 0 &&
      std::ranges::find(context.InstanceExtensions, "VK_KHR_wayland_surface") !=
          context.InstanceExtensions.end()) {
    return WaylandSurface(context, window);
  }
  return std::unexpected(
      Error{.Kind = ErrorKind::Unsupported,
            .Message = "Vulkan instance cannot attach this SDL window platform"});
}

}

Result<void> CreateSurface(Context &context, SDL_Window *window) {
  if (window == nullptr) { return {}; }
  auto made = MakeSurface(context, window);
  if (!made) { return std::unexpected(made.error()); }
  context.Surface = *made;
  return {};
}

Result<std::shared_ptr<SurfaceState>> PrepareSurface(const std::shared_ptr<Context> &context,
                                                     SDL_Window *window) {
  if (!context->PresentCapable) {
    return std::unexpected(Error{.Kind = ErrorKind::Unsupported,
                                 .Message = "Vulkan device was not prepared for presentation"});
  }
  auto surface = std::make_shared<SurfaceState>();
  surface->Owner = context;
  surface->WindowId = SDL_GetWindowID(window);
  if (context->Surface != VK_NULL_HANDLE) {
    surface->Handle = std::exchange(context->Surface, VK_NULL_HANDLE);
  } else {
    auto made = MakeSurface(*context, window);
    if (!made) { return std::unexpected(made.error()); }
    surface->Handle = *made;
  }
  VkBool32 supported = VK_FALSE;
  const auto result = vkGetPhysicalDeviceSurfaceSupportKHR(
      context->Physical, context->QueueFamily, surface->Handle, &supported);
  if (result != VK_SUCCESS) {
    return std::unexpected(Failure("vkGetPhysicalDeviceSurfaceSupportKHR", result));
  }
  if (supported == VK_FALSE) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Unsupported,
              .Message = "Vulkan queue family cannot present to the requested surface"});
  }
  return surface;
}

}
#endif
