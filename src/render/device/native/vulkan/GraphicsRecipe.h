#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_GRAPHICSRECIPE_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_GRAPHICSRECIPE_H
#ifdef __linux__
#include "Graphics.h"
#include <compare>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace outshine::Render::Native::Vulkan {
struct GraphicsRecipe {
  uint64_t Digest = 0;
  std::vector<std::byte> Bytes;

  [[nodiscard]] static GraphicsRecipe From(const GraphicsDescription &description);
  auto operator<=>(const GraphicsRecipe &) const = default;
};
}
#endif
#endif
