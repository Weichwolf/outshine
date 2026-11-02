#ifndef OUTSHINE_RENDER_DEVICE_R8FIELDTEXTURE_H
#define OUTSHINE_RENDER_DEVICE_R8FIELDTEXTURE_H

#include "native/Device.h"
#include <cstdint>
#include <memory>
#include <span>

namespace outshine::Render {
struct R8FieldMip {
  uint32_t Side = 0;
  std::span<const uint8_t> Pixels;
};

struct R8FieldStorage {
  uint64_t TexelBytes = 0;
  uint64_t TransferBytes = 0;
};

struct R8FieldTexture {
  std::unique_ptr<Native::Texture> Image;
  std::unique_ptr<Native::Sampler> Sampling;
  R8FieldStorage Storage;
};

[[nodiscard]] Native::Result<R8FieldStorage>
R8FieldTextureStorage(std::span<const R8FieldMip> mips);

// Record into the caller's upload batch; native commands retain transfer backing until completion.
[[nodiscard]] Native::Result<R8FieldTexture> UploadR8FieldTexture(Native::Device &device,
                                                                  Native::Commands &commands,
                                                                  std::span<const R8FieldMip> mips,
                                                                  R8FieldStorage limits);
}
#endif
