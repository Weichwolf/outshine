#ifndef OUTSHINE_TEST_RENDER_NATIVERADIANCE_H
#define OUTSHINE_TEST_RENDER_NATIVERADIANCE_H
#include "NativeGpuCheck.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>

namespace outshine::Test::Gpu {
inline std::unique_ptr<Texture> RadianceSource(Device &device) {
  constexpr std::array<uint16_t, 16> values{
      0x3c00, 0, 0, 0x3800, 0, 0x4000, 0, 0x3c00, 0, 0, 0, 0, 0, 0, 0x3800, 0x3400};
  static_assert(sizeof(values) == 32);
  auto staging = Require(device.CreateBuffer(
      {.Bytes = sizeof(values), .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*staging, {.Bytes = sizeof(values)}));
  std::memcpy(mapped.data(), values.data(), sizeof(values));
  Require(device.Unmap(*staging));
  auto source =
      Require(device.CreateTexture({.Width = 2,
                                    .Height = 2,
                                    .Texels = Format::Rgba16Float,
                                    .Use = TextureUse::Sampled | TextureUse::CopyDestination}));
  auto commands = Require(device.Begin());
  Require(commands->Copy(*staging, *source, {.RowBytes = 16, .Region = {.Width = 2, .Height = 2}}));
  Require(device.Submit(std::move(commands)));
  return source;
}
}
#endif
