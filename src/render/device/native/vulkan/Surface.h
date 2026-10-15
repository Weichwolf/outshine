#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_SURFACE_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_SURFACE_H
#ifdef __linux__
#include "Context.h"
#include <vector>

namespace outshine::Render::Native::Vulkan {

[[nodiscard]] Result<std::vector<const char *>> WindowExtensions(Context &context,
                                                                 SDL_Window *window);
[[nodiscard]] Result<void> CreateSurface(Context &context, SDL_Window *window);
[[nodiscard]] Result<VkSurfaceKHR> X11Surface(Context &context, SDL_Window *window);
[[nodiscard]] Result<VkSurfaceKHR> WaylandSurface(Context &context, SDL_Window *window);

}
#endif
#endif
