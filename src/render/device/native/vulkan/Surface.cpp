#ifdef __linux__
#include "Surface.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <cstring>
#include <expected>
#include <vector>

namespace outshine::Render::Native::Vulkan {

Result<std::vector<const char *>> WindowExtensions(Context &context, SDL_Window *window) {
  std::vector<const char *> extensions;
  if (window == nullptr) { return extensions; }
  if (!SDL_IsMainThread() || SDL_WasInit(SDL_INIT_VIDEO) == 0) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid,
              .Message = "Vulkan window devices require initialized SDL video on the main thread"});
  }
  if (!SDL_Vulkan_LoadLibrary(nullptr)) {
    return std::unexpected(Error{.Kind = ErrorKind::Unsupported, .Message = SDL_GetError()});
  }
  context.SdlVulkan = true;
  Uint32 count = 0;
  const auto *const required = SDL_Vulkan_GetInstanceExtensions(&count);
  if (required == nullptr) {
    return std::unexpected(Error{.Kind = ErrorKind::Unsupported, .Message = SDL_GetError()});
  }
  extensions.assign(required, required + count);
  extensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
  extensions.push_back(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
  return extensions;
}

Result<void> CreateSurface(Context &context, SDL_Window *window) {
  if (window == nullptr) { return {}; }
  if ((SDL_GetWindowFlags(window) & SDL_WINDOW_VULKAN) != 0) {
    if (!SDL_Vulkan_CreateSurface(window, context.Instance, nullptr, &context.Surface)) {
      return std::unexpected(Error{.Kind = ErrorKind::Unsupported, .Message = SDL_GetError()});
    }
    return {};
  }
  const auto *const driver = SDL_GetCurrentVideoDriver();
  Result<VkSurfaceKHR> surface = std::unexpected(
      Error{.Kind = ErrorKind::Unsupported,
            .Message = "Vulkan presentation requires an X11 or Wayland SDL window"});
  if (driver != nullptr && std::strcmp(driver, "x11") == 0) {
    surface = X11Surface(context, window);
  }
  if (driver != nullptr && std::strcmp(driver, "wayland") == 0) {
    surface = WaylandSurface(context, window);
  }
  if (!surface) { return std::unexpected(surface.error()); }
  context.Surface = *surface;
  return {};
}

}
#endif
