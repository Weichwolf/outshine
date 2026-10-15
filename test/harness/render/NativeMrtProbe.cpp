#include "NativeGpuCheck.h"
#include "Check.h"
#include "Device.h"
#include <algorithm>
#include <array>
#include <string_view>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <utility>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

constexpr uint32_t kWidth = 8, kHeight = 4, kRowBytes = 256, kPyramidValues = 12;
constexpr uint64_t kImageBytes = uint64_t{kRowBytes} * kHeight;
constexpr uint64_t kBytes = 6 * kImageBytes + kPyramidValues * sizeof(float);

struct alignas(16) Reducing {
  std::array<uint32_t, 4> Source{kWidth, kHeight, 0, 0};
  std::array<uint32_t, 4> Wide{4, 2, 1, 1};
  std::array<uint32_t, 4> High{2, 1, 1, 1};
  std::array<uint32_t, 4> At{0, 8, 10, 11};
};

static_assert(sizeof(Reducing) == 64 && alignof(Reducing) == 16 && offsetof(Reducing, Wide) == 16 &&
              offsetof(Reducing, High) == 32 &&
              offsetof(Reducing, At) == 3 * sizeof(Reducing::Source));

float Depth(uint32_t x, uint32_t y) {
  constexpr uint32_t horizontal = 7;
  constexpr uint32_t vertical = 13;
  constexpr uint32_t period = 256;
  return static_cast<float>((x * horizontal + y * vertical) % period) / period;
}

void VerifyMrt(std::span<const std::byte> mapped) {
  for (uint32_t image = 0; image < 6; ++image) {
    for (uint32_t y = 0; y < kHeight; ++y) {
      for (uint32_t x = 0; x < kWidth; ++x) {
        const std::array<float, 4> expected{
            Depth(x, y), static_cast<float>(image) / 4, static_cast<float>(x) / 64, 1};
        const size_t channels = image == 5 ? 1 : 4;
        std::array<float, 4> actual{};
        std::memcpy(actual.data(),
                    mapped.data() + image * kImageBytes + size_t{y} * kRowBytes +
                        x * channels * sizeof(float),
                    channels * sizeof(float));
        for (size_t channel = 0; channel < channels; ++channel) {
          CHECK(actual[channel] == expected[channel],
                "common-API draw writes all five MRTs and reverse-Z depth");
        }
      }
    }
  }
}

void VerifyPyramid(std::span<const std::byte> mapped) {
  const Reducing reducing;
  for (uint32_t level = 0; level < 4; ++level) {
    const uint32_t side = 2u << level;
    for (uint32_t y = 0; y < reducing.High[level]; ++y) {
      for (uint32_t x = 0; x < reducing.Wide[level]; ++x) {
        float expected = 1;
        for (uint32_t dy = 0; dy < side; ++dy) {
          for (uint32_t dx = 0; dx < side; ++dx) {
            expected = std::min(
                expected,
                Depth(std::min(kWidth - 1, x * side + dx), std::min(kHeight - 1, y * side + dy)));
          }
        }
        float actual = 0;
        const auto index = reducing.At[level] + y * reducing.Wide[level] + x;
        std::memcpy(
            &actual, mapped.data() + 6 * kImageBytes + index * sizeof(float), sizeof(float));
        CHECK(actual == expected,
              "Hi-Z consumes the rendered depth and matches independent CPU reduction");
      }
    }
  }
}

void Render(Device &device,
            const char *vertexPath,
            const char *fragmentPath,
            const char *pyramidPath) {
  const auto vertex = Spirv(vertexPath);
  const auto fragment = Spirv(fragmentPath);
  std::array<ColourTarget, 5> targets{};
  std::array<std::unique_ptr<Texture>, 5> images;
  std::array<ColourAttachment, 5> colours;
  for (size_t slot = 0; slot < images.size(); ++slot) {
    targets[slot].Texels = Format::Rgba32Float;
    images[slot] = Require(
        device.CreateTexture({.Width = kWidth,
                              .Height = kHeight,
                              .Texels = Format::Rgba32Float,
                              .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
    colours[slot] = {.Image = {.Image = images[slot].get()}, .Begin = Load::Clear};
  }
  const auto depth = Require(device.CreateTexture(
      {.Width = kWidth,
       .Height = kHeight,
       .Texels = Format::Depth32Float,
       .Use = TextureUse::DepthAttachment | TextureUse::CopySource | TextureUse::Sampled}));
  const auto pipeline = Require(
      device.CreateGraphicsPipeline({.Vertex = {.Spirv = vertex},
                                     .Fragment = ShaderCode{.Spirv = fragment},
                                     .Streams = {},
                                     .Attributes = {},
                                     .Colours = targets,
                                     .Depth = DepthTarget{.Compare = DepthCompare::GreaterEqual},
                                     .Culling = Cull::None,
                                     .Label = "MRT/depth/Hi-Z"}));
  const auto pyramid = Require(device.CreateComputePipeline(Spirv(pyramidPath)));
  const auto reduced =
      Require(device.CreateBuffer({.Bytes = kPyramidValues * sizeof(float),
                                   .Use = BufferUse::Storage | BufferUse::CopySource}));
  const auto sampler = Require(device.CreateSampler(
      {.Min = Filter::Nearest, .Mag = Filter::Nearest, .Mip = Filter::Nearest}));
  const auto pixels = Require(device.CreateBuffer(
      {.Bytes = kBytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  auto commands = Require(device.Begin());
  Require(commands->BeginRenderPass(
      {.Colours = colours,
       .Depth = DepthAttachment{.Image = {.Image = depth.get()}, .Begin = Load::Clear}}));
  Require(commands->Draw({.State = {.Pipeline = pipeline.get(),
                                    .Bindings = {},
                                    .Vertices = {},
                                    .Area = {.Width = kWidth, .Height = kHeight},
                                    .Clip = {.Width = kWidth, .Height = kHeight}},
                          .Range = {.Count = 3}}));
  Require(commands->EndRenderPass());
  const Reducing reducing;
  const std::array bindings{
      Binding{.Set = 0,
              .Slot = 0,
              .Value = SampledTexture{.Image = {.Image = depth.get()}, .Sampling = sampler.get()}},
      Storage(1, 0, *reduced),
      Binding{.Set = 2, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(&reducing, 1))}}};
  Require(commands->Dispatch(*pyramid, bindings, {}));
  for (size_t slot = 0; slot < images.size(); ++slot) {
    Require(commands->Copy(*images[slot],
                           *pixels,
                           {.BufferOffset = slot * kImageBytes,
                            .RowBytes = kRowBytes,
                            .Region = {.Width = kWidth, .Height = kHeight}}));
  }
  Require(commands->Copy(*depth,
                         *pixels,
                         {.BufferOffset = 5 * kImageBytes,
                          .RowBytes = kRowBytes,
                          .Region = {.Width = kWidth, .Height = kHeight}}));
  Require(commands->Copy(
      *reduced,
      *pixels,
      {.DestinationOffset = 6 * kImageBytes, .Bytes = kPyramidValues * sizeof(float)}));
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "the coupled MRT/depth/compute/transfer path completes");
  const auto mapped = Require(device.MapRead(*pixels, {.Bytes = kBytes}));
  VerifyMrt(mapped);
  VerifyPyramid(mapped);
  Require(device.Unmap(*pixels));
}

}

int main(int argc, char *const *argv) {
  if ((argc != 4 && argc != 5) || (argc == 5 && std::string_view(argv[4]) != "--validation")) {
    return 1;
  }
  auto device = Require(CreateDevice({.Validation = argc == 5}));
  Render(*device, argv[1], argv[2], argv[3]);
  return Report();
}
