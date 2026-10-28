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
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kWidth = 128, kHeight = 64;

std::vector<float> SourcePixels() {
  std::vector<float> result(size_t{kWidth} * kHeight * 4);
  for (uint32_t y = 0; y < kHeight; ++y) {
    for (uint32_t x = 0; x < kWidth; ++x) {
      const size_t at = (size_t{y} * kWidth + x) * 4;
      const float alpha = static_cast<float>((x + y * 3) % 5) / 4;
      result[at] = alpha * static_cast<float>(x % 31) / 2;
      result[at + 1] = alpha * static_cast<float>(y % 17) / 4;
      result[at + 2] = alpha * static_cast<float>((x + y) % 11) / 8;
      result[at + 3] = alpha;
    }
  }
  return result;
}

std::unique_ptr<Texture> Source(Device &device, std::span<const float> pixels) {
  const auto bytes = uint64_t{kWidth} * kHeight * 8;
  auto staging = Require(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*staging, {.Bytes = bytes}));
  CHECK(SDL_ConvertPixels(kWidth,
                          kHeight,
                          SDL_PIXELFORMAT_RGBA128_FLOAT,
                          pixels.data(),
                          kWidth * 16,
                          SDL_PIXELFORMAT_RGBA64_FLOAT,
                          mapped.data(),
                          kWidth * 8),
        "dyadic HDR samples encode exactly in the source half format");
  Require(device.Unmap(*staging));
  auto texture =
      Require(device.CreateTexture({.Width = kWidth,
                                    .Height = kHeight,
                                    .Texels = Format::Rgba16Float,
                                    .Use = TextureUse::Sampled | TextureUse::CopyDestination}));
  auto commands = Require(device.Begin());
  Require(commands->Copy(
      *staging,
      *texture,
      {.RowBytes = uint64_t{kWidth} * 8, .Region = {.Width = kWidth, .Height = kHeight}}));
  const auto submitted = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(submitted, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "fixture upload completes within its offline deadline");
  return texture;
}

void Compare(Device &device,
             const RadianceFilter &filter,
             const Texture &source,
             std::span<const float> pixels,
             uint32_t width,
             uint32_t height) {
  auto destination =
      Require(device.CreateTexture({.Width = width,
                                    .Height = height,
                                    .Texels = Format::Rgba16Float,
                                    .Use = TextureUse::Storage | TextureUse::CopySource}));
  auto commands = Require(device.Begin());
  Require(filter.Encode(*commands, source, *destination));
  const auto submitted = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(submitted, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "the GPU reduction completes within its offline deadline");
  auto rows = ReadImage(device, *destination, {.Width = width, .Height = height});
  std::vector<float> actual(size_t{width} * height * 4);
  CHECK(SDL_ConvertPixels(static_cast<int>(width),
                          static_cast<int>(height),
                          SDL_PIXELFORMAT_RGBA64_FLOAT,
                          rows.Rows.data(),
                          static_cast<int>(rows.RowBytes),
                          SDL_PIXELFORMAT_RGBA128_FLOAT,
                          actual.data(),
                          static_cast<int>(width * 16)),
        "the selected GPU footprint decodes without changing its HDR format");
  const uint32_t across = kWidth / width;
  const uint32_t down = kHeight / height;
  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      std::array<double, 4> sum{};
      for (uint32_t row = y * down; row < (y + 1) * down; ++row) {
        for (uint32_t column = x * across; column < (x + 1) * across; ++column) {
          const auto at = (size_t{row} * kWidth + column) * 4;
          for (size_t channel = 0; channel < 4; ++channel) { sum[channel] += pixels[at + channel]; }
        }
      }
      for (size_t channel = 0; channel < 4; ++channel) {
        const double expected = sum[channel] / (across * down);
        const float value = actual[(size_t{y} * width + x) * 4 + channel];
        const double ulp = expected == 0 ? std::ldexp(1.0, -24)
                                         : std::max(std::ldexp(1.0, std::ilogb(expected) - 10),
                                                    std::ldexp(1.0, -24));
        CHECK(std::isfinite(value) && std::abs(value - expected) <= ulp * .5,
              "premultiplied HDR and coverage round to the nearest half value of independent box "
              "integrals");
      }
    }
  }
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  const auto pixels = SourcePixels();
  const auto source = Source(*device, pixels);
  RadianceFilter filter;
  auto commands = Require(device->Begin());
  CHECK(!filter.Encode(*commands, *source, *source), "an unconfigured filter records no work");
  Require(filter.Configure(*device));
  Require(filter.Configure(*device));
  auto other = Require(CreateDevice());
  CHECK(!filter.Configure(*other), "another device cannot silently replace the filter owner");
  CHECK(!filter.Encode(*commands, *source, *source),
        "read/write aliasing is rejected before dispatch");
  const auto invalid = Require(device->CreateTexture(
      {.Width = 7, .Height = 3, .Texels = Format::Rgba16Float, .Use = TextureUse::Storage}));
  CHECK(!filter.Encode(*commands, *source, *invalid),
        "partial integer footprints need an explicit policy");
  const auto wrongFormat = Require(device->CreateTexture(
      {.Width = 32, .Height = 16, .Texels = Format::Rgba8Unorm, .Use = TextureUse::Storage}));
  CHECK(!filter.Encode(*commands, *source, *wrongFormat),
        "an integer display image cannot silently replace a linear HDR target");
  const auto unwritable = Require(device->CreateTexture(
      {.Width = 32, .Height = 16, .Texels = Format::Rgba16Float, .Use = TextureUse::Sampled}));
  CHECK(!filter.Encode(*commands, *source, *unwritable),
        "a sampled-only image cannot become an implicit storage output");
  commands.reset();
  for (const uint32_t width : {128u, 64u, 32u, 8u, 2u}) {
    Compare(*device, filter, *source, pixels, width, width / 2);
  }
  Compare(*device, filter, *source, pixels, 1, 1);
  return Report();
}
