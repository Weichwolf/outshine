#ifndef OUTSHINE_TEST_NATIVE_MATERIAL_CAPTURE_H
#define OUTSHINE_TEST_NATIVE_MATERIAL_CAPTURE_H

#include "NativeGpuCheck.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iosfwd>
#include <vector>

namespace outshine::Test::Gpu {
struct alignas(16) CapturePixel {
  std::array<float, 4> First, Second;
};

struct alignas(16) CaptureUniform {
  uint32_t Count = 0, First = 0, Width = 0, Reserved = 0;
};

static_assert(sizeof(CapturePixel) == 32 && alignof(CapturePixel) == 16 &&
              offsetof(CapturePixel, First) == 0 && offsetof(CapturePixel, Second) == 16);
static_assert(sizeof(CaptureUniform) == 16 && alignof(CaptureUniform) == 16 &&
              offsetof(CaptureUniform, Count) == 0 && offsetof(CaptureUniform, First) == 4 &&
              offsetof(CaptureUniform, Width) == 8 && offsetof(CaptureUniform, Reserved) == 12);

inline std::vector<uint32_t> ReadCaptureWords(const char *path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  const std::streamoff size = file ? file.tellg() : std::streampos(-1);
  constexpr std::streamoff limit = 64L * 1024 * 1024;
  CHECK(size > 0 && size <= limit && size % sizeof(uint32_t) == 0, "input words are bounded");
  if (size <= 0 || size > limit || size % sizeof(uint32_t) != 0) { Failed(); }
  std::vector<uint32_t> words(static_cast<size_t>(size) / sizeof(uint32_t));
  file.seekg(0);
  if (!file.read(reinterpret_cast<char *>(words.data()), size)) {
    CHECK(false, "input words read completely");
    Failed();
  }
  return words;
}
}
#endif
