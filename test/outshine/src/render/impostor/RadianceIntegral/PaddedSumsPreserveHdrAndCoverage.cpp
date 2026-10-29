#include "RadianceIntegral.h"
#include "Check.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeImageReadback.h"
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>
#include <array>
#include <chrono>
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

struct Shape {
  uint32_t Width, Height;
};

std::vector<float> Pixels(Shape shape) {
  std::vector<float> pixels(size_t{shape.Width} * shape.Height * 4);
  for (uint32_t y = 0; y < shape.Height; ++y) {
    for (uint32_t x = 0; x < shape.Width; ++x) {
      const float alpha = static_cast<float>((x + 2 * y + 1) % 5) / 4;
      const size_t at = (size_t{y} * shape.Width + x) * 4;
      pixels[at] = alpha * 1024 * static_cast<float>(x % 7);
      pixels[at + 1] = alpha * static_cast<float>(y % 11) / 8;
      pixels[at + 2] = alpha * 2;
      pixels[at + 3] = alpha;
    }
  }
  return pixels;
}

std::unique_ptr<Texture>
Source(Device &device, Shape shape, Format format, std::span<const float> pixels) {
  const uint32_t pixelBytes = format == Format::Rgba16Float ? 8 : 16;
  const uint64_t bytes = uint64_t{shape.Width} * shape.Height * pixelBytes;
  auto upload = Require(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*upload, {.Bytes = bytes}));
  CHECK(SDL_ConvertPixels(shape.Width,
                          shape.Height,
                          SDL_PIXELFORMAT_RGBA128_FLOAT,
                          pixels.data(),
                          shape.Width * 16,
                          format == Format::Rgba16Float ? SDL_PIXELFORMAT_RGBA64_FLOAT
                                                        : SDL_PIXELFORMAT_RGBA128_FLOAT,
                          mapped.data(),
                          shape.Width * pixelBytes),
        "linear source has exact dyadic texels");
  Require(device.Unmap(*upload));
  auto source =
      Require(device.CreateTexture({.Width = shape.Width,
                                    .Height = shape.Height,
                                    .Texels = format,
                                    .Use = TextureUse::Sampled | TextureUse::CopyDestination}));
  auto commands = Require(device.Begin());
  Require(commands->Copy(*upload,
                         *source,
                         {.RowBytes = uint64_t{shape.Width} * pixelBytes,
                          .Region = {.Width = shape.Width, .Height = shape.Height}}));
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "offline input upload completes before reuse");
  return source;
}

std::array<double, 4> Integral(std::span<const float> pixels, Shape shape, Shape past) {
  std::array<double, 4> result{};
  for (uint32_t y = 0; y < past.Height; ++y) {
    for (uint32_t x = 0; x < past.Width; ++x) {
      for (size_t channel = 0; channel < 4; ++channel) {
        result[channel] += pixels[(size_t{y} * shape.Width + x) * 4 + channel];
      }
    }
  }
  return result;
}

void Exercise(Device &device, const RadianceIntegral &integral, Shape shape, Format format) {
  const auto pixels = Pixels(shape);
  const auto source = Source(device, shape, format, pixels);
  const auto description =
      TextureDescription{.Width = shape.Width + 1,
                         .Height = shape.Height + 1,
                         .Texels = Format::Rgba32Float,
                         .Use = TextureUse::Storage | TextureUse::Sampled | TextureUse::CopySource};
  auto scratch = Require(device.CreateTexture(description));
  auto target = Require(device.CreateTexture(description));
  auto commands = Require(device.Begin());
  CHECK(!integral.Encode(*commands, *source, *target, *target),
        "aliased passes are refused before recording");
  CHECK(!integral.Encode(*commands, *source, *scratch, *source),
        "an unpadded input cannot become its own prefix target");
  Require(integral.Encode(*commands, *source, *scratch, *target));
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "row and column passes complete in one accepted submission");
  const auto rows =
      ReadImage(device, *target, {.Width = shape.Width + 1, .Height = shape.Height + 1});
  for (uint32_t y = 0; y <= shape.Height; ++y) {
    for (uint32_t x = 0; x <= shape.Width; ++x) {
      std::array<float, 4> actual{};
      std::memcpy(
          actual.data(), rows.Rows.data() + y * rows.RowBytes + uint64_t{x} * 16, sizeof(actual));
      const auto expected = Integral(pixels, shape, {.Width = x, .Height = y});
      for (size_t channel = 0; channel < 4; ++channel) {
        CHECK(actual[channel] == expected[channel],
              "exclusive zero-padded Float32 prefix matches independent source enumeration");
      }
    }
  }
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  RadianceIntegral integral;
  Require(integral.Configure(*device));
  Require(integral.Configure(*device));
  auto other = Require(CreateDevice());
  CHECK(!integral.Configure(*other), "another device cannot replace the configured owner");
  for (const Shape shape : {Shape{.Width = 1, .Height = 1},
                            Shape{.Width = 13, .Height = 7},
                            Shape{.Width = 128, .Height = 64}}) {
    for (const Format format : {Format::Rgba16Float, Format::Rgba32Float}) {
      Exercise(*device, integral, shape, format);
    }
  }
  return Report();
}
