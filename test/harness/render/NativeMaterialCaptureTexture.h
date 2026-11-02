#ifndef OUTSHINE_TEST_NATIVE_MATERIAL_CAPTURE_TEXTURE_H
#define OUTSHINE_TEST_NATIVE_MATERIAL_CAPTURE_TEXTURE_H

#include "NativeGpuCheck.h"
#include "R8FieldTexture.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <span>
#include <vector>
#include <utility>

namespace outshine::Test::Gpu {
struct alignas(16) CaptureTextureQuery {
  std::array<float, 4> PositionDx, DyScaleBias, Control;
};

static_assert(sizeof(CaptureTextureQuery) == 48 && alignof(CaptureTextureQuery) == 16 &&
              offsetof(CaptureTextureQuery, PositionDx) == 0 &&
              offsetof(CaptureTextureQuery, DyScaleBias) == 16 &&
              offsetof(CaptureTextureQuery, Control) == 32);

struct CaptureTextureSize {
  uint32_t Side = 0;
  uint32_t Mips = 0;
};

inline Render::R8FieldTexture UploadCaptureTexture(Device &device,
                                                   Commands &commands,
                                                   const char *path,
                                                   CaptureTextureSize extent) {
  const auto side = extent.Side;
  const auto mipCount = extent.Mips;
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  uint64_t sourceBytes = 0;
  for (uint32_t mip = 0; mip < mipCount; ++mip) {
    const uint32_t width = side >> mip;
    sourceBytes += uint64_t{width} * width;
  }
  const std::streamoff length = file ? file.tellg() : std::streampos(-1);
  CHECK(std::cmp_equal(length, sourceBytes), "R8 input contains exactly its bounded mip range");
  if (!std::cmp_equal(length, sourceBytes)) { Failed(); }
  std::vector<uint8_t> pixels(sourceBytes);
  file.seekg(0);
  file.read(reinterpret_cast<char *>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
  CHECK(file, "R8 input is completely read");
  if (!file) { Failed(); }
  std::array<Render::R8FieldMip, 32> levels;
  size_t offset = 0;
  for (uint32_t mip = 0; mip < mipCount; ++mip) {
    const uint32_t width = side >> mip;
    const size_t size = size_t{width} * width;
    levels[mip] = {.Side = width, .Pixels = std::span(pixels).subspan(offset, size)};
    offset += size;
  }
  const auto mips = std::span(levels).first(mipCount);
  const auto bytes = Require(Render::R8FieldTextureStorage(mips));
  return Require(Render::UploadR8FieldTexture(device, commands, mips, bytes));
}
}
#endif
