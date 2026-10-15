#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_METAL_METALSHADER_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_METAL_METALSHADER_H

#include "../Commands.h"
#include <string>

namespace outshine::Render::Native::Metal {

constexpr uint32_t kBufferSizesSlot = 25;

struct ResourceBinding {
  ShaderBinding Source;
  uint32_t Buffer = 0;
  uint32_t Texture = 0;
  uint32_t Sampler = 0;
};

struct Shader {
  std::string Source, Entry;
  ComputeLayout Layout;
  std::vector<ResourceBinding> Resources;
  bool NeedsBufferSizes = false;
};

[[nodiscard]] Result<Shader> Translate(std::span<const uint32_t> spirv, std::string_view entry);

}
#endif
