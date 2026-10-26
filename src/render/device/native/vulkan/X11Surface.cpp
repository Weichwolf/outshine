#ifdef __linux__
#include "Surface.h"
#include <SDL3/SDL.h>
#include <X11/Xlib.h>
#include <vulkan/vulkan_xlib.h>
#include <expected>

namespace outshine::Render::Native::Vulkan {

Result<VkSurfaceKHR> X11Surface(Context &context, SDL_Window *window) {
  const auto properties = SDL_GetWindowProperties(window);
  VkXlibSurfaceCreateInfoKHR create{};
  create.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
  create.dpy = static_cast<Display *>(
      SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr));
  create.window =
      static_cast<Window>(SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
  const auto make = reinterpret_cast<PFN_vkCreateXlibSurfaceKHR>(
      vkGetInstanceProcAddr(context.Instance, "vkCreateXlibSurfaceKHR"));
  if (make == nullptr || create.dpy == nullptr || create.window == 0) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Unsupported,
              .Message = "SDL X11 window properties cannot create a Vulkan surface"});
  }
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  const auto result = make(context.Instance, &create, nullptr, &surface);
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkCreateXlibSurfaceKHR", result)); }
  return surface;
}

}
#endif
