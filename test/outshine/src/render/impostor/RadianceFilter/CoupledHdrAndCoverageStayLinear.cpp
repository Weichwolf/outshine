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

std::vector<float> SourcePixels(uint32_t width = kWidth, uint32_t height = kHeight) {
  std::vector<float> result(size_t{width} * height * 4);
  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      const size_t at = (size_t{y} * width + x) * 4;
      const float alpha = static_cast<float>((x + y * 3) % 5) / 4;
      result[at] = alpha * static_cast<float>(x % 31) / 2;
      result[at + 1] = alpha * static_cast<float>(y % 17) / 4;
      result[at + 2] = alpha * static_cast<float>((x + y) % 11) / 8;
      result[at + 3] = alpha;
    }
  }
  return result;
}

std::unique_ptr<Texture> Source(Device &device,
                                std::span<const float> pixels,
                                uint32_t width = kWidth,
                                uint32_t height = kHeight) {
  const auto bytes = uint64_t{width} * height * 8;
  auto staging = Require(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*staging, {.Bytes = bytes}));
  CHECK(SDL_ConvertPixels(width,
                          height,
                          SDL_PIXELFORMAT_RGBA128_FLOAT,
                          pixels.data(),
                          width * 16,
                          SDL_PIXELFORMAT_RGBA64_FLOAT,
                          mapped.data(),
                          width * 8),
        "dyadic HDR samples encode exactly in the source half format");
  Require(device.Unmap(*staging));
  auto texture =
      Require(device.CreateTexture({.Width = width,
                                    .Height = height,
                                    .Texels = Format::Rgba16Float,
                                    .Use = TextureUse::Sampled | TextureUse::CopyDestination}));
  auto commands = Require(device.Begin());
  Require(commands->Copy(
      *staging,
      *texture,
      {.RowBytes = uint64_t{width} * 8, .Region = {.Width = width, .Height = height}}));
  const auto submitted = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(submitted, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "fixture upload completes within its offline deadline");
  return texture;
}

struct Pixel {
  uint32_t X, Y;
};

std::array<double, 4> AreaAverage(std::span<const float> pixels,
                                  const TextureDescription &from,
                                  const TextureDescription &to,
                                  Pixel pixel) {
  std::array<double, 4> sum{};
  for (uint32_t row = 0; row < from.Height; ++row) {
    const int64_t down =
        std::max(int64_t{0},
                 std::min(int64_t{row + 1} * to.Height, int64_t{pixel.Y + 1} * from.Height) -
                     std::max(int64_t{row} * to.Height, int64_t{pixel.Y} * from.Height));
    if (down == 0) { continue; }
    for (uint32_t column = 0; column < from.Width; ++column) {
      const int64_t across =
          std::max(int64_t{0},
                   std::min(int64_t{column + 1} * to.Width, int64_t{pixel.X + 1} * from.Width) -
                       std::max(int64_t{column} * to.Width, int64_t{pixel.X} * from.Width));
      const auto at = (size_t{row} * from.Width + column) * 4;
      for (size_t channel = 0; channel < 4; ++channel) {
        sum[channel] += pixels[at + channel] * static_cast<double>(across * down);
      }
    }
  }
  for (auto &value : sum) { value /= static_cast<double>(from.Width) * from.Height; }
  return sum;
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
  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      const auto average =
          AreaAverage(pixels, source.Description(), destination->Description(), {.X = x, .Y = y});
      for (size_t channel = 0; channel < 4; ++channel) {
        const double expected = average[channel];
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
      {.Width = 129, .Height = 3, .Texels = Format::Rgba16Float, .Use = TextureUse::Storage}));
  CHECK(!filter.Encode(*commands, *source, *invalid),
        "an enlarged axis is refused even when the other axis shrinks");
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
  for (const auto [width, height] : std::array<std::array<uint32_t, 2>, 6>{
           {{127, 63}, {7, 3}, {63, 1}, {1, 31}, {127, 1}, {17, 9}}}) {
    Compare(*device, filter, *source, pixels, width, height);
  }
  const auto oddPixels = SourcePixels(13, 11);
  const auto oddSource = Source(*device, oddPixels, 13, 11);
  for (const auto [width, height] :
       std::array<std::array<uint32_t, 2>, 4>{{{7, 3}, {2, 1}, {12, 10}, {1, 1}}}) {
    Compare(*device, filter, *oddSource, oddPixels, width, height);
  }
  return Report();
}
