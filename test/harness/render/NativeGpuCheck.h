#ifndef OUTSHINE_TEST_RENDER_NATIVEGPUCHECK_H
#define OUTSHINE_TEST_RENDER_NATIVEGPUCHECK_H
#include "NativeDeviceCheck.h"
#include "SpirvFile.h"

namespace outshine::Test::Gpu {
inline std::vector<uint32_t> Shader(const char *name) {
  auto words = outshine::Render::ReadShaderSpirv(name);
  CHECK(words, "the built shader is a complete SPIR-V module");
  if (!words) {
    std::println(stderr, "{}", words.error());
    Failed();
  }
  return std::move(*words);
}

}
#endif
