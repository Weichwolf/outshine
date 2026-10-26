#ifdef __linux__
#include "Surface.h"
#include <SDL3/SDL.h>
#include <vulkan/vulkan_wayland.h>
#include <expected>

namespace outshine::Render::Native::Vulkan {

Result<VkSurfaceKHR> WaylandSurface(Context &context, SDL_Window *window) {
  const auto properties = SDL_GetWindowProperties(window);
  VkWaylandSurfaceCreateInfoKHR create{};
  create.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
  create.display = static_cast<wl_display *>(
      SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr));
  create.surface = static_cast<wl_surface *>(
      SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr));
  const auto make = reinterpret_cast<PFN_vkCreateWaylandSurfaceKHR>(
      vkGetInstanceProcAddr(context.Instance, "vkCreateWaylandSurfaceKHR"));
  if (make == nullptr || create.display == nullptr || create.surface == nullptr) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Unsupported,
              .Message = "SDL Wayland window properties cannot create a Vulkan surface"});
  }
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  const auto result = make(context.Instance, &create, nullptr, &surface);
  if (result != VK_SUCCESS) {
    return std::unexpected(Failure("vkCreateWaylandSurfaceKHR", result));
  }
  return surface;
}

}
#endif
