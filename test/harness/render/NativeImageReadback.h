#ifndef OUTSHINE_TEST_RENDER_NATIVEIMAGEREADBACK_H
#define OUTSHINE_TEST_RENDER_NATIVEIMAGEREADBACK_H

#include "src/render/device/ReadbackTransfer.h"
#include "Check.h"
#include "NativeGpuCheck.h"
#include "NativeOffline.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace outshine::Test::Gpu {
struct ImagePixels {
  uint64_t RowBytes = 0;
  std::vector<std::byte> Rows;
};

inline ImagePixels ReadImage(Render::Native::Device &device,
                             const Render::Native::Texture &texture,
                             Render::Native::TextureRegion region) {
  auto read =
      Require(RetryOffline([&] { return Render::ReadbackTransfer::Start(device, texture, region); },
                           std::chrono::seconds(5)));
  auto bytes = Require(read.Wait(device, std::chrono::seconds(5)));
  CHECK(bytes, "native image readback completes within its explicit offline budget");
  if (!bytes) { Failed(); }
  return {.RowBytes = read.RowBytes(), .Rows = {bytes->begin(), bytes->end()}};
}
}
#endif
