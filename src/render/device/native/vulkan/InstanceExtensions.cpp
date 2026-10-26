#ifdef __linux__
#include "Surface.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <cstdint>
#include <array>
#include <cstring>
#include <expected>
#include <string>
#include <vector>

namespace outshine::Render::Native::Vulkan {
namespace {

bool Has(const std::vector<VkExtensionProperties> &extensions, const char *name) {
  return std::ranges::any_of(extensions, [name](const VkExtensionProperties &item) {
    return std::strcmp(item.extensionName, name) == 0;
  });
}

Result<void> WindowRequirements(Context &context,
                                SDL_Window *window,
                                const std::vector<VkExtensionProperties> &available) {
  if (window == nullptr) { return {}; }
  if (!SDL_IsMainThread() || SDL_WasInit(SDL_INIT_VIDEO) == 0) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid,
              .Message = "Vulkan window preparation requires SDL video on the main thread"});
  }
  if (!SDL_Vulkan_LoadLibrary(nullptr)) {
    return std::unexpected(Error{.Kind = ErrorKind::Unsupported, .Message = SDL_GetError()});
  }
  Uint32 count = 0;
  const auto *const needed = SDL_Vulkan_GetInstanceExtensions(&count);
  Result<void> result;
  if (needed == nullptr) {
    result = std::unexpected(Error{.Kind = ErrorKind::Unsupported, .Message = SDL_GetError()});
  } else {
    for (Uint32 at = 0; at < count; ++at) {
      if (!Has(available, needed[at])) {
        result = std::unexpected(Error{.Kind = ErrorKind::Unsupported,
                                       .Message = "required Vulkan window extension unavailable"});
        break;
      }
      if (std::ranges::find(context.InstanceExtensions, needed[at]) ==
          context.InstanceExtensions.end()) {
        context.InstanceExtensions.emplace_back(needed[at]);
      }
    }
  }
  SDL_Vulkan_UnloadLibrary();
  return result;
}

}

Result<void> PrepareInstanceExtensions(Context &context, SDL_Window *window) {
  uint32_t count = 0;
  auto result = vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
  if (result != VK_SUCCESS) {
    return std::unexpected(Failure("vkEnumerateInstanceExtensionProperties", result));
  }
  std::vector<VkExtensionProperties> available(count);
  result = vkEnumerateInstanceExtensionProperties(nullptr, &count, available.data());
  if (result != VK_SUCCESS) {
    return std::unexpected(Failure("vkEnumerateInstanceExtensionProperties", result));
  }
  constexpr std::array common{VK_KHR_SURFACE_EXTENSION_NAME,
                              VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
                              VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME};
  context.WsiEnabled =
      std::ranges::all_of(common, [&](const char *name) { return Has(available, name); });
  if (!context.WsiEnabled) {
    if (window == nullptr) { return {}; }
    return std::unexpected(Error{.Kind = ErrorKind::Unsupported,
                                 .Message = "Vulkan surface maintenance extensions unavailable"});
  }
  for (const auto *name : common) { context.InstanceExtensions.emplace_back(name); }
  constexpr std::array platforms{"VK_KHR_xlib_surface",
                                 "VK_KHR_xcb_surface",
                                 "VK_KHR_wayland_surface",
                                 "VK_KHR_display",
                                 "VK_EXT_headless_surface"};
  for (const auto *name : platforms) {
    if (Has(available, name)) { context.InstanceExtensions.emplace_back(name); }
  }
  return WindowRequirements(context, window, available);
}

}
#endif
