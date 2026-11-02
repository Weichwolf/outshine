#ifndef OUTSHINE_TEST_NATIVE_MATERIAL_CAPTURE_TEXTURE_H
#define OUTSHINE_TEST_NATIVE_MATERIAL_CAPTURE_TEXTURE_H

#include "NativeGpuCheck.h"
#include "Check.h"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include <memory>
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

struct CaptureTexture {
  std::unique_ptr<Texture> Image;
  std::unique_ptr<Sampler> Sampling;
};

inline CaptureTexture
UploadCaptureTexture(Device &device, Commands &commands, const char *path, uint32_t side) {
  constexpr uint64_t alignment = 256;
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  uint64_t sourceBytes = 0;
  uint64_t uploadBytes = 0;
  for (uint32_t width = side; width != 0; width /= 2) {
    sourceBytes += uint64_t{width} * width;
    uploadBytes += ((width + alignment - 1) / alignment) * alignment * width;
  }
  const std::streamoff length = file ? file.tellg() : std::streampos(-1);
  const bool complete = std::cmp_equal(length, sourceBytes);
  CHECK(complete, "R8 input contains exactly the full bounded mip chain");
  if (!complete) { Failed(); }
  auto upload = Require(device.CreateBuffer(
      {.Bytes = uploadBytes, .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*upload, {.Bytes = uploadBytes}));
  std::memset(mapped.data(), 0, mapped.size());
  file.seekg(0);
  uint64_t offset = 0;
  for (uint32_t width = side; width != 0; width /= 2) {
    const uint64_t rowBytes = ((width + alignment - 1) / alignment) * alignment;
    for (uint32_t row = 0; row < width; ++row) {
      file.read(reinterpret_cast<char *>(mapped.data() + offset + row * rowBytes), width);
    }
    offset += rowBytes * width;
  }
  CHECK(file, "R8 mip rows are completely read into aligned transfer storage");
  if (!file) { Failed(); }
  Require(device.Unmap(*upload));
  auto image =
      Require(device.CreateTexture({.Width = side,
                                    .Height = side,
                                    .Mips = static_cast<uint32_t>(std::bit_width(side)),
                                    .Texels = Format::R8Unorm,
                                    .Use = TextureUse::Sampled | TextureUse::CopyDestination}));
  offset = 0;
  uint32_t mip = 0;
  for (uint32_t width = side; width != 0; width /= 2, ++mip) {
    const uint64_t rowBytes = ((width + alignment - 1) / alignment) * alignment;
    Require(commands.Copy(*upload,
                          *image,
                          {.BufferOffset = offset,
                           .RowBytes = rowBytes,
                           .Region = {.Mip = mip, .Width = width, .Height = width}}));
    offset += rowBytes * width;
  }
  return {.Image = std::move(image),
          .Sampling = Require(
              device.CreateSampler({.MaxLod = static_cast<float>(std::bit_width(side) - 1)}))};
}
}
#endif
