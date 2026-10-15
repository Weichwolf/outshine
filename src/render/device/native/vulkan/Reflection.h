#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_REFLECTION_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_REFLECTION_H

#ifdef __linux__
#include "Pipeline.h"
#include <spirv_cross/spirv_cross_c.h>

namespace outshine::Render::Native::Vulkan {

struct Reflection {
  spvc_context Owner = nullptr;
  spvc_compiler Compiler = nullptr;
  spvc_resources Resources = nullptr;

  Reflection() = default;

  ~Reflection() {
    if (Owner != nullptr) { spvc_context_destroy(Owner); }
  }

  Reflection(const Reflection &) = delete;
  Reflection &operator=(const Reflection &) = delete;
};

[[nodiscard]] Error ReflectionError(const Reflection &reflection);
[[nodiscard]] Result<void>
AppendImages(const Reflection &reflection, spvc_resource_type kind, ComputeReflection &into);

}
#endif
#endif
