#include "NativeGpuCheck.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <cstring>
#include <string_view>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

constexpr std::array<float, 4> kClear{.125f, .25f, .375f, .5f};

std::array<float, 4> Expected(float depth, std::size_t target, uint32_t x) {
  std::array pixel{depth, static_cast<float>(target) / 4, static_cast<float>(x) / 64, 1.f};
  if (target == 1) {
    pixel[1] = kClear[1];
    pixel[2] = kClear[2];
  }
  if (target == 2 || target == 3) { return kClear; }
  if (target == 4) {
    for (std::size_t channel = 0; channel < pixel.size(); ++channel) {
      pixel[channel] += kClear[channel];
    }
  }
  return pixel;
}

void Draw(Device &device, const char *vertexPath, const char *fragmentPath) {
  constexpr uint32_t width = 35;
  constexpr uint32_t height = 19;
  constexpr uint32_t rowBytes = 768;
  constexpr uint64_t imageBytes = uint64_t(rowBytes) * height;
  const auto vertex = Spirv(vertexPath);
  const auto fragment = Spirv(fragmentPath);
  std::array<ColourTarget, 5> formats{};
  std::array<ColourAttachment, 5> colours{};
  std::array<std::unique_ptr<Texture>, 5> images;
  for (std::size_t at = 0; at < images.size(); ++at) {
    formats[at].Texels = Format::Rgba32Float;
    images[at] = Require(
        device.CreateTexture({.Width = width,
                              .Height = height,
                              .Texels = Format::Rgba32Float,
                              .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
    colours[at] = {.Image = {.Image = images[at].get()}, .Begin = Load::Clear, .Clear = kClear};
  }
  formats[1].Write = ColourChannel::Red | ColourChannel::Alpha;
  formats[2].Blend = true;
  formats[2].Rgb = {.Source = BlendFactor::Zero, .Destination = BlendFactor::One};
  formats[2].Alpha = formats[2].Rgb;
  formats[3].Write = {};
  formats[4].Blend = true;
  formats[4].Rgb = {.Source = BlendFactor::One, .Destination = BlendFactor::One};
  formats[4].Alpha = formats[4].Rgb;
  auto depth =
      Require(device.CreateTexture({.Width = width,
                                    .Height = height,
                                    .Texels = Format::Depth32Float,
                                    .Use = TextureUse::DepthAttachment | TextureUse::CopySource}));
  auto pipeline = Require(
      device.CreateGraphicsPipeline({.Vertex = {.Spirv = vertex},
                                     .Fragment = ShaderCode{.Spirv = fragment},
                                     .Streams = {},
                                     .Attributes = {},
                                     .Colours = formats,
                                     .Depth = DepthTarget{.Compare = DepthCompare::GreaterEqual},
                                     .Culling = Cull::None,
                                     .Label = {}}));
  auto readback = Require(device.CreateBuffer(
      {.Bytes = imageBytes * 6, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  auto commands = Require(device.Begin());
  Require(commands->BeginRenderPass(
      {.Colours = colours,
       .Depth = DepthAttachment{.Image = {.Image = depth.get()}, .Begin = Load::Clear}}));
  Require(commands->Draw({.State = {.Pipeline = pipeline.get(),
                                    .Bindings = {},
                                    .Vertices = {},
                                    .Area = {.Width = width, .Height = height},
                                    .Clip = {.Width = width, .Height = height}},
                          .Range = {.Count = 3}}));
  pipeline.reset();
  Require(commands->EndRenderPass());
  for (std::size_t at = 0; at < images.size(); ++at) {
    Require(commands->Copy(*images[at],
                           *readback,
                           {.BufferOffset = at * imageBytes,
                            .RowBytes = rowBytes,
                            .Region = {.Width = width, .Height = height}}));
    images[at].reset();
  }
  Require(commands->Copy(*depth,
                         *readback,
                         {.BufferOffset = 5 * imageBytes,
                          .RowBytes = rowBytes,
                          .Region = {.Width = width, .Height = height}}));
  depth.reset();
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "five MRT draws retain pipeline and attachment payloads after outer owner release");
  const auto bytes = Require(device.MapRead(*readback, {.Bytes = imageBytes * 6}));
  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      const float expectedDepth = static_cast<float>((x * 7 + y * 13) % 256) / 256;
      for (std::size_t at = 0; at < images.size(); ++at) {
        std::array<float, 4> pixel{};
        std::memcpy(pixel.data(),
                    bytes.data() + at * imageBytes + std::size_t{y} * rowBytes + x * sizeof(pixel),
                    sizeof(pixel));
        CHECK(pixel == Expected(expectedDepth, at, x),
              "independent MRT write masks and RGB/alpha blends preserve each target's declared "
              "result");
      }
      float actualDepth = 0;
      std::memcpy(&actualDepth,
                  bytes.data() + 5 * imageBytes + std::size_t{y} * rowBytes + x * sizeof(float),
                  sizeof(float));
      CHECK(actualDepth == expectedDepth,
            "reverse-Z depth preserves each fragment's independent depth value");
    }
  }
  Require(device.Unmap(*readback));
}
}

int main(int argc, char *const *argv) {
  if ((argc != 3 && argc != 4) || (argc == 4 && std::string_view(argv[3]) != "--validation")) {
    return 1;
  }
  auto device = Require(CreateDevice({.Validation = argc == 4}));
  Draw(*device, argv[1], argv[2]);
  return Report();
}
