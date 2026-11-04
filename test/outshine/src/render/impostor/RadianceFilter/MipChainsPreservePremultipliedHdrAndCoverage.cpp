#include "RadianceFilter.h"
#include "Check.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeImageReadback.h"
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kWidth = 8, kHeight = 4, kMips = 4;

std::vector<float> Pixels() {
  std::vector<float> pixels(size_t{kWidth} * kHeight * 4);
  for (uint32_t row = 0; row < kHeight; ++row) {
    for (uint32_t col = 0; col < kWidth; ++col) {
      const float coverage = col < 4 && row < 2 ? 1.0f : 0.0f;
      const size_t at = (size_t{row} * kWidth + col) * 4;
      pixels[at] = coverage * 16;
      pixels[at + 1] = coverage * 4;
      pixels[at + 2] = coverage * 8;
      pixels[at + 3] = coverage;
    }
  }
  return pixels;
}

void Compare(Device &device, const Texture &image, std::span<const float> source, uint32_t mip) {
  const uint32_t width = std::max(1u, kWidth >> mip);
  const uint32_t height = std::max(1u, kHeight >> mip);
  const auto bytes = ReadImage(device, image, {.Mip = mip, .Width = width, .Height = height});
  std::vector<float> pixels(size_t{width} * height * 4);
  CHECK(SDL_ConvertPixels(static_cast<int>(width),
                          static_cast<int>(height),
                          SDL_PIXELFORMAT_RGBA64_FLOAT,
                          bytes.Rows.data(),
                          static_cast<int>(bytes.RowBytes),
                          SDL_PIXELFORMAT_RGBA128_FLOAT,
                          pixels.data(),
                          static_cast<int>(width * 16)),
        "native mip texels decode to linear floats");
  const uint32_t across = kWidth / width;
  const uint32_t down = kHeight / height;
  for (uint32_t row = 0; row < height; ++row) {
    for (uint32_t col = 0; col < width; ++col) {
      std::array<double, 4> sum{};
      for (uint32_t y = row * down; y < (row + 1) * down; ++y) {
        for (uint32_t x = col * across; x < (col + 1) * across; ++x) {
          for (size_t c = 0; c < 4; ++c) { sum[c] += source[(size_t{y} * kWidth + x) * 4 + c]; }
        }
      }
      for (size_t c = 0; c < 4; ++c) {
        const double expected = sum[c] / (across * down);
        CHECK(pixels[(size_t{row} * width + col) * 4 + c] == expected,
              "every mip preserves the same area-weighted HDR and coverage");
      }
    }
  }
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  const auto pixels = Pixels();
  std::vector<uint8_t> half(size_t{kWidth} * kHeight * 8);
  CHECK(SDL_ConvertPixels(kWidth,
                          kHeight,
                          SDL_PIXELFORMAT_RGBA128_FLOAT,
                          pixels.data(),
                          kWidth * 16,
                          SDL_PIXELFORMAT_RGBA64_FLOAT,
                          half.data(),
                          kWidth * 8),
        "the fixture packs exact HDR and coverage values");
  auto staging = Require(device->CreateBuffer(
      {.Bytes = half.size(), .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  std::memcpy(
      Require(device->MapWrite(*staging, {.Bytes = half.size()})).data(), half.data(), half.size());
  Require(device->Unmap(*staging));
  auto image =
      Require(device->CreateTexture({.Width = kWidth,
                                     .Height = kHeight,
                                     .Mips = kMips,
                                     .Texels = Format::Rgba16Float,
                                     .Use = TextureUse::Sampled | TextureUse::Storage |
                                            TextureUse::CopySource | TextureUse::CopyDestination}));
  auto commands = Require(device->Begin());
  Require(commands->Copy(
      *staging,
      *image,
      {.RowBytes = uint64_t{kWidth} * 8, .Region = {.Width = kWidth, .Height = kHeight}}));
  RadianceFilter filter;
  CHECK(!filter.EncodeMipmaps(*commands, *image), "unconfigured mip filtering records no work");
  Require(filter.Configure(*device));
  CHECK(!filter.Encode(*commands,
                       TextureView{.Image = image.get(), .FirstMip = kMips},
                       {.Image = image.get(), .FirstMip = 1}),
        "out-of-range mip views are rejected before dispatch");
  Require(filter.EncodeMipmaps(*commands, *image));
  const auto submitted = Require(device->Submit(std::move(commands)));
  CHECK(Require(device->Wait(submitted, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "the complete mip chain finishes within the offline deadline");
  for (uint32_t mip = 0; mip < kMips; ++mip) { Compare(*device, *image, pixels, mip); }
  return Report();
}
